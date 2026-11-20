#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "core/dynamic_array.h"
#include <cstddef>
#include <stdexcept>
#include <string>

enum class TreePart { Root, Left, Right };

inline std::size_t match_tree_part(const std::string& text, std::size_t pos, TreePart& part) {
    struct PartSymbol {
        const char* text;
        TreePart part;
    };

    static constexpr PartSymbol PART_SYMBOLS[] = {
        {"К", TreePart::Root}, {"Л", TreePart::Left}, {"П", TreePart::Right},
        {"K", TreePart::Root}, {"N", TreePart::Root},
        {"L", TreePart::Left},
        {"P", TreePart::Right}, {"R", TreePart::Right},
    };

    for (const PartSymbol& symbol : PART_SYMBOLS) {
        std::string candidate(symbol.text);
        if (text.compare(pos, candidate.size(), candidate) == 0) {
            part = symbol.part;
            return candidate.size();
        }
    }
    return 0;
}

inline const char* tree_part_name(TreePart part) {
    switch (part) {
        case TreePart::Root: return "К";
        case TreePart::Left: return "Л";
        default: return "П";
    }
}

class TraversalOrder {
    private:
        TreePart parts[3];

        void validate() const {
            bool seen[3] = {false, false, false};
            for (TreePart part : parts) {
                int index = static_cast<int>(part);
                if (seen[index]) throw std::invalid_argument("Traversal part repeats");
                seen[index] = true;
            }
        }
    public:
        TraversalOrder() : parts{TreePart::Left, TreePart::Root, TreePart::Right} {} // ЛКП

        TraversalOrder(TreePart first, TreePart second, TreePart third)
            : parts{first, second, third} {
            validate();
        }

        static TraversalOrder from_string(const std::string& text) {
            TreePart found[3];
            int count = 0;
            std::size_t pos = 0;
            while (pos < text.size()) {
                TreePart part;
                std::size_t length = match_tree_part(text, pos, part);
                if (length == 0 || count == 3) {
                    throw std::invalid_argument("Bad traversal order: '" + text + "'");
                }
                found[count++] = part;
                pos += length;
            }
            if (count != 3) throw std::invalid_argument("Bad traversal order: '" + text + "'");
            return TraversalOrder(found[0], found[1], found[2]);
        }

        TreePart get(int index) const {
            if (index < 0 || index > 2) throw std::out_of_range("Traversal index out of range");
            return parts[index];
        }

        std::string to_string() const {
            std::string result;
            for (TreePart part : parts) result += tree_part_name(part);
            return result;
        }

        bool operator==(const TraversalOrder& other) const {
            return parts[0] == other.parts[0] && parts[1] == other.parts[1]
                && parts[2] == other.parts[2];
        }
        bool operator!=(const TraversalOrder& other) const { return !(*this == other); }
};

template <class Node>
class DFS {
    private:
        DynamicArray<Node> nodes;
        DynamicArray<int> steps;
        int depth;
        TraversalOrder order;

        void push(Node node);
    public:
        explicit DFS(const Node& root, TraversalOrder order = {});
        DFS(const Node& root, const std::string& order) : DFS(root, TraversalOrder::from_string(order)) {}
        Node next();
};

template <class Node>
class BFS {
    private:
        DynamicArray<Node> queue;
        int head;
        int tail;

        void enqueue(Node node);
    public:
        explicit BFS(const Node& root);
        Node next();
};

class TraversalFormat {
    private:
        std::string literals[4];
        TraversalOrder order;

        void validate() const {
            for (const std::string& literal : literals) {
                if (literal.empty()) {
                    throw std::invalid_argument("Format needs delimiters around every part");
                }
                if (literal.find('\\') != std::string::npos) {
                    throw std::invalid_argument("'\\' is reserved for escaping");
                }
            }
            for (int index = 0; index < 3; index++) {
                bool is_subtree = order.get(index) != TreePart::Root;
                if (is_subtree && literals[index + 1][0] == literals[0][0]) {
                    throw std::invalid_argument(
                        "Delimiter after a subtree must not start like the node opening");
                }
            }
        }
    public:
        TraversalFormat() : TraversalFormat("{К}(Л)[П]") {}

        TraversalFormat(const std::string& pattern) {
            TreePart found[3];
            int count = 0;
            std::size_t pos = 0;
            while (pos < pattern.size()) {
                TreePart part;
                std::size_t length = match_tree_part(pattern, pos, part);
                if (length == 0) {
                    literals[count] += pattern[pos++];
                    continue;
                }
                if (count == 3) throw std::invalid_argument("Format has more than 3 parts");
                found[count++] = part;
                pos += length;
            }
            if (count != 3) throw std::invalid_argument("Format must contain К, Л and П");

            order = TraversalOrder(found[0], found[1], found[2]);
            validate();
        }

        const TraversalOrder& get_order() const { return order; }

        const std::string& get_literal(int index) const {
            if (index < 0 || index > 3) throw std::out_of_range("Literal index out of range");
            return literals[index];
        }

        bool is_delimiter(char symbol) const {
            if (symbol == '\\') return true;
            for (const std::string& literal : literals) {
                if (literal.find(symbol) != std::string::npos) return true;
            }
            return false;
        }

        bool literal_at(const std::string& text, std::size_t pos, int index) const {
            return text.compare(pos, literals[index].size(), literals[index]) == 0;
        }

        std::string escape(const std::string& value) const {
            std::string result;
            for (char symbol : value) {
                if (is_delimiter(symbol)) result += '\\';
                result += symbol;
            }
            return result;
        }

        std::string read_value(const std::string& text, std::size_t& pos) const {
            std::string value;
            while (pos < text.size()) {
                char symbol = text[pos];
                if (symbol == '\\') {
                    if (pos + 1 >= text.size()) {
                        throw std::invalid_argument("Dangling '\\' at end of string");
                    }
                    value += text[pos + 1];
                    pos += 2;
                    continue;
                }
                if (is_delimiter(symbol)) break;
                value += symbol;
                pos++;
            }
            return value;
        }

        std::string to_string() const {
            std::string result = literals[0];
            for (int index = 0; index < 3; index++) {
                result += tree_part_name(order.get(index));
                result += literals[index + 1];
            }
            return result;
        }
};

#include "tree/traversal.tpp"

#endif
