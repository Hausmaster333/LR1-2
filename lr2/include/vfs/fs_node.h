#ifndef FS_NODE_H
#define FS_NODE_H

#include "core/dynamic_array.h"
#include "ptr/shrd_ptr.h"
#include "ptr/unq_ptr.h"
#include "ptr/weak_ptr.h"
#include "tree/tree_dictionary.h"
#include "vfs/content_source.h"
#include <cstdint>
#include <string>

class Directory;

// Узел виртуальной ФС: каталог или файл. Узлом владеет словарь детей родителя (ShrdPtr),
// ссылка на родителя - WeakPtr: иначе родитель и ребёнок держали бы друг друга.
class FsNode {
    friend class Directory; // Имя и родителя меняет только каталог, чтобы не разойтись с ключом в словаре
    private:
        std::string name;
        WeakPtr<Directory> parent;
    public:
        explicit FsNode(const std::string& name) : name(name) {}
        virtual ~FsNode() = default;

        const std::string& get_name() const { return name; }
        ShrdPtr<Directory> get_parent() const { return parent.lock(); } // У корня - пустой
        std::string get_path() const; // Полный путь от корня: "/docs/report.pdf"
        virtual bool is_directory() const = 0;
};

// Каталог: дети лежат в словаре «имя -> узел» на AVL-дереве.
// Узлы хранятся как ShrdPtr: дерево хранит значения копиями, а UnqPtr не копируется.
class Directory : public FsNode {
    private:
        // Конструктор открыт только для ShrdPtr::make внутри create(): снаружи Key не назвать
        struct Key {
            explicit Key() = default;
        };

        TreeDictionary<std::string, ShrdPtr<FsNode>> children;
        int subdirectory_count = 0;
        WeakPtr<Directory> self; // Чтобы записать себя родителем ребёнка
    public:
        Directory(Key, const std::string& name) : FsNode(name) {}
        static ShrdPtr<Directory> create(const std::string& name);

        bool is_directory() const override { return true; }
        int get_child_count() const { return children.get_count(); }
        bool has_subdirectories() const { return subdirectory_count > 0; } // O(1), для дерева в GUI

        ShrdPtr<FsNode> find_child(const std::string& name) const; // Ребёнок по имени или пустой
        DynamicArray<ShrdPtr<const FsNode>> list() const; // Сначала каталоги, потом файлы, по алфавиту

        void add_child(ShrdPtr<FsNode> child); // Имя занято - std::invalid_argument
        ShrdPtr<FsNode> remove_child(const std::string& name); // Отцепляет и отдаёт владение
        void rename_child(const std::string& old_name, const std::string& new_name);
};

// Файл: виртуальное имя плюс источник содержимого
class File : public FsNode {
    private:
        UnqPtr<IContentSource> content;
        std::uint64_t id; // Уникален среди всех файлов: по нему упорядочен индекс
    public:
        File(const std::string& name, UnqPtr<IContentSource> content);

        bool is_directory() const override { return false; }
        const IContentSource& get_content() const { return *content; }
        std::uint64_t get_id() const { return id; }
};

#endif
