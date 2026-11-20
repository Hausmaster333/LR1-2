#include "vfs/virtual_file_system.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "core/array_sequence.h"

namespace fs = std::filesystem;

// Каждый тест получает свою временную папку с настоящими файлами:
//   a.txt ("hello"), b.log, data/c.txt, data/inner/d.txt
class VfsTest : public ::testing::Test {
    protected:
        fs::path real_dir;
        VirtualFileSystem vfs;

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

        // Имена детей каталога в том порядке, в котором их отдаёт list()
        ArraySequence<std::string> names(const std::string& path) const {
            ArraySequence<std::string> result;
            DynamicArray<ShrdPtr<const FsNode>> children = vfs.list(path);
            for (int index = 0; index < children.get_size(); index++) {
                result.push_back(children.get(index)->get_name());
            }
            return result;
        }

        const File& file(const std::string& path) const {
            return static_cast<const File&>(vfs.get(path));
        }
};

TEST_F(VfsTest, EmptyRoot) {
    EXPECT_TRUE(vfs.exists("/"));
    EXPECT_TRUE(vfs.get("/").is_directory());
    EXPECT_EQ(vfs.get("/").get_path(), "/");
    EXPECT_TRUE(names("/").empty());
}

TEST_F(VfsTest, MakeDirectory) {
    vfs.make_directory("/docs");
    vfs.make_directory("/docs/2024/"); // '/' в конце допустим
    EXPECT_TRUE(vfs.get("/docs/2024").is_directory());
    EXPECT_EQ(vfs.get("/docs/2024").get_path(), "/docs/2024");
}

TEST_F(VfsTest, MakeDirectoryErrors) {
    vfs.make_directory("/docs");
    EXPECT_THROW(vfs.make_directory("/docs"), std::invalid_argument);      // Уже есть
    EXPECT_THROW(vfs.make_directory("/missing/x"), std::invalid_argument); // Нет родителя
    EXPECT_THROW(vfs.make_directory("docs2"), std::invalid_argument);      // Не абсолютный путь
    EXPECT_THROW(vfs.make_directory("/.."), std::invalid_argument);        // ".." - не имя
    EXPECT_THROW(vfs.make_directory("/"), std::invalid_argument);          // Пустое имя
}

TEST_F(VfsTest, AttachFileReadsRealContent) {
    vfs.attach_file(real("a.txt"), "/");
    const File& attached = file("/a.txt");
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
    EXPECT_THROW(vfs.attach_file(real("a.txt"), "/"), std::invalid_argument);    // Имя занято
    EXPECT_THROW(vfs.attach_file(real("nope.txt"), "/"), std::invalid_argument); // Нет такого файла
    EXPECT_THROW(vfs.attach_file(real("data"), "/"), std::invalid_argument);     // Папка, не файл
    EXPECT_THROW(vfs.attach_file(real("b.log"), "/a.txt"), std::invalid_argument); // Цель - не каталог
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

// /a, /a/b, /b: каждая операция ниже ошибочна, и после всех дерево такое же
TEST_F(VfsTest, InvalidMovesKeepState) {
    vfs.make_directory("/a");
    vfs.make_directory("/a/b");
    vfs.make_directory("/b");

    EXPECT_THROW(vfs.move("/a", "/a/b"), std::invalid_argument); // В собственного потомка
    EXPECT_THROW(vfs.move("/a", "/a"), std::invalid_argument);   // В самого себя
    EXPECT_THROW(vfs.move("/a/b", "/"), std::invalid_argument);  // "/b" уже есть
    EXPECT_THROW(vfs.rename("/a", "b"), std::invalid_argument);  // "/b" уже есть
    EXPECT_THROW(vfs.rename("/a", "x/y"), std::invalid_argument); // '/' в имени
    EXPECT_THROW(vfs.move("/", "/a"), std::invalid_argument);     // Корень не двигается
    EXPECT_THROW(vfs.rename("/", "root"), std::invalid_argument);

    EXPECT_EQ(names("/"), (ArraySequence<std::string>{"a", "b"}));
    EXPECT_EQ(names("/a"), (ArraySequence<std::string>{"b"}));
}

TEST_F(VfsTest, RemoveFileAndDirectoryRecursively) {
    vfs.import_directory(real("data"), "/");
    vfs.attach_file(real("a.txt"), "/");
    vfs.remove("/a.txt");
    vfs.remove("/data"); // Вместе с data/inner
    EXPECT_TRUE(names("/").empty());
}

TEST_F(VfsTest, RemoveErrors) {
    EXPECT_THROW(vfs.remove("/data"), std::invalid_argument); // Нет такого
    EXPECT_THROW(vfs.remove("/"), std::invalid_argument);
}

// Узлы из list() - владельцы: после удаления узел жив, но отцеплен (родитель - WeakPtr)
TEST_F(VfsTest, ListedNodeOutlivesRemoval) {
    vfs.import_directory(real("data"), "/");
    ShrdPtr<const FsNode> data = vfs.list("/").get(0);
    ShrdPtr<const FsNode> inner = static_cast<const Directory&>(*data).list().get(0);
    EXPECT_EQ(inner->get_path(), "/data/inner");
    EXPECT_TRUE(inner->get_parent() == data);

    vfs.remove("/data");
    EXPECT_FALSE(data->get_parent());
    EXPECT_EQ(inner->get_path(), "/inner"); // Цепочка родителей обрывается на удалённом каталоге
    EXPECT_EQ(static_cast<const Directory&>(*data).get_child_count(), 2);
    EXPECT_EQ(vfs.find_files(SearchField::Name, "d.txt", SearchMethod::Index).get_size(), 0);
}

// ФС виртуальная: ни одна операция не создаёт, не двигает и не удаляет настоящие файлы
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
    EXPECT_THROW(vfs.import_directory(real("data"), "/"), std::invalid_argument);  // Имя занято
    EXPECT_THROW(vfs.import_directory(real("a.txt"), "/"), std::invalid_argument); // Не папка
    EXPECT_EQ(names("/"), (ArraySequence<std::string>{"data"}));
}

TEST_F(VfsTest, FileDeletedOutsideIsUnavailable) {
    vfs.attach_file(real("b.log"), "/");
    fs::remove(real_dir / "b.log");
    EXPECT_EQ(file("/b.log").get_content().get_size(), -1);
    EXPECT_EQ(file("/b.log").get_content().read_beginning(10), "");
}

// Пути хранятся в UTF-8: на Windows без u8path кириллическое имя не открылось бы
TEST_F(VfsTest, CyrillicNames) {
    std::string name = "отчёт.txt";
    write(real_dir / fs::u8path(name), "текст");
    vfs.attach_file(real(name), "/");
    vfs.make_directory("/документы");
    vfs.move("/" + name, "/документы");
    EXPECT_EQ(file("/документы/" + name).get_content().read_beginning(100), "текст");
}

// Число подкаталогов хранится в каталоге, а не считается обходом: проверяем, что оно не врёт
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

    VirtualFileSystem vfs(reader);
    vfs.attach_file("note.txt", "/", "copy.txt");
    vfs.import_directory("docs", "/");
    EXPECT_TRUE(vfs.exists("/docs/sub/note.txt"));
    EXPECT_TRUE(vfs.exists("/copy.txt"));
    EXPECT_EQ(vfs.find_files(SearchField::Extension, "txt", SearchMethod::Index).get_size(), 2);
    EXPECT_THROW(vfs.attach_file("missing.txt", "/"), std::invalid_argument);
    EXPECT_FALSE(vfs.exists("/missing.txt"));
}
