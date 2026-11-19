#ifndef TREE_SET_H
#define TREE_SET_H

#include "core/iset.h"
#include "tree/bst.h"

template <class K>
class TreeSet : public ISet<K> {
    private:
        BinarySearchTree<K> tree;
    public:
        int get_count() const override { return tree.get_count(); }
        bool contains(const K& key) const override { return tree.contains_key(key); }
        bool add(K key) override { return tree.insert(std::move(key)); }
        bool remove(const K& key) override { return tree.remove(key); }
        UnqPtr<IEnumerator<K>> get_enumerator() const override { return tree.get_enumerator(); }
};

#endif
