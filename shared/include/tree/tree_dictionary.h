#ifndef TREE_DICTIONARY_H
#define TREE_DICTIONARY_H

#include "core/idictionary.h"
#include "tree/bst.h"
#include <stdexcept>

// Словарь на AVL-дереве: пары упорядочены по ключу, операции за O(log n),
// перебор по возрастанию ключей. Поиск идёт по самому ключу, а пара переезжает в узел
// перемещением, поэтому от V не нужно ничего: ни конструктора по умолчанию, ни копирования.
// Повторный ключ в add и отсутствующий в get/remove - исключение, а не тихий пропуск.
template <class K, class V>
class TreeDictionary : public IDictionary<K, V> {
    private:
        mutable BinarySearchTree<K, V> tree; // В узле лежит KeyValue<K, V>
    public:
        int get_count() const override { return tree.get_count(); }
        bool contains_key(const K& key) const override { return tree.contains_key(key); }

        const V& get(const K& key) const override { return tree.find(key).value; }
        V& get(const K& key) override { return tree.find_value(key); }

        void add(K key, V value) override {
            if (!tree.insert({std::move(key), std::move(value)})) {
                throw std::invalid_argument("Key already exists");
            }
        }

        void remove(const K& key) override {
            if (!tree.remove(key)) throw std::out_of_range("Key not found");
        }

        UnqPtr<IEnumerator<KeyValue<K, V>>> get_threaded_enumerator() const {
            if (!tree.is_threaded()) tree.thread();
            return tree.get_threaded_enumerator();
        }

        UnqPtr<IEnumerator<KeyValue<K, V>>> get_enumerator() const override {
            return tree.get_enumerator();
        }
};

#endif
