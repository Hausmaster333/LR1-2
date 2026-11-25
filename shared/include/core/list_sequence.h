#ifndef LIST_SEQUENCE_H
#define LIST_SEQUENCE_H

#include <utility>

template <class T>
class ListSequence {
private:
    struct Node {
        T value;
        Node* next;
    };

    Node* head = nullptr;
    Node* tail = nullptr;

    class Iterator {
        Node* node;
    public:
        explicit Iterator(Node* node) : node(node) {}
        T& operator*() const { return node->value; }
        Iterator& operator++() { node = node->next; return *this; }
        bool operator!=(const Iterator& other) const { return node != other.node; }
    };

public:
    ListSequence() = default;
    ListSequence(const ListSequence&) = delete;
    ListSequence& operator=(const ListSequence&) = delete;

    ~ListSequence() { clear(); }

    void clear() {
        while (head) {
            Node* node = head;
            head = head->next;
            delete node;
        }
        tail = nullptr;
    }

    Iterator begin() { return Iterator(head); }
    Iterator end() { return Iterator(nullptr); }

    T& push_back(T value) {
        Node* node = new Node{std::move(value), nullptr};
        if (tail) tail->next = node;
        else head = node;
        tail = node;
        return node->value;
    }

    void remove(const T& value) {
        Node* previous = nullptr;
        for (Node* node = head; node; node = node->next) {
            if (node->value == value) {
                if (previous) previous->next = node->next;
                else head = node->next;
                if (tail == node) tail = previous;
                delete node;
                return;
            }
            previous = node;
        }
    }
};

#endif
