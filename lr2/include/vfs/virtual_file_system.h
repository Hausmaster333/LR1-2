#ifndef VIRTUAL_FILE_SYSTEM_H
#define VIRTUAL_FILE_SYSTEM_H

#include "tree/tree_set.h"
#include "vfs/file_index.h"
#include "vfs/file_system_reader.h"
#include "vfs/file.h"
#include <string>

class VirtualFileSystem {
    private:
        ShrdPtr<Directory> root;
        ShrdPtr<Directory> current;
        FileIndex index;

        IFileSystemReader& reader;
        // Собирает виртуальное поддерево по содержимому папки на диске.
        ShrdPtr<Directory> build_directory(const std::filesystem::path& path) const;

        // Возвращает пустой указатель, если компонент пути отсутствует.
        ShrdPtr<File> find(const std::string& path, bool follow_last = true) const;
        // Разбирает путь и обнаруживает циклы при раскрытии ссылок.
        ShrdPtr<File> resolve(const std::string& path, bool follow_last,
                              TreeSet<std::string>& resolving) const;
        // Возвращает объект по пути или выдаёт ошибку.
        ShrdPtr<File> node_at(const std::string& path, bool follow_last = true) const;
        // Раскрывает ссылки и проверяет, что путь ведёт в каталог.
        ShrdPtr<Directory> directory_at(const std::string& path) const;
        // Возвращает саму запись для изменения. Для корня выдаёт ошибку.
        ShrdPtr<File> movable_node_at(const std::string& path) const;
    public:
        // Создаёт пустую ВФС со стандартным читателем диска.
        VirtualFileSystem();
        // Использует переданный читатель и делает корень текущим каталогом.
        explicit VirtualFileSystem(IFileSystemReader& reader);
        // Копирование и присваивание ВФС запрещены.
        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

        // Проверяет наличие записи, включая битую ссылку.
        bool exists(const std::string& path) const;
        // Возвращает саму запись, даже если это ссылка.
        const File& get(const std::string& path) const;
        // Возвращает корневой каталог для чтения.
        const Directory& get_root() const { return *root; }
        // Перечисляет детей по имени внутри групп, каталоги идут первыми.
        DynamicArray<ShrdPtr<const File>> list(const std::string& directory_path) const;

        // Вычисляет полный путь текущего каталога по его родителям.
        std::string current_path() const { return current->get_path(); }
        // Делает найденный каталог текущим.
        void change_directory(const std::string& path);
        // Создаёт ссылку на существующий каталог по абсолютному пути target.
        void make_link(const std::string& target, const std::string& path);
        // Создаёт пустой текстовый файл. Для существующего файла возвращает false.
        bool touch(const std::string& path);
        // Возвращает обычный файл для чтения. Для каталога выдаёт ошибку.
        ShrdPtr<const RegularFile> regular_file(const std::string& path) const;
        // Читает всё содержимое файла.
        std::string read_text(const std::string& path) const;
        // Сохраняет текст после проверки ID открытого файла.
        void save_text(const std::string& path, std::uint64_t id, const std::string& text);

        // Создаёт пустой каталог в существующей папке.
        void make_directory(const std::string& path);

        // Добавляет файл с заданным источником в каталог и индекс.
        void add_file(const std::string& directory_path, const std::string& name, UnqPtr<IContentSource> content);

        // Подключает файл с диска как источник содержимого.
        void attach_file(const std::string& real_path, const std::string& directory_path, const std::string& name = "");

        // Импортирует папку с диска и индексирует её файлы.
        void import_directory(const std::string& real_path, const std::string& directory_path);

        // Удаляет запись вместе с поддеревом и убирает его файлы из индекса.
        void remove(const std::string& path);
        // Переносит запись в другой каталог с сохранением имени.
        void move(const std::string& path, const std::string& target_directory);
        // Меняет имя записи и обновляет индекс для обычного файла.
        void rename(const std::string& path, const std::string& new_name);

        // Ищет по имени или расширению через индекс либо полным обходом.
        DynamicArray<ShrdPtr<const RegularFile>> find_files(SearchField field, const std::string& value,  SearchMethod method) const;
};

#endif
