#ifndef VIRTUAL_FILE_SYSTEM_H
#define VIRTUAL_FILE_SYSTEM_H

#include "vfs/file_index.h"
#include "vfs/fs_node.h"
#include <string>

// Виртуальная ФС: иерархия каталогов в памяти поверх реальных файлов.
// Пути абсолютные, через '/': "/docs/report.pdf". Реальные файлы никогда не меняются.
// При ошибке бросается исключение (неверный путь или имя - std::invalid_argument),
// и ФС остаётся в прежнем состоянии.
class VirtualFileSystem {
    private:
        ShrdPtr<Directory> root;
        FileIndex index; // Обновляется каждой операцией, меняющей набор или имена файлов

        ShrdPtr<FsNode> find(const std::string& path) const; // Узел по пути или пустой
        ShrdPtr<FsNode> node_at(const std::string& path) const; // Узел по пути; нет - исключение
        ShrdPtr<Directory> directory_at(const std::string& path) const; // То же, но обязательно каталог
        ShrdPtr<FsNode> movable_node_at(const std::string& path) const; // Любой узел, кроме корня
    public:
        VirtualFileSystem();
        // Копия делила бы узлы с оригиналом, а индекс - нет; копировать ФС незачем
        VirtualFileSystem(const VirtualFileSystem&) = delete;
        VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

        bool exists(const std::string& path) const;
        const FsNode& get(const std::string& path) const;
        const Directory& get_root() const { return *root; }
        DynamicArray<ShrdPtr<const FsNode>> list(const std::string& directory_path) const;

        // Создаёт каталог; родитель должен существовать
        void make_directory(const std::string& path);
        // Добавляет файл с любым источником содержимого (генератор, бенчмарк)
        void add_file(const std::string& directory_path, const std::string& name,
                      UnqPtr<IContentSource> content);
        // Прикрепляет реальный файл к каталогу; пустое имя - взять имя реального файла
        void attach_file(const std::string& real_path, const std::string& directory_path,
                         const std::string& name = "");
        // Прикрепляет реальную папку целиком, с подкаталогами, как новый каталог
        void import_directory(const std::string& real_path, const std::string& directory_path);

        void remove(const std::string& path); // Файл или каталог со всем содержимым
        void move(const std::string& path, const std::string& target_directory);
        void rename(const std::string& path, const std::string& new_name);

        // Файлы с точным именем или расширением (без учёта регистра латиницы):
        // полным обходом за O(n) или по индексу за O(log n). Порядок не гарантирован.
        DynamicArray<ShrdPtr<const File>> find_files(SearchField field, const std::string& value,
                                                     SearchMethod method) const;
};

#endif
