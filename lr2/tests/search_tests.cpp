#include "vfs/virtual_file_system.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include "core/array_sequence.h"

namespace fs = std::filesystem;


ArraySequence<std::string> paths(const DynamicArray<ShrdPtr<const RegularFile>>& files) {
    ArraySequence<std::string> result;
    for (int index = 0; index < files.get_size(); index++) {
        result.push_back(files.get(index)->get_path());
    }
    std::sort(result.begin(), result.end());
    return result;
}

void collect_directories(const VirtualFileSystem& vfs, const std::string& path, ArraySequence<std::string>& out) {
    out.push_back(path);
    DynamicArray<ShrdPtr<const File>> children = vfs.list(path);
    for (int index = 0; index < children.get_size(); index++) {
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

        ArraySequence<std::string> find(SearchField field, const std::string& value) {
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

    EXPECT_EQ(find(SearchField::Name, "report.txt"),
              (ArraySequence<std::string>{"/a/b/report.txt", "/report.txt"}));
    EXPECT_TRUE(find(SearchField::Name, "Report.txt").empty());
    EXPECT_TRUE(find(SearchField::Name, "missing").empty());
}

TEST_F(SearchTest, ByExtensionIgnoresCaseAndDot) {
    attach("/", "a.TXT");
    attach("/", "b.txt");
    attach("/", "c.log");
    for (const char* query : {"txt", ".txt", "TXT"}) {
        EXPECT_EQ(find(SearchField::Extension, query), (ArraySequence<std::string>{"/a.TXT", "/b.txt"}));
    }
}

TEST_F(SearchTest, EmptyExtensionFindsFilesWithoutIt) {
    attach("/", "b.txt");
    attach("/", "noext");
    attach("/", ".hidden");
    EXPECT_EQ(find(SearchField::Extension, ""), (ArraySequence<std::string>{"/.hidden", "/noext"}));
}

TEST_F(SearchTest, IndexSeesImportedFiles) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    EXPECT_EQ(find(SearchField::Extension, "log"), (ArraySequence<std::string>{"/data/inner/three.LOG"}));
}

TEST_F(SearchTest, IndexFollowsRename) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    vfs.rename("/data/two.txt", "renamed.log");
    EXPECT_TRUE(find(SearchField::Name, "two.txt").empty());
    EXPECT_EQ(find(SearchField::Extension, "log").size(), 2u);
}

TEST_F(SearchTest, IndexFollowsMove) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    vfs.make_directory("/moved");
    vfs.move("/data/inner", "/moved");
    EXPECT_EQ(find(SearchField::Name, "three.LOG"), (ArraySequence<std::string>{"/moved/inner/three.LOG"}));
}

TEST_F(SearchTest, IndexFollowsRemove) {
    vfs.import_directory((real_dir / "data").u8string(), "/");
    vfs.rename("/data/two.txt", "renamed.log");
    vfs.remove("/data/inner");
    vfs.remove("/data/renamed.log");
    EXPECT_TRUE(find(SearchField::Extension, "log").empty());
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

        ArraySequence<std::string> files = paths(vfs.find_files(SearchField::Extension, "txt", SearchMethod::FullScan));
        ArraySequence<std::string> logs = paths(vfs.find_files(SearchField::Extension, "log", SearchMethod::FullScan));
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

        for (const char* query : names) find(SearchField::Name, query);
        for (const char* query : {"txt", "log", ""}) find(SearchField::Extension, query);
    }
}
