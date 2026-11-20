#include "tree/bst.h"
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class K, class V>
void BinarySearchTree<K, V>::check(const WeakPtr<Generation>& generation) {
    if (generation.expired()) {
        throw std::logic_error("Tree was modified or destroyed during enumeration");
    }
}

template <class K, class V>
WeakPtr<typename BinarySearchTree<K, V>::Generation>
BinarySearchTree<K, V>::current_generation() const {
    if (!generation) generation = ShrdPtr<Generation>::make();
    return generation;
}

template <class K, class V>
template <class Traversal>
bool BinarySearchTree<K, V>::TraversalEnumerator<Traversal>::move_next() {
    check(generation);
    current = walker.next();
    return bool(current);
}

template <class K, class V>
template <class Traversal>
const TreeEntry<K, V>& BinarySearchTree<K, V>::TraversalEnumerator<Traversal>::get_current() const {
    check(generation);
    if (!current) throw std::logic_error("Enumerator has no current element");
    return current.value();
}

template <class K, class V>
template <class Traversal>
void BinarySearchTree<K, V>::TraversalEnumerator<Traversal>::reset() {
    walker = start;
    current = ReadOnlyNode();
}

template <class K, class V>
bool BinarySearchTree<K, V>::ThreadedEnumerator::move_next() {
    check(generation);
    if (!started) {
        current = head.lock();
    } else if (current) {
        current = current->thread_next.lock();
    }
    started = true;
    return bool(current);
}

template <class K, class V>
const TreeEntry<K, V>& BinarySearchTree<K, V>::ThreadedEnumerator::get_current() const {
    check(generation);
    if (!current) throw std::logic_error("Enumerator has no current element");
    return current->value;
}

template <class K, class V>
void BinarySearchTree<K, V>::ThreadedEnumerator::reset() {
    current.reset();
    started = false;
}

template <class K, class V>
BinarySearchTree<K, V>::BinarySearchTree() : root(), threads_stale(false) {}

template <class K, class V>
BinarySearchTree<K, V>::BinarySearchTree(NodePtr root) : root(std::move(root)), threads_stale(false) {}

template <class K, class V>
BinarySearchTree<K, V>::BinarySearchTree(const Entry* items, int count) : BinarySearchTree() {
    if (count < 0) throw std::invalid_argument("Count must be >= 0");
    if (items == nullptr && count > 0) throw std::invalid_argument("Items is nullptr");

    for (int idx = 0; idx < count; idx++) insert(items[idx]);
}

template <class K, class V>
BinarySearchTree<K, V>::BinarySearchTree(const BinarySearchTree<K, V>& other)
    : root(clone(other.root)), thread_rule(other.thread_rule), threads_stale(other.is_threaded()) {}

template <class K, class V>
BinarySearchTree<K, V>::BinarySearchTree(BinarySearchTree<K, V>&& other) noexcept
    : generation(std::move(other.generation)), 
      root(std::move(other.root)),
      thread_head(std::move(other.thread_head)), thread_rule(std::move(other.thread_rule)), threads_stale(other.threads_stale) {
    other.thread_rule = nullptr;
}

template <class K, class V>
BinarySearchTree<K, V>& BinarySearchTree<K, V>::operator=(const BinarySearchTree<K, V>& other) {
    if (this == &other) return *this;

    BinarySearchTree<K, V> copy(other);
    *this = std::move(copy);
    return *this;
}

template <class K, class V>
BinarySearchTree<K, V>& BinarySearchTree<K, V>::operator=(BinarySearchTree<K, V>&& other) noexcept {
    if (this == &other) return *this;

    root = std::move(other.root);
    thread_head = std::move(other.thread_head);
    thread_rule = std::move(other.thread_rule);
    other.thread_rule = nullptr;
    threads_stale = other.threads_stale;
    generation = std::move(other.generation);
    return *this;
}

template <class K, class V>
const K& BinarySearchTree<K, V>::key_of(const Entry& entry) {
    if constexpr (std::is_void_v<V>) {
        return entry;
    } else {
        return entry.key;
    }
}

template <class K, class V>
int BinarySearchTree<K, V>::height_of(const NodePtr& node) {
    return node ? node->height : 0;
}

template <class K, class V>
int BinarySearchTree<K, V>::subtree_size_of(const NodePtr& node) {
    return node ? node->subtree_size : 0;
}

template <class K, class V>
bool BinarySearchTree<K, V>::equivalent(const K& first, const K& second) {
    return !(first < second) && !(second < first);
}

template <class K, class V>
void BinarySearchTree<K, V>::update(const NodePtr& node) {
    int left_height = height_of(node->left);
    int right_height = height_of(node->right);

    node->height = 1 + (left_height > right_height ? left_height : right_height);
    node->subtree_size = 1 + subtree_size_of(node->left) + subtree_size_of(node->right);
}

template <class K, class V>
void BinarySearchTree<K, V>::rotate_left(NodePtr& node) {
    NodePtr pivot = std::move(node->right);
    node->right = std::move(pivot->left);
    update(node);
    pivot->left = std::move(node);
    update(pivot);
    node = std::move(pivot);
}

template <class K, class V>
void BinarySearchTree<K, V>::rotate_right(NodePtr& node) {
    NodePtr pivot = std::move(node->left);
    node->left = std::move(pivot->right);
    update(node);
    pivot->right = std::move(node);
    update(pivot);
    node = std::move(pivot);
}

template <class K, class V>
void BinarySearchTree<K, V>::rebalance(NodePtr& node) {
    update(node);
    int balance_factor = height_of(node->left) - height_of(node->right);

    if (balance_factor > 1) {
        const NodePtr& left = node->left;
        if (height_of(left->left) < height_of(left->right)) {
            rotate_left(node->left);
        }
        rotate_right(node);
    } else if (balance_factor < -1) {
        const NodePtr& right = node->right;
        if (height_of(right->right) < height_of(right->left)) {
            rotate_right(node->right);
        }
        rotate_left(node);
    }
}

template <class K, class V>
bool BinarySearchTree<K, V>::insert_into(NodePtr& node, Entry& value) {
    if (!node) {
        node = NodePtr::make(std::move(value));
        return true;
    }

    const K& key = key_of(value);
    bool inserted = false;
    if (key < key_of(node->value)) {
        inserted = insert_into(node->left, value);
    } else if (key_of(node->value) < key) {
        inserted = insert_into(node->right, value);
    } else {
        return false;
    }

    if (inserted) rebalance(node);
    return inserted;
}

template <class K, class V>
bool BinarySearchTree<K, V>::remove_from(NodePtr& node, const K& key) {
    if (!node) return false;

    bool removed = true;
    if (key < key_of(node->value)) {
        removed = remove_from(node->left, key);
    } else if (key_of(node->value) < key) {
        removed = remove_from(node->right, key);
    } else if (!node->left) {
        node = std::move(node->right);
    } else if (!node->right) {
        node = std::move(node->left);
    } else {

        NodePtr successor = node->right;
        while (successor->left) 
            successor = successor->left;

        remove_from(node->right, key_of(successor->value));
        successor->left = std::move(node->left);
        successor->right = std::move(node->right);
        node = std::move(successor);
    }

    if (removed && node) rebalance(node);
    return removed;
}

template <class K, class V>
const typename BinarySearchTree<K, V>::NodePtr&
BinarySearchTree<K, V>::find_link(const NodePtr& node, const K& key) {
    if (!node) return node;
    if (key < key_of(node->value)) return find_link(node->left, key);
    if (key_of(node->value) < key) return find_link(node->right, key);
    return node;
}

template <class K, class V>
const TreeEntry<K, V>& BinarySearchTree<K, V>::value_at(const NodePtr& node, int index) {
    int left_size = subtree_size_of(node->left);
    if (index < left_size) return value_at(node->left, index);
    if (index == left_size) return node->value;
    return value_at(node->right, index - left_size - 1);
}

// before_left_node - сколько элементов левее поддерева node
template <class K, class V>
int BinarySearchTree<K, V>::index_in(const NodePtr& node, const K& key, int before_left_node) {
    if (!node) return -1;
    if (key < key_of(node->value)) return index_in(node->left, key, before_left_node);
    if (key_of(node->value) < key) return index_in(node->right, key, before_left_node + subtree_size_of(node->left) + 1);
    return before_left_node + subtree_size_of(node->left);
}

template <class K, class V>
typename BinarySearchTree<K, V>::NodePtr BinarySearchTree<K, V>::clone(const NodePtr& node) {
    if (!node) return {};

    auto copy = NodePtr::make(node->value);
    copy->left = clone(node->left);
    copy->right = clone(node->right);
    copy->height = node->height;
    copy->subtree_size = node->subtree_size;
    return copy;
}

template <class K, class V>
bool BinarySearchTree<K, V>::same_structure(const NodePtr& first, const NodePtr& second) {
    if (!first || !second) return !first && !second;

    return equivalent(key_of(first->value), key_of(second->value))
        && same_structure(first->left, second->left)
        && same_structure(first->right, second->right);
}
template <class K, class V>
typename BinarySearchTree<K, V>::NodePtr
BinarySearchTree<K, V>::build_balanced(const DynamicArray<NodePtr>& sorted, int begin, int end) {
    if (begin >= end) return {};

    int middle = begin + (end - begin) / 2;
    NodePtr node = sorted.get(middle);
    node->left = build_balanced(sorted, begin, middle);
    node->right = build_balanced(sorted, middle + 1, end);
    update(node);
    return node;
}

template <class K, class V>
void BinarySearchTree<K, V>::on_modified() {
    generation.reset();
    if (thread_rule) threads_stale = true;
}

template <class K, class V>
void BinarySearchTree<K, V>::clear_threads() const {
    auto walker = walk();
    while (ReadOnlyNode node = walker.next())
        node.node->thread_next.reset();
    thread_head.reset();
}

template <class K, class V>
template <template <class> class Traversal, class... Args>
void BinarySearchTree<K, V>::link_threads(const Args&... args) const {
    clear_threads();
    NodePtr previous;
    auto walker = walk<Traversal>(args...);
    while (ReadOnlyNode view = walker.next()) {
        const NodePtr& node = view.node;

        if (!previous) {
            thread_head = node;
        } else {
            previous->thread_next = node;
        }
        previous = node;
    }
    threads_stale = false;
}

template <class K, class V>
bool BinarySearchTree<K, V>::insert(Entry value) {
    bool inserted = insert_into(root, value);
    if (inserted) on_modified();
    return inserted;
}

template <class K, class V>
bool BinarySearchTree<K, V>::remove(const K& key) {
    bool removed = remove_from(root, key);
    if (removed) on_modified();
    return removed;
}

template <class K, class V>
void BinarySearchTree<K, V>::clear() {
    root.reset();
    on_modified();
}

template <class K, class V>
Option<TreeEntry<K, V>> BinarySearchTree<K, V>::try_find(const K& key) const {
    const NodePtr& node = find_link(root, key);
    return node ? Option<Entry>::Some(node->value) : Option<Entry>::None();
}

template <class K, class V>
const TreeEntry<K, V>& BinarySearchTree<K, V>::get(int index) const {
    if (index < 0 || index >= get_count()) throw std::out_of_range("Index out of range");
    return value_at(root, index);
}

template <class K, class V>
int BinarySearchTree<K, V>::get_count() const {
    return subtree_size_of(root);
}

template <class K, class V>
const typename BinarySearchTree<K, V>::NodePtr& BinarySearchTree<K, V>::node_at(const K& key) const {
    const NodePtr& node = find_link(root, key);
    if (!node) throw std::out_of_range("Key not found");
    return node;
}

template <class K, class V>
const TreeEntry<K, V>& BinarySearchTree<K, V>::find(const K& key) const {
    return node_at(key)->value;
}

template <class K, class V>
auto& BinarySearchTree<K, V>::find_value(const K& key) {
    static_assert(!std::is_void_v<V>, "A set has no mapped value");
    return node_at(key)->value.value;
}

template <class K, class V>
bool BinarySearchTree<K, V>::contains(const Entry& value) const {
    return contains_key(key_of(value));
}

template <class K, class V>
bool BinarySearchTree<K, V>::contains_key(const K& key) const {
    return bool(find_link(root, key));
}

template <class K, class V>
int BinarySearchTree<K, V>::get_height() const {
    return height_of(root);
}

template <class K, class V>
int BinarySearchTree<K, V>::index_of(const K& key) const {
    return index_in(root, key, 0);
}

template <class K, class V>
template <class F>
void BinarySearchTree<K, V>::visit_from(const NodePtr& node, int depth, int parent_index, int& next_index, F& visit) {
    if (!node) return;

    int index = next_index + subtree_size_of(node->left);
    visit_from(node->left, depth + 1, index, next_index, visit);
    visit(std::as_const(node->value), depth, index, parent_index);
    next_index = index + 1;
    visit_from(node->right, depth + 1, index, next_index, visit);
}

template <class K, class V>
template <class F>
void BinarySearchTree<K, V>::visit_nodes(F visit) const {
    int next_index = 0;
    visit_from(root, 0, -1, next_index, visit);
}

template <class K, class V>
template <class F>
auto BinarySearchTree<K, V>::map(F func) const {
    using Result = std::decay_t<decltype(func(get(0)))>;
    BinarySearchTree<Result> result;
    auto walker = walk();
    while (ReadOnlyNode node = walker.next()) 
        result.insert(func(node.value()));
    return result;
}

template <class K, class V>
template <class F>
BinarySearchTree<K, V> BinarySearchTree<K, V>::where(F predicate) const {
    DynamicArray<NodePtr> matched(get_count());
    int matched_count = 0;
    auto walker = walk();
    while (ReadOnlyNode node = walker.next()) {
        if (predicate(node.value())) 
            matched.set(matched_count++, NodePtr::make(node.value()));
    }
    return BinarySearchTree<K, V>(build_balanced(matched, 0, matched_count));
}

template <class K, class V>
template <template <class> class Traversal, class A, class F, class... Args>
A BinarySearchTree<K, V>::reduce(F func, A initial, const Args&... args) const {
    A accumulator = std::move(initial);
    auto walker = walk<Traversal>(args...);
    while (ReadOnlyNode node = walker.next()) {
        accumulator = func(node.value(), std::move(accumulator));
    }
    return accumulator;
}

template <class K, class V>
BinarySearchTree<K, V> BinarySearchTree<K, V>::get_subtree(const K& key) const {
    const NodePtr& node = find_link(root, key);
    if (!node) throw std::invalid_argument("Subtree root not found");

    return BinarySearchTree<K, V>(clone(node));
}

template <class K, class V>
bool BinarySearchTree<K, V>::contains_subtree(const BinarySearchTree<K, V>& other) const {
    if (!other.root) return true;

    const NodePtr& node = find_link(root, key_of(other.root->value));
    return node && same_structure(node, other.root);
}

template <class K, class V>
template <template <class> class Traversal, class... Args>
void BinarySearchTree<K, V>::thread(const Args&... args) {
    auto rule = [args...](const BinarySearchTree& tree) { tree.link_threads<Traversal>(args...); };
    generation.reset();
    thread_rule = nullptr;
    link_threads<Traversal>(args...);
    thread_rule = std::move(rule);
}

template <class K, class V>
bool BinarySearchTree<K, V>::is_threaded() const {
    return thread_rule != nullptr;
}

template <class K, class V>
UnqPtr<IEnumerator<TreeEntry<K, V>>> BinarySearchTree<K, V>::get_threaded_enumerator() const {
    if (!thread_rule) throw std::logic_error("Tree is not threaded");
    if (threads_stale) thread_rule(*this);
    return UnqPtr<ThreadedEnumerator>::make(thread_head, current_generation());
}

template <class K, class V>
template <template <class> class Traversal, class... Args>
DynamicArray<TreeEntry<K, V>> BinarySearchTree<K, V>::to_array(const Args&... args) const {
    DynamicArray<Entry> result(get_count());
    int index = 0;
    auto walker = walk<Traversal>(args...);
    while (ReadOnlyNode node = walker.next()) result.set(index++, node.value());
    if (index < result.get_size()) result.resize(index);
    return result;
}

template <class K, class V>
void BinarySearchTree<K, V>::write_node(const NodePtr& node, const TraversalFormat& format, std::string& out) {
    if (!node) return;

    const TraversalOrder& order = format.get_order();
    out += format.get_literal(0);
    for (int index = 0; index < 3; index++) {
        TreePart part = order.get(index);
        if (part == TreePart::Root) {
            out += format.escape(ValueCodec<Entry>::to_string(node->value));
        } else {
            write_node(part == TreePart::Left ? node->left : node->right, format, out);
        }
        out += format.get_literal(index + 1);
    }
}

template <class K, class V>
std::string BinarySearchTree<K, V>::to_string(const TraversalFormat& format) const {
    std::string result;
    write_node(root, format, result);
    return result;
}

template <class K, class V>
typename BinarySearchTree<K, V>::NodePtr
BinarySearchTree<K, V>::read_node(const std::string& text, std::size_t& pos, const TraversalFormat& format, int depth) {
    if (!format.literal_at(text, pos, 0)) return {};
    if (depth > MAX_PARSE_DEPTH) throw std::length_error("Tree in string is too deep");
    pos += format.get_literal(0).size();

    Option<Entry> value;
    NodePtr left;
    NodePtr right;
    const TraversalOrder& order = format.get_order();
    for (int index = 0; index < 3; index++) {
        TreePart part = order.get(index);
        if (part == TreePart::Root) {
            value = Option<Entry>::Some(ValueCodec<Entry>::from_string(format.read_value(text, pos)));
        } else if (part == TreePart::Left) {
            left = read_node(text, pos, format, depth + 1);
        } else {
            right = read_node(text, pos, format, depth + 1);
        }

        if (!format.literal_at(text, pos, index + 1)) {
            throw std::invalid_argument("Expected '" + format.get_literal(index + 1)
                                        + "' at position " + std::to_string(pos));
        }
        pos += format.get_literal(index + 1).size();
    }

    auto node = NodePtr::make(value.get_value());
    node->left = std::move(left);
    node->right = std::move(right);
    update(node);
    return node;
}

template <class K, class V>
bool BinarySearchTree<K, V>::is_balanced(const NodePtr& node) {
    if (!node) return true;

    int balance = height_of(node->left) - height_of(node->right);
    return balance >= -1 && balance <= 1 && is_balanced(node->left) && is_balanced(node->right);
}

template <class K, class V>
BinarySearchTree<K, V> BinarySearchTree<K, V>::from_string(const std::string& text, const TraversalFormat& format) {
    std::size_t pos = 0;
    NodePtr parsed = read_node(text, pos, format, 0);
    if (pos != text.size()) {
        throw std::invalid_argument("Unexpected symbol at position " + std::to_string(pos));
    }

    BinarySearchTree<K, V> result(std::move(parsed));
    auto in_order = result.walk();
    ReadOnlyNode previous;
    while (auto current = in_order.next()) {
        if (previous && !(key_of(previous.value()) < key_of(current.value()))) {
            throw std::invalid_argument("String does not describe a binary search tree");
        }
        previous = std::move(current);
    }

    if (!is_balanced(result.root)) {
        DynamicArray<NodePtr> sorted(result.get_count());
        int index = 0;
        auto walker = result.walk();
        while (ReadOnlyNode node = walker.next())
            sorted.set(index++, node.node);
        result.root = build_balanced(sorted, 0, sorted.get_size());
    }
    return result;
}

template <class K, class V>
UnqPtr<IEnumerator<TreeEntry<K, V>>> BinarySearchTree<K, V>::get_enumerator() const {
    return get_enumerator<DFS>();
}

template <class K, class V>
template <template <class> class Traversal, class... Args>
UnqPtr<IEnumerator<TreeEntry<K, V>>>
BinarySearchTree<K, V>::get_enumerator(const Args&... args) const {
    using Enumerator = TraversalEnumerator<Traversal<ReadOnlyNode>>;
    return UnqPtr<Enumerator>::make(walk<Traversal>(args...), current_generation());
}

template <class K, class V>
const TreeEntry<K, V>& BinarySearchTree<K, V>::operator[](int index) const {
    return get(index);
}

template <class K, class V>
BinarySearchTree<K, V>& BinarySearchTree<K, V>::operator+=(Entry value) {
    insert(std::move(value));
    return *this;
}

template <class K, class V>
BinarySearchTree<K, V>& BinarySearchTree<K, V>::operator-=(const K& key) {
    remove(key);
    return *this;
}

template <class K, class V>
bool BinarySearchTree<K, V>::operator==(const BinarySearchTree<K, V>& other) const {
    if (get_count() != other.get_count()) return false;

    auto this_walker = walk();
    auto other_walker = other.walk();
    while (ReadOnlyNode node = this_walker.next()) {
        if (!equivalent(key_of(node.value()), key_of(other_walker.next().value()))) return false;
    }
    return true;
}

template <class K, class V>
bool BinarySearchTree<K, V>::operator!=(const BinarySearchTree<K, V>& other) const {
    return !(*this == other);
}
