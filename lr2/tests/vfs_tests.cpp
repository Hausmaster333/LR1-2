#include "vfs/search_criteria.h"
#include "vfs/command.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "core/array_sequence.h"

namespace fs = std::filesystem;

class VfsTest : public ::testing::Test {
    protected:
        fs::path real_dir;
        FileSystemAccess access{ShrdPtr<VirtualFileSystem>::make()};
        VirtualFileSystem& vfs = access.filesystem();

        void SetUp() override {
            const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
            real_dir = fs::temp_directory_path() / (std::string("vfs_tests_") + test->name());
            fs::remove_all(real_dir);
            fs::create_directories(real_dir / "data" / "inner");
            write(real_dir / "a.txt", "hello");
            write(real_dir / "b.log", "log");
            write(real_dir / "data" / "c.txt", "c");
            write(real_dir / "data" / "inner" / "d.txt", "d");
        }

        void TearDown() override { fs::remove_all(real_dir); }

        static void write(const fs::path& path, const std::string& text) {
            std::ofstream(path, std::ios::binary) << text;
        }

        std::string real(const std::string& name) const { return (real_dir / name).u8string(); }

        ArraySequence<std::string> names(const std::string& path) const {
            ArraySequence<std::string> result;
            DynamicArray<ShrdPtr<const File>> children = vfs.list(path);
            for (std::size_t index = 0; index < children.get_size(); index++) {
                result.push_back(children.get(index)->get_name());
            }
            return result;
        }

        const RegularFile& file(const std::string& path) const {
            return static_cast<const RegularFile&>(vfs.get(path));
        }
};

TEST_F(VfsTest, EmptyRoot) {
    EXPECT_TRUE(vfs.exists("/"));
    EXPECT_TRUE(vfs.get("/").is_directory());
    EXPECT_EQ(vfs.get("/").get_path(), "/");
    EXPECT_TRUE(names("/").empty());
}

TEST(FileSystemAccessTest, ClosingAClientReleasesOnlyItsDirectory) {
    auto vfs = ShrdPtr<VirtualFileSystem>::make();
    vfs->make_directory("/docs");
    vfs->make_directory("/notes");
    FileSystemAccess explorer(vfs);
    explorer.change_directory("/docs");
    {
        FileSystemAccess terminal(vfs);
        terminal.change_directory("/notes");
        EXPECT_THROW(vfs->remove("/notes"), std::invalid_argument);
    }
    vfs->remove("/notes");
    FileSystemAccess next_terminal(vfs);
    EXPECT_THROW(vfs->remove("/docs"), std::invalid_argument);
    explorer.change_directory("/");
    vfs->remove("/docs");
}

TEST_F(VfsTest, MakeDirectory) {
    vfs.make_directory("/docs");
    vfs.make_directory("/docs/2024/");
    EXPECT_TRUE(vfs.get("/docs/2024").is_directory());
    EXPECT_EQ(vfs.get("/docs/2024").get_path(), "/docs/2024");
}

TEST_F(VfsTest, MakeDirectoryErrors) {
    vfs.make_directory("/docs");
    EXPECT_THROW(vfs.make_directory("/docs"), std::invalid_argument);
    EXPECT_THROW(vfs.make_directory("/missing/x"), std::invalid_argument);
    EXPECT_NO_THROW(access.make_directory("docs2"));
    EXPECT_THROW(vfs.make_directory("/.."), std::invalid_argument);
    EXPECT_THROW(vfs.make_directory("/"), std::invalid_argument);
}

TEST_F(VfsTest, AttachFileReadsRealContent) {
    vfs.attach_file(real("a.txt"), "/");
    const RegularFile& attached = file("/a.txt");
    EXPECT_FALSE(attached.is_directory());
    EXPECT_EQ(attached.get_content().get_size(), 5);
    EXPECT_EQ(attached.get_content().read_beginning(3), "hel");
    EXPECT_EQ(attached.get_content().read_beginning(100), "hello");
    EXPECT_TRUE(fs::equivalent(fs::u8path(attached.get_content().get_location()), real_dir / "a.txt"));
}

TEST_F(VfsTest, OneRealFileUnderTwoNames) {
    vfs.attach_file(real("a.txt"), "/");
    vfs.attach_file(real("a.txt"), "/", "copy.txt");
    EXPECT_TRUE(vfs.exists("/a.txt"));
    EXPECT_TRUE(vfs.exists("/copy.txt"));
}

TEST_F(VfsTest, AttachFileErrors) {
    vfs.attach_file(real("a.txt"), "/");
    EXPECT_THROW(vfs.attach_file(real("a.txt"), "/"), std::invalid_argument);
    EXPECT_THROW(vfs.attach_file(real("nope.txt"), "/"), std::invalid_argument);
    EXPECT_THROW(vfs.attach_file(real("data"), "/"), std::invalid_argument);
    EXPECT_THROW(vfs.attach_file(real("b.log"), "/a.txt"), std::invalid_argument);
}

TEST_F(VfsTest, ListsDirectoriesFirstThenAlphabetically) {
    vfs.attach_file(real("b.log"), "/");
    vfs.make_directory("/zeta");
    vfs.attach_file(real("a.txt"), "/");
    vfs.make_directory("/alpha");
    EXPECT_EQ(names("/"), (ArraySequence<std::string>{"alpha", "zeta", "a.txt", "b.log"}));
    vfs.attach_file(real("data/c.txt"), "/");
    vfs.remove("/b.log");
    EXPECT_EQ(names("/"), (ArraySequence<std::string>{"alpha", "zeta", "a.txt", "c.txt"}));
}

TEST_F(VfsTest, Rename) {
    vfs.attach_file(real("a.txt"), "/");
    vfs.rename("/a.txt", "note.txt");
    EXPECT_TRUE(vfs.exists("/note.txt"));
    EXPECT_FALSE(vfs.exists("/a.txt"));
}

TEST_F(VfsTest, MoveTakesDirectoryContents) {
    vfs.make_directory("/docs");
    vfs.make_directory("/docs/sub");
    vfs.attach_file(real("a.txt"), "/docs/sub");

    vfs.move("/docs/sub", "/");
    EXPECT_EQ(vfs.get("/sub/a.txt").get_path(), "/sub/a.txt");
    EXPECT_TRUE(names("/docs").empty());
}

TEST_F(VfsTest, InvalidMovesKeepState) {
    vfs.make_directory("/a");
    vfs.make_directory("/a/b");
    vfs.make_directory("/b");

    EXPECT_THROW(vfs.move("/a", "/a/b"), std::invalid_argument);
    EXPECT_THROW(vfs.move("/a", "/a"), std::invalid_argument);
    EXPECT_THROW(vfs.move("/a/b", "/"), std::invalid_argument);
    EXPECT_THROW(vfs.rename("/a", "b"), std::invalid_argument);
    EXPECT_THROW(vfs.rename("/a", "x/y"), std::invalid_argument);
    EXPECT_THROW(vfs.move("/", "/a"), std::invalid_argument);
    EXPECT_THROW(vfs.rename("/", "root"), std::invalid_argument);

    EXPECT_EQ(names("/"), (ArraySequence<std::string>{"a", "b"}));
    EXPECT_EQ(names("/a"), (ArraySequence<std::string>{"b"}));
}

TEST_F(VfsTest, RemoveFileAndDirectoryRecursively) {
    vfs.import_directory(real("data"), "/");
    vfs.attach_file(real("a.txt"), "/");
    vfs.remove("/a.txt");
    vfs.remove("/data");
    EXPECT_TRUE(names("/").empty());
}

TEST_F(VfsTest, RemoveErrors) {
    EXPECT_THROW(vfs.remove("/data"), std::invalid_argument);
    EXPECT_THROW(vfs.remove("/"), std::invalid_argument);
}

TEST_F(VfsTest, ListedNodeOutlivesRemoval) {
    vfs.import_directory(real("data"), "/");
    ShrdPtr<const File> data = vfs.list("/").get(0);
    ShrdPtr<const File> inner = static_cast<const Directory&>(*data).list().get(0);
    EXPECT_EQ(inner->get_path(), "/data/inner");
    EXPECT_TRUE(inner->get_parent() == data);

    vfs.remove("/data");
    EXPECT_FALSE(data->get_parent());
    EXPECT_EQ(inner->get_path(), "/inner");
    EXPECT_EQ(static_cast<const Directory&>(*data).get_child_count(), 2);
    EXPECT_EQ(vfs.find_files(by_name, std::string{"d.txt"}, SearchMethod::Index).get_size(), 0u);
}

TEST_F(VfsTest, RealFilesAreNeverTouched) {
    vfs.import_directory(real("data"), "/");
    vfs.attach_file(real("a.txt"), "/data");
    vfs.rename("/data/a.txt", "renamed.txt");
    vfs.make_directory("/other");
    vfs.move("/data/inner", "/other");
    vfs.remove("/data");
    vfs.remove("/other");

    for (const char* name : {"a.txt", "b.log", "data/c.txt", "data/inner/d.txt"}) {
        EXPECT_TRUE(fs::exists(real_dir / fs::u8path(name))) << name;
    }
    EXPECT_EQ(fs::file_size(real_dir / "a.txt"), 5u);
}

TEST_F(VfsTest, ImportDirectory) {
    vfs.import_directory(real("data"), "/");
    EXPECT_EQ(names("/data"), (ArraySequence<std::string>{"inner", "c.txt"}));
    EXPECT_EQ(names("/data/inner"), (ArraySequence<std::string>{"d.txt"}));
}

TEST_F(VfsTest, ImportDirectoryErrors) {
    vfs.import_directory(real("data"), "/");
    EXPECT_THROW(vfs.import_directory(real("data"), "/"), std::invalid_argument);
    EXPECT_THROW(vfs.import_directory(real("a.txt"), "/"), std::invalid_argument);
    EXPECT_EQ(names("/"), (ArraySequence<std::string>{"data"}));
}

TEST_F(VfsTest, FileDeletedOutsideIsUnavailable) {
    vfs.attach_file(real("b.log"), "/");
    fs::remove(real_dir / "b.log");
    EXPECT_EQ(file("/b.log").get_content().get_size(), -1);
    EXPECT_EQ(file("/b.log").get_content().read_beginning(10), "");
}

TEST_F(VfsTest, CyrillicNames) {
    std::string name = "отчёт.txt";
    write(real_dir / fs::u8path(name), "текст");
    vfs.attach_file(real(name), "/");
    vfs.make_directory("/документы");
    vfs.move("/" + name, "/документы");
    EXPECT_EQ(file("/документы/" + name).get_content().read_beginning(100), "текст");
}

TEST_F(VfsTest, SubdirectoryFlagFollowsOperations) {
    const Directory& root = vfs.get_root();
    vfs.add_file("/", "generated.txt", UnqPtr<GeneratedSource>::make());
    EXPECT_FALSE(root.has_subdirectories()); // Файлы не в счёт

    vfs.make_directory("/a");
    vfs.make_directory("/b");
    vfs.rename("/a", "c"); // Переименование не меняет число подкаталогов
    vfs.move("/b", "/c");
    EXPECT_TRUE(root.has_subdirectories());
    EXPECT_TRUE(static_cast<const Directory&>(vfs.get("/c")).has_subdirectories());

    vfs.remove("/c");
    EXPECT_FALSE(root.has_subdirectories());
    EXPECT_EQ(names("/"), (ArraySequence<std::string>{"generated.txt"}));
}

TEST(VirtualDisk, AttachAndImportWithoutRealFiles) {
    class Reader : public IFileSystemReader {
        public:
            UnqPtr<IContentSource> open_file(const fs::path& path) const override {
                if (path.filename() != "note.txt") throw std::invalid_argument("missing file");
                return UnqPtr<GeneratedSource>::make();
            }
            ArraySequence<DiskEntry> list_directory(const fs::path& path) const override {
                if (path == "docs") return {{path / "sub", true}};
                if (path == fs::path("docs") / "sub") return {{path / "note.txt", false}};
                throw std::invalid_argument("missing directory");
            }
    } reader;

    VirtualFileSystem vfs;
    vfs.attach_file("note.txt", "/", "copy.txt", reader);
    vfs.import_directory("docs", "/", reader);
    EXPECT_TRUE(vfs.exists("/docs/sub/note.txt"));
    EXPECT_TRUE(vfs.exists("/copy.txt"));
    EXPECT_EQ(vfs.find_files(by_extension, std::string{"txt"}, SearchMethod::Index).get_size(), 2u);
    EXPECT_THROW(vfs.attach_file("missing.txt", "/", "", reader), std::invalid_argument);
    EXPECT_FALSE(vfs.exists("/missing.txt"));
}

TEST_F(VfsTest, TerminalAndEditorShareFileAndIndex) {
    ASSERT_TRUE(execute_command(access, "mkdir docs").changed);
    ASSERT_TRUE(execute_command(access, "cd docs").navigated);
    const std::string path = "/docs/план защиты.txt";
    ASSERT_TRUE(execute_command(access, "touch \"план защиты.txt\"").changed);
    auto file = vfs.regular_file(path);
    EXPECT_TRUE(vfs.read_text(path).empty());
    std::string text(4800, 'a');
    text += "\nплан защиты\nвторая строка";
    vfs.save_text(path, file->get_id(), text);
    EXPECT_FALSE(execute_command(access, "touch \"план защиты.txt\"").changed);
    EXPECT_EQ(execute_command(access, "cat \"план защиты.txt\"").output, text);
    access.make_directory("txt");
    EXPECT_EQ(execute_command(access, "ls").output, "txt/\nплан защиты.txt\n");
    EXPECT_EQ(vfs.find_files(by_extension, std::string{"txt"}, SearchMethod::Index).get_size(), 1u);
    ASSERT_TRUE(execute_command(access, "rm \"план защиты.txt\"").changed);
    EXPECT_EQ(vfs.find_files(by_extension, std::string{"txt"}, SearchMethod::Index).get_size(), 0u);
    ASSERT_TRUE(vfs.touch(path));
    EXPECT_THROW(vfs.save_text(path, file->get_id(), text), std::invalid_argument);
    EXPECT_TRUE(vfs.read_text(path).empty());
}

TEST_F(VfsTest, NavigationFollowsMovedDirectory) {
    access.make_directory("a");
    access.make_directory("a/b");
    access.change_directory("a/./b");
    for (auto cmd : {"rm .", "rm ..", "rm /", "cd missing", "cd missing/.."}) {
        EXPECT_TRUE(execute_command(access, cmd).failed) << cmd;
        EXPECT_EQ(access.current_path(), "/a/b");
    }
    vfs.rename("/a", "new");
    vfs.make_directory("/dest");
    vfs.move("/new", "/dest");
    EXPECT_EQ(access.current_path(), "/dest/new/b");
    EXPECT_TRUE(execute_command(access, "touch ./relative").changed);
    EXPECT_TRUE(vfs.exists("/dest/new/b/relative"));
    access.change_directory("../../../..");
    EXPECT_EQ(access.current_path(), "/");
}

TEST_F(VfsTest, CommandErrorsAndReadOnlySources) {
    vfs.attach_file(real("a.txt"), "/");
    EXPECT_EQ(access.read_text("a.txt"), "hello");
    EXPECT_THROW(vfs.save_text("/a.txt", file("/a.txt").get_id(), "x"), std::invalid_argument);
    vfs.add_file("/", "empty", UnqPtr<GeneratedSource>::make());
    EXPECT_TRUE(access.read_text("empty").empty());
    fs::remove(real_dir / "a.txt");
    EXPECT_TRUE(execute_command(access, "cat a.txt").failed);
    for (auto cmd : {"cat", "cat /", "cat missing", "cd empty", "touch /", "rm /", "mkdir /",
                     "ls empty", "ls a b", "exit arg", "bad", "touch \"unfinished"}) {
        EXPECT_TRUE(execute_command(access, cmd).failed) << cmd;
    }
    EXPECT_TRUE(execute_command(access, "exit").exit);
}

TEST_F(VfsTest, DirectoryLinkNavigationAndRemoval) {
    vfs.make_directory("/учёба");
    vfs.make_directory("/учёба/лабы");
    ASSERT_TRUE(execute_command(access, "ln -s /учёба/лабы \"мои лабы\"").changed);
    EXPECT_NE(execute_command(access, "ls").output.find("мои лабы → /учёба/лабы"), std::string::npos);
    EXPECT_TRUE(vfs.touch("/мои лабы/работа.txt"));
    auto file = vfs.regular_file("/учёба/лабы/работа.txt");
    vfs.save_text("/мои лабы/работа.txt", file->get_id(), "готово");
    EXPECT_EQ(vfs.read_text("/мои лабы/работа.txt"), "готово");
    vfs.make_link("/", "/учёба/лабы/назад");
    for (auto method : {SearchMethod::Index, SearchMethod::FullScan})
        EXPECT_EQ(vfs.find_files(by_extension, std::string{"txt"}, method).get_size(), 1u);
    access.change_directory("/мои лабы/назад/мои лабы");
    EXPECT_EQ(access.current_path(), "/учёба/лабы");
    access.change_directory("..");
    EXPECT_EQ(access.current_path(), "/учёба");
    vfs.rename("/мои лабы", "ярлык");
    vfs.move("/ярлык", "/учёба");
    access.remove("ярлык/");
    EXPECT_EQ(access.read_text("лабы/работа.txt"), "готово");
}

TEST_F(VfsTest, LinksFollowMovedTargetAndBreakAfterRemoval) {
    vfs.make_directory("/a");
    vfs.make_link("/a", "/b");
    vfs.make_link("/b", "/c");
    EXPECT_THROW(vfs.make_link("/a", "/b"), std::invalid_argument);
    vfs.make_directory("/folder");
    vfs.move("/a", "/folder");
    access.change_directory("/c");
    EXPECT_EQ(access.current_path(), "/folder/a");
    access.change_directory("/");
    vfs.remove("/folder/a");
    vfs.make_directory("/a");
    EXPECT_THROW(access.change_directory("/b"), std::invalid_argument);
    EXPECT_THROW(access.change_directory("/c"), std::invalid_argument);
    EXPECT_EQ(access.current_path(), "/");
    vfs.remove("/b");
    vfs.remove("/c");
    EXPECT_TRUE(vfs.exists("/a"));
}
