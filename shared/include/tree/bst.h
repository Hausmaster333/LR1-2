#ifndef BINARY_SEARCH_TREE_H
#define BINARY_SEARCH_TREE_H

#include "core/dynamic_array.h"
#include "core/icollection.h"
#include "core/ienumerator.h"
#include "core/key_value.h"
#include "core/option.h"
#include "core/value_codec.h"
#include "tree/traversal.h"
#include "ptr/shrd_ptr.h"
#include "ptr/unq_ptr.h"
#include "ptr/weak_ptr.h"
#include <string>
#include <type_traits>
#include <utility>

template <class K, class V>
using TreeEntry = std::conditional_t<std::is_void_v<V>, K, KeyValue<K, V>>;

template <class K, class V = void>
class BinarySearchTree : public ICollection<TreeEntry<K, V>>, public IEnumerable<TreeEntry<K, V>> {
    private:
        using Entry = TreeEntry<K, V>;

        struct Node {
            Entry value;
            ShrdPtr<Node> left;
            ShrdPtr<Node> right;
            int height;
            int subtree_size;

            explicit Node(Entry value) : value(std::move(value)), height(1), subtree_size(1) {}
        };

        using NodePtr = ShrdPtr<Node>;

        class ReadOnlyNode {
            private:
                NodePtr node;

                explicit ReadOnlyNode(const NodePtr& node) : node(node) {}
                friend class BinarySearchTree<K, V>;
            public:
                ReadOnlyNode() = default;

                explicit operator bool() const { return bool(node); }
                const Entry& value() const { return node->value; }
                ReadOnlyNode left() const { return ReadOnlyNode(node->left); }
                ReadOnlyNode right() const { return ReadOnlyNode(node->right); }
                int height() const { return height_of(node); }
        };

        struct Generation {};
        mutable ShrdPtr<Generation> generation;
        NodePtr root;

        static const int MAX_PARSE_DEPTH = 1000;

        explicit BinarySearchTree(NodePtr root);

        static const K& key_of(const Entry& entry);
        static int height_of(const NodePtr& node);
        static int subtree_size_of(const NodePtr& node);
        static bool equivalent(const K& first, const K& second);
        static void update(const NodePtr& node);
        static void rotate_left(NodePtr& node);
        static void rotate_right(NodePtr& node);
        static void rebalance(NodePtr& node);

        static bool insert_into(NodePtr& node, Entry& value);
        static bool remove_from(NodePtr& node, const K& key);

        static const NodePtr& find_link(const NodePtr& node, const K& key);
        const NodePtr& node_at(const K& key) const;
        static const Entry& value_at(const NodePtr& node, int index);
        static int index_in(const NodePtr& node, const K& key, int before_left_node);

        static NodePtr clone(const NodePtr& node);
        static bool same_structure(const NodePtr& first, const NodePtr& second);
        static NodePtr build_balanced(const DynamicArray<NodePtr>& sorted, int begin, int end);

        static void write_node(const NodePtr& node, const TraversalFormat& format, std::string& out);
        static NodePtr read_node(const std::string& text, std::size_t& pos, const TraversalFormat& format, int depth);
        static bool is_balanced(const NodePtr& node);

        template <class F>
        static void visit_from(const NodePtr& node, int depth, int parent_index, int& next_index, F& visit);

        static void check(const WeakPtr<Generation>& generation);
        WeakPtr<Generation> current_generation() const;
        void on_modified();

        template <template <class> class Traversal = DFS, class... Args>
        auto walk(const Args&... args) const {
            return Traversal<ReadOnlyNode>(ReadOnlyNode(root), args...);
        }

    public:
        BinarySearchTree();
        BinarySearchTree(const Entry* items, int count);
        BinarySearchTree(const BinarySearchTree& other);
        BinarySearchTree(BinarySearchTree&& other) noexcept;
        BinarySearchTree& operator=(const BinarySearchTree& other);
        BinarySearchTree& operator=(BinarySearchTree&& other) noexcept;

        bool insert(Entry value);
        bool remove(const K& key);
        void clear();
        Option<Entry> try_find(const K& key) const;
        const Entry& find(const K& key) const;
        auto& find_value(const K& key);

        const Entry& get(int index) const override;
        int get_count() const override;
        bool contains(const Entry& value) const override;
        bool contains_key(const K& key) const;
        int get_height() const;
        int index_of(const K& key) const;

        template <class F>
        void visit_nodes(F visit) const;

        template <class F>
        auto map(F func) const;

        template <class F>
        BinarySearchTree where(F predicate) const;

        template <template <class> class Traversal = DFS, class A, class F, class... Args>
        A reduce(F func, A initial, const Args&... args) const;

        template <template <class> class Traversal = DFS, class... Args>
        DynamicArray<Entry> to_array(const Args&... args) const;

        UnqPtr<IEnumerator<Entry>> get_enumerator() const override;

        template <template <class> class Traversal, class... Args>
        UnqPtr<IEnumerator<Entry>> get_enumerator(const Args&... args) const;

        BinarySearchTree get_subtree(const K& key) const;
        bool contains_subtree(const BinarySearchTree& other) const;

        std::string to_string(const TraversalFormat& format = TraversalFormat()) const;
        static BinarySearchTree from_string(const std::string& text, const TraversalFormat& format = TraversalFormat());

        const Entry& operator[](int index) const;
        BinarySearchTree& operator+=(Entry value);
        BinarySearchTree& operator-=(const K& key);
        bool operator==(const BinarySearchTree& other) const;
        bool operator!=(const BinarySearchTree& other) const;

        template <class Traversal>
        class TraversalEnumerator : public IEnumerator<Entry> {
            private:
                Traversal start;
                Traversal walker;
                ReadOnlyNode current;
                WeakPtr<Generation> generation;
            public:
                TraversalEnumerator(const Traversal& walker, const WeakPtr<Generation>& generation)
                    : start(walker), walker(walker), generation(generation) {}

                bool move_next() override;
                const Entry& get_current() const override;
                void reset() override;
        };

        ~BinarySearchTree() override = default;
};

#include "tree/bst.tpp"

#endif
