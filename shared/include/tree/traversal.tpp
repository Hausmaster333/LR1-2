#include "tree/traversal.h"
#include <utility>

// ---------- DepthFirst ----------

template <class Node>
DepthFirst::Walk<Node>::Walk(const Node& root, const DepthFirst& traversal)
    : nodes(root.height() + 1), steps(root.height() + 1), depth(0), order(traversal.get_order()) {
    if (root) push(root);
}

template <class Node>
void DepthFirst::Walk<Node>::push(Node node) {
    nodes.set(depth, std::move(node));
    steps.set(depth, 0);
    depth++;
}

template <class Node>
Node DepthFirst::Walk<Node>::next() {
    while (depth > 0) {
        int step = steps.get(depth - 1);
        if (step == 3) {
            nodes.set(--depth, Node()); // Пройденный узел больше не держим
            continue;
        }

        steps.set(depth - 1, step + 1);
        const Node& node = nodes.get(depth - 1);
        TreePart part = order.get(step);
        if (part == TreePart::Root) return node;

        Node child = (part == TreePart::Left) ? node.left() : node.right();
        if (child) push(std::move(child)); // Узел переезжает в стек без лишней копии
    }
    return Node();
}

// ---------- LevelOrder ----------

template <class Node>
LevelOrder::Walk<Node>::Walk(const Node& root, const LevelOrder&) : queue(1), head(0), tail(0) {
    enqueue(root);
}

template <class Node>
void LevelOrder::Walk<Node>::enqueue(Node node) {
    if (!node) return;
    if (tail == queue.get_size()) queue.resize(2 * tail);
    queue.set(tail++, std::move(node));
}

template <class Node>
Node LevelOrder::Walk<Node>::next() {
    if (head == tail) return Node();

    Node node = queue.get(head);
    queue.set(head++, Node()); // Пройденный узел больше не держим
    enqueue(node.left());
    enqueue(node.right());
    return node;
}
