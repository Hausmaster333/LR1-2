#include "vfs/search_criteria.h"
#include "vfs/virtual_file_system.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include "core/array_sequence.h"

namespace fs = std::filesystem;

ArraySequence<std::string> paths(const DynamicArray<ShrdPtr<const File>>& files) {
    ArraySequence<std::string> result;
    for (std::size_t index = 0; index < files.get_size(); index++) {
        result.push_back(files.get(index)->get_path());
    }
    std::sort(result.begin(), result.end());
    return result;
}

void collect_directories(const VirtualFileSystem& vfs, const std::string& path, ArraySequence<std::string>& out) {
    out.push_back(path);
    DynamicArray<ShrdPtr<const File>> children = vfs.list(path);
    for (std::size_t index = 0; index < children.get_size(); index++) {
        if (children.get(index)->is_directory()) {
            collect_directories(vfs, children.get(index)->get_path(), out);
        }
    }
}

class SearchTest : public ::testing::Test {
    protected:
        fs::path real_dir;
        VirtualFileSystem vfs;

        void SetUp() override {
            const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
            real_dir = fs::temp_directory_path() / (std::string("search_tests_") + test->name());
            fs::remove_all(real_dir);
            fs::create_directories(real_dir / "data" / "inner");
            for (const char* name : {"one.txt", "data/two.txt", "data/inner/three.LOG"}) {
                std::ofstream(real_dir / name) << name;
            }
        }

        void TearDown() override { fs::remove_all(real_dir); }

        void attach(const std::string& directory, const std::string& name) {
            vfs.attach_file((real_dir / "one.txt").u8string(), directory, name);
        }

        ArraySequence<std::string> find(SearchCriterion<ShrdPtr<const File>, std::string> field, const std::string& value) {
            ArraySequence<std::string> by_scan = paths(vfs.find_files(field, value, SearchMethod::FullScan));
            ArraySequence<std::string> by_index = paths(vfs.find_files(field, value, SearchMethod::Index));
            EXPECT_EQ(by_scan, by_index) << value;
            return by_index;
        }
};

TEST_F(SearchTest, ByNameFindsFilesInAllDirectories) {
    vfs.make_directory("/a");
    vfs.make_directory("/a/b");
    attach("/", "report.txt");
    attach("/a/b", "report.txt");
    attach("/a", "other.txt");

    EXPECT_EQ(find(by_name, "report.txt"),
              (ArraySequence<std::string>{"/a/b/report.txt", "/report.txt"}));
    EXPECT_TRUE(find(by_name, "Report.txt").empty());
    EXPECT_TRUE(find(by_name, "missing").empty());
}

TEST_F(SearchTest, ByExtensionIgnoresCaseAndDot) {
    attach("/", "a.TXT");
    attach("/", "b.txt");
    attach("/", "c.log");
    for (const char* query : {"txt", ".txt", "TXT"}) {
        EXPECT_EQ(find(by_extension, normalize_extension(query)), (ArraySequence<std::string>{"/a.TXT", "/b.txt"}));
    }
}

TEST_F(SearchTest, EmptyExtensionFindsFilesWithoutIt) {
    attach("/", "b.txt");
    attach("/", "noext");
    attach("/", ".hidden");
    EXPECT_EQ(find(by_extension, ""), (ArraySequence<std::string>{"/.hidden", "/noext"}));
}

TEST_F(SearchTest, IndexSeesImportedFiles) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    EXPECT_EQ(find(by_extension, "log"), (ArraySequence<std::string>{"/data/inner/three.LOG"}));
}

TEST_F(SearchTest, IndexFollowsRename) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    vfs.rename("/data/two.txt", "renamed.log");
    EXPECT_TRUE(find(by_name, "two.txt").empty());
    EXPECT_EQ(find(by_extension, "log").size(), 2u);
}

TEST_F(SearchTest, IndexFollowsMove) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    vfs.make_directory("/moved");
    vfs.move("/data/inner", "/moved");
    EXPECT_EQ(find(by_name, "three.LOG"), (ArraySequence<std::string>{"/moved/inner/three.LOG"}));
}

TEST_F(SearchTest, IndexFollowsRemove) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    vfs.rename("/data/two.txt", "renamed.log");
    vfs.remove("/data/inner");
    vfs.remove("/data/renamed.log");
    EXPECT_TRUE(find(by_extension, "log").empty());
}

TEST_F(SearchTest, IndexMatchesFullScanAfterRandomOperations) {
    const char* names[] = {"a.txt", "b.TXT", "c.log", "noext", "a.log"};
    std::mt19937 random(11);
    auto pick = [&random](int size) { return static_cast<int>(random() % size); };

    for (int step = 0; step < 400; step++) {
        ArraySequence<std::string> directories;
        collect_directories(vfs, "/", directories);
        std::string directory = directories[pick(directories.size())];
        std::string name = names[pick(5)];

        ArraySequence<std::string> files = paths(vfs.find_files(by_extension, std::string{"txt"}, SearchMethod::FullScan));
        ArraySequence<std::string> logs = paths(vfs.find_files(by_extension, std::string{"log"}, SearchMethod::FullScan));
        for (const auto& path : logs) files.push_back(path);

        try {
            switch (pick(6)) {
                case 0: vfs.make_directory(directory + "/d" + std::to_string(step)); break;
                case 1: case 2: attach(directory, name); break;
                case 3: if (!files.empty()) vfs.rename(files[pick(files.size())], name); break;
                case 4: if (!files.empty()) vfs.move(files[pick(files.size())], directory); break;
                case 5: if (directories.size() > 1) vfs.remove(directories[1 + pick(directories.size() - 1)]); break;
            }
        } catch (const std::invalid_argument&) {}

        for (const char* query : names) find(by_name, query);
        for (const char* query : {"txt", "log", ""}) find(by_extension, query);
    }
}

TEST(SearchIndex, BuildsOnFirstRequestAndReusesCustomCriterion) {
    static int calls;
    calls = 0;
    SearchCriterion<ShrdPtr<const File>, std::string> by_path{[](const ShrdPtr<const File>& file) -> std::optional<std::string> {
        ++calls;
        return file->get_path();
    }};
    VirtualFileSystem vfs;
    vfs.touch("/first");
    EXPECT_EQ(vfs.find_files(by_path, std::string{"/first"}, SearchMethod::FullScan).get_size(), 1u);
    calls = 0;
    vfs.touch("/second");
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(vfs.find_files(by_path, std::string{"/second"}, SearchMethod::Index).get_size(), 1u);
    EXPECT_EQ(calls, 2);
    calls = 0;
    EXPECT_EQ(vfs.find_files(by_path, std::string{"/first"}, SearchMethod::Index).get_size(), 1u);
    EXPECT_EQ(calls, 0);
    vfs.rename("/second", "renamed");
    EXPECT_EQ(vfs.find_files(by_path, std::string{"/second"}, SearchMethod::Index).get_size(), 0u);
    EXPECT_EQ(vfs.find_files(by_path, std::string{"/renamed"}, SearchMethod::Index).get_size(), 1u);
    calls = 0;
    vfs.build_index(by_path);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(vfs.find_files(by_path, std::string{"/renamed"}, SearchMethod::Index).get_size(), 1u);
    EXPECT_EQ(calls, 2);
}

TEST(SearchIndex, StringAndNumericIndexesFollowParentChanges) {
    VirtualFileSystem vfs;
    vfs.make_directory("/docs");
    vfs.make_directory("/archive");
    vfs.touch("/docs/report.PDF");
    const auto docs = vfs.get("/docs").get_id();
    const auto archive = vfs.get("/archive").get_id();
    vfs.build_index(by_parent);
    vfs.build_index(by_name);
    EXPECT_EQ(vfs.find_files(by_parent, docs, SearchMethod::Index).get_size(), 1u);
    vfs.move("/docs/report.PDF", "/archive");
    for (auto method : {SearchMethod::Index, SearchMethod::FullScan}) {
        EXPECT_EQ(vfs.find_files(by_parent, docs, method).get_size(), 0u);
        EXPECT_EQ(paths(vfs.find_files(by_parent, archive, method)),
                  (ArraySequence<std::string>{"/archive/report.PDF"}));
        EXPECT_EQ(vfs.find_files(by_name, std::string{"report.PDF"}, method).get_size(), 1u);
    }
}

TEST(SearchIndex, CriteriaShareOneFileSnapshotUntilMutation) {
    VirtualFileSystem vfs;
    vfs.touch("/note.txt");
    auto file = vfs.regular_file("/note.txt");
    const auto owners = file.use_count();
    vfs.build_index(by_name);
    EXPECT_EQ(file.use_count(), owners + 1);
    vfs.build_index(by_parent);
    vfs.build_index(by_extension);
    EXPECT_EQ(file.use_count(), owners + 1);
    vfs.touch("/a.txt");
    EXPECT_EQ(file.use_count(), owners);
    auto found = vfs.find_files(by_name, std::string{"note.txt"}, SearchMethod::Index);
    ASSERT_EQ(found.get_size(), 1u);
    EXPECT_EQ(found.get(0)->get_id(), file->get_id());
}

TEST(SearchIndex, RebuildsKeysDependingOnSiblings) {
    SearchCriterion<ShrdPtr<const File>, int> by_siblings{[](const ShrdPtr<const File>& file) -> std::optional<int> {
        return file->get_parent()->get_child_count();
    }};
    VirtualFileSystem vfs;
    vfs.touch("/a");
    EXPECT_EQ(vfs.find_files(by_siblings, 1, SearchMethod::Index).get_size(), 1u);
    vfs.touch("/b");
    EXPECT_EQ(vfs.find_files(by_siblings, 1, SearchMethod::Index).get_size(), 0u);
    EXPECT_EQ(paths(vfs.find_files(by_siblings, 2, SearchMethod::Index)),
              paths(vfs.find_files(by_siblings, 2, SearchMethod::FullScan)));
}

TEST(SearchIndex, ThrowingExtractorCannotInterruptMutationOrPublishPartialIndex) {
    SearchCriterion<ShrdPtr<const File>, std::string> checked_name{[](const ShrdPtr<const File>& file) -> std::optional<std::string> {
        if (file->get_name() == "bad") throw std::invalid_argument("bad key");
        return file->get_name();
    }};
    VirtualFileSystem vfs;
    vfs.touch("/a");
    vfs.build_index(checked_name);
    EXPECT_TRUE(vfs.touch("/bad"));
    EXPECT_THROW(vfs.find_files(checked_name, std::string{"a"}, SearchMethod::Index), std::invalid_argument);
    EXPECT_EQ(vfs.find_files(by_name, std::string{"bad"}, SearchMethod::Index).get_size(), 1u);
    vfs.remove("/bad");
    EXPECT_EQ(vfs.find_files(checked_name, std::string{"a"}, SearchMethod::Index).get_size(), 1u);
}

TEST(SearchIndex, SavingTextInvalidatesContentKeys) {
    SearchCriterion<ShrdPtr<const File>, long long> by_size{[](const ShrdPtr<const File>& file) -> std::optional<long long> {
        if (file->kind() != FileKind::Regular) return std::nullopt;
        return static_cast<const RegularFile&>(*file).get_content().get_size();
    }};
    VirtualFileSystem vfs;
    vfs.touch("/note");
    ASSERT_EQ(vfs.find_files(by_size, 0LL, SearchMethod::Index).get_size(), 1u);
    vfs.save_text("/note", vfs.get("/note").get_id(), "hello");
    EXPECT_EQ(vfs.find_files(by_size, 0LL, SearchMethod::Index).get_size(), 0u);
    EXPECT_EQ(vfs.find_files(by_size, 5LL, SearchMethod::Index).get_size(), 1u);
}
