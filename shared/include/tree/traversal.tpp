#include "tree/traversal.h"
#include <utility>

template <class Node>
DFS<Node>::DFS(const Node& root, TraversalOrder order)
    : nodes(root.height() + 1), steps(root.height() + 1), depth(0), order(order) {
    if (root) push(root);
}

template <class Node>
void DFS<Node>::push(Node node) {
    nodes.set(depth, std::move(node));
    steps.set(depth, 0);
    depth++;
}

template <class Node>
Node DFS<Node>::next() {
    while (depth > 0) {
        int step = steps.get(depth - 1);
        if (step == 3) {
            nodes.set(--depth, Node());
            continue;
        }

        steps.set(depth - 1, step + 1);
        const Node& node = nodes.get(depth - 1);
        TreePart part = order.get(step);
        if (part == TreePart::Root) return node;

        Node child = (part == TreePart::Left) ? node.left() : node.right();
        if (child) push(std::move(child));
    }
    return Node();
}

template <class Node>
BFS<Node>::BFS(const Node& root) : queue(1), head(0), tail(0) {
    enqueue(root);
}

template <class Node>
void BFS<Node>::enqueue(Node node) {
    if (!node) return;
    if (tail == queue.get_size()) 
        queue.resize(2 * tail);
    queue.set(tail++, std::move(node));
}

template <class Node>
Node BFS<Node>::next() {
    if (head == tail) return Node();

    Node node = queue.get(head);
    queue.set(head++, Node());
    enqueue(node.left());
    enqueue(node.right());
    return node;
}
