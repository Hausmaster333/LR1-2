#ifndef GUI_VALUES_H
#define GUI_VALUES_H

#include "tree/bst.h"
#include "types/value_codec.h"
#include <cctype>
#include <cmath>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace gui_detail {
    // Обходы в списках интерфейса: шесть порядков в глубину и по уровням
    inline const char* TRAVERSAL_NAMES[] = {"КЛП", "КПЛ", "ЛПК", "ЛКП", "ПЛК", "ПКЛ", "по уровням"};
    const int TRAVERSAL_COUNT = 7;
    const int IN_ORDER_INDEX = 3;
    const int LEVEL_ORDER_INDEX = 6;
}

// Обход, выбранный в списке интерфейса по номеру. Сам тоже стратегия обхода: перебор
// отдаёт готовой стратегии - DepthFirst с порядком из списка или LevelOrder
class ListedTraversal {
    private:
        int index;
    public:
        explicit ListedTraversal(int index) : index(index) {}

        bool is_level_order() const { return index == gui_detail::LEVEL_ORDER_INDEX; }
        DepthFirst depth_first() const {
            if (is_level_order()) return DepthFirst();
            return DepthFirst(gui_detail::TRAVERSAL_NAMES[index]);
        }

        template <class Node>
        class Walk {
            private:
                bool by_levels;
                DepthFirst::Walk<Node> depth;
                LevelOrder::Walk<Node> levels;
            public:
                // Невыбранный обход получает пустой корень и ничего не обходит
                Walk(const Node& root, const ListedTraversal& traversal)
                    : by_levels(traversal.is_level_order()),
                      depth(by_levels ? Node() : root, traversal.depth_first()),
                      levels(by_levels ? root : Node(), LevelOrder()) {}

                Node next() { return by_levels ? levels.next() : depth.next(); }
        };
};

// Готовые функции для map / where / reduce, которые можно выбрать в интерфейсе
template <class T>
struct MapPreset {
    const char* name;
    std::function<T(const T&)> func;
};

template <class T>
struct WherePreset {
    const char* name;
    std::function<bool(const T&)> func;
};

template <class T>
struct ReducePreset {
    const char* name;
    std::function<std::string(const BinarySearchTree<T>&, const ListedTraversal&)> run;
};

// Что нужно интерфейсу от типа элементов: генерация, короткая подпись узла, пресеты
template <class T>
struct GuiValue;

namespace gui_detail {
    inline const char* pick(std::mt19937& random, const std::vector<const char*>& items) {
        return items[random() % items.size()];
    }

    inline const std::vector<const char*>& first_names() {
        static const std::vector<const char*> names = {
            "Иван", "Пётр", "Анна", "Мария", "Олег", "Дарья", "Илья", "Ольга"};
        return names;
    }

    inline const std::vector<const char*>& middle_names() {
        static const std::vector<const char*> names = {
            "Иванович", "Петрович", "Сергеевна", "Андреевна", "Олегович", "Ильич"};
        return names;
    }

    inline const std::vector<const char*>& last_names() {
        static const std::vector<const char*> names = {
            "Иванов", "Петров", "Смирнова", "Кузнецов", "Попова", "Соколов", "Лебедева",
            "Козлов", "Новиков", "Морозова"};
        return names;
    }

    inline Person_ID random_id(std::mt19937& random) {
        return Person_ID{static_cast<int>(1000 + random() % 9000),
                         static_cast<int>(100000 + random() % 900000)};
    }

    template <class T>
    std::string join_labels(const BinarySearchTree<T>& tree, const ListedTraversal& traversal,
                            std::function<std::string(const T&)> label) {
        return tree.reduce([&](const T& elem, std::string acc) {
            return acc.empty() ? label(elem) : acc + " " + label(elem);
        }, std::string(), traversal);
    }
}

template <>
struct GuiValue<int> {
    static constexpr const char* TYPE_NAME = "Целые числа";
    static constexpr const char* INPUT_HINT = "например 42";

    static int random(std::mt19937& random, int range) {
        return std::uniform_int_distribution<int>(0, range)(random);
    }

    static std::string label(const int& value) { return std::to_string(value); }

    static std::vector<MapPreset<int>> map_presets() {
        return {{"x * 2", [](const int& x) { return x * 2; }},
                {"x + 1", [](const int& x) { return x + 1; }},
                {"-x", [](const int& x) { return -x; }},
                {"x % 10 (схлопывает повторы)", [](const int& x) { return x % 10; }}};
    }

    static std::vector<WherePreset<int>> where_presets() {
        return {{"чётные", [](const int& x) { return x % 2 == 0; }},
                {"нечётные", [](const int& x) { return x % 2 != 0; }},
                {"x < 50", [](const int& x) { return x < 50; }}};
    }

    static std::vector<ReducePreset<int>> reduce_presets() {
        return {
            {"сумма", [](const BinarySearchTree<int>& tree, const ListedTraversal& traversal) {
                long long sum = tree.reduce(
                    [](const int& x, long long acc) { return acc + x; }, 0LL, traversal);
                return std::to_string(sum);
            }},
            {"f(x, a) = 2x + 3a, c = 4 (из задания)",
             [](const BinarySearchTree<int>& tree, const ListedTraversal& traversal) {
                double result = tree.reduce(
                    [](const int& x, double acc) { return 2.0 * x + 3.0 * acc; }, 4.0, traversal);
                return ValueCodec<double>::to_string(result);
            }},
            {"запись в порядке обхода",
             [](const BinarySearchTree<int>& tree, const ListedTraversal& traversal) {
                return gui_detail::join_labels<int>(tree, traversal, label);
            }}};
    }
};

template <>
struct GuiValue<double> {
    static constexpr const char* TYPE_NAME = "Вещественные числа";
    static constexpr const char* INPUT_HINT = "например -1.25";

    static double random(std::mt19937& random, int range) {
        double value = std::uniform_real_distribution<double>(-range, range)(random);
        return std::round(value * 100.0) / 100.0;
    }

    static std::string label(const double& value) { return ValueCodec<double>::to_string(value); }

    static std::vector<MapPreset<double>> map_presets() {
        return {{"x / 2", [](const double& x) { return x / 2.0; }},
                {"-x", [](const double& x) { return -x; }},
                {"|x|", [](const double& x) { return std::fabs(x); }},
                {"округлить", [](const double& x) { return std::round(x); }}};
    }

    static std::vector<WherePreset<double>> where_presets() {
        return {{"x > 0", [](const double& x) { return x > 0; }},
                {"|x| < 10", [](const double& x) { return std::fabs(x) < 10; }}};
    }

    static std::vector<ReducePreset<double>> reduce_presets() {
        return {{"сумма", [](const BinarySearchTree<double>& tree, const ListedTraversal& traversal) {
            double sum = tree.reduce([](const double& x, double acc) { return acc + x; },
                                     0.0, traversal);
            return ValueCodec<double>::to_string(sum);
        }}};
    }
};

template <>
struct GuiValue<std::string> {
    static constexpr const char* TYPE_NAME = "Строки";
    static constexpr const char* INPUT_HINT = "любой текст";

    static std::string random(std::mt19937& random, int) {
        std::string word(3 + random() % 5, 'a');
        for (char& symbol : word) symbol = static_cast<char>('a' + random() % 26);
        return word;
    }

    static std::string label(const std::string& value) { return value; }

    static std::vector<MapPreset<std::string>> map_presets() {
        return {{"В ВЕРХНИЙ РЕГИСТР", [](const std::string& text) {
                    std::string result = text;
                    for (char& symbol : result) {
                        symbol = static_cast<char>(std::toupper(static_cast<unsigned char>(symbol)));
                    }
                    return result;
                }},
                {"перевернуть", [](const std::string& text) {
                    return std::string(text.rbegin(), text.rend());
                }},
                {"первая буква", [](const std::string& text) { return text.substr(0, 1); }}};
    }

    static std::vector<WherePreset<std::string>> where_presets() {
        return {{"длина > 4", [](const std::string& text) { return text.size() > 4; }},
                {"содержит 'a'", [](const std::string& text) {
                    return text.find('a') != std::string::npos;
                }}};
    }

    static std::vector<ReducePreset<std::string>> reduce_presets() {
        return {{"склеить в порядке обхода",
                 [](const BinarySearchTree<std::string>& tree, const ListedTraversal& traversal) {
                     return gui_detail::join_labels<std::string>(tree, traversal, label);
                 }},
                {"суммарная длина",
                 [](const BinarySearchTree<std::string>& tree, const ListedTraversal& traversal) {
                     long long total = tree.reduce(
                         [](const std::string& text, long long acc) {
                             return acc + static_cast<long long>(text.size());
                         }, 0LL, traversal);
                     return std::to_string(total);
                 }}};
    }
};

template <>
struct GuiValue<Complex> {
    static constexpr const char* TYPE_NAME = "Комплексные числа";
    static constexpr const char* INPUT_HINT = "a+bi, например 1.5-2i";

    static Complex random(std::mt19937& random, int range) {
        std::uniform_int_distribution<int> part(-range, range);
        return Complex(part(random) / 10.0, part(random) / 10.0);
    }

    static std::string label(const Complex& value) { return ValueCodec<Complex>::to_string(value); }

    static std::vector<MapPreset<Complex>> map_presets() {
        return {{"сопряжённое", [](const Complex& z) {
                    return Complex(z.get_real(), -z.get_imag());
                }},
                {"z * i", [](const Complex& z) { return Complex(-z.get_imag(), z.get_real()); }},
                {"z * 2", [](const Complex& z) {
                    return Complex(2 * z.get_real(), 2 * z.get_imag());
                }}};
    }

    static std::vector<WherePreset<Complex>> where_presets() {
        return {{"|z| > 5", [](const Complex& z) { return z.modulus() > 5; }},
                {"Re z > 0", [](const Complex& z) { return z.get_real() > 0; }}};
    }

    static std::vector<ReducePreset<Complex>> reduce_presets() {
        return {{"сумма", [](const BinarySearchTree<Complex>& tree, const ListedTraversal& traversal) {
            Complex sum = tree.reduce([](const Complex& z, Complex acc) {
                return Complex(acc.get_real() + z.get_real(), acc.get_imag() + z.get_imag());
            }, Complex(), traversal);
            return ValueCodec<Complex>::to_string(sum);
        }}};
    }
};

template <>
struct GuiValue<Student> {
    static constexpr const char* TYPE_NAME = "Студенты";
    static constexpr const char* INPUT_HINT = "серия номер имя отчество фамилия зачётка группа";

    static Student random(std::mt19937& random, int) {
        static const std::vector<const char*> groups = {"Б24-504", "Б24-505", "Б23-101"};
        return Student(gui_detail::random_id(random),
                       gui_detail::pick(random, gui_detail::first_names()),
                       gui_detail::pick(random, gui_detail::middle_names()),
                       gui_detail::pick(random, gui_detail::last_names()),
                       static_cast<int>(10000 + random() % 90000),
                       gui_detail::pick(random, groups));
    }

    static std::string label(const Student& value) { return value.get_last_name(); }

    static std::vector<MapPreset<Student>> map_presets() {
        return {{"перевести в группу Б24-504", [](const Student& s) {
            return Student(s.get_id(), s.get_first_name(), s.get_middle_name(),
                           s.get_last_name(), s.get_grade_book_num(), "Б24-504");
        }}};
    }

    static std::vector<WherePreset<Student>> where_presets() {
        return {{"группа Б24-504", [](const Student& s) {
                    return std::string(s.get_group_id()) == "Б24-504";
                }},
                {"чётная серия", [](const Student& s) { return s.get_id().series % 2 == 0; }}};
    }

    static std::vector<ReducePreset<Student>> reduce_presets() {
        return {{"фамилии в порядке обхода",
                 [](const BinarySearchTree<Student>& tree, const ListedTraversal& traversal) {
                     return gui_detail::join_labels<Student>(tree, traversal, label);
                 }}};
    }
};

template <>
struct GuiValue<Teacher> {
    static constexpr const char* TYPE_NAME = "Преподаватели";
    static constexpr const char* INPUT_HINT = "серия номер имя отчество фамилия кафедра должность";

    static Teacher random(std::mt19937& random, int) {
        static const std::vector<const char*> positions = {"ассистент", "доцент", "профессор"};
        return Teacher(gui_detail::random_id(random),
                       gui_detail::pick(random, gui_detail::first_names()),
                       gui_detail::pick(random, gui_detail::middle_names()),
                       gui_detail::pick(random, gui_detail::last_names()),
                       static_cast<int>(1 + random() % 40),
                       gui_detail::pick(random, positions));
    }

    static std::string label(const Teacher& value) { return value.get_last_name(); }

    static std::vector<MapPreset<Teacher>> map_presets() {
        return {{"повысить до профессора", [](const Teacher& t) {
            return Teacher(t.get_id(), t.get_first_name(), t.get_middle_name(),
                           t.get_last_name(), t.get_depart_num(), "профессор");
        }}};
    }

    static std::vector<WherePreset<Teacher>> where_presets() {
        return {{"доценты", [](const Teacher& t) {
                    return std::string(t.get_position()) == "доцент";
                }},
                {"кафедра <= 20", [](const Teacher& t) { return t.get_depart_num() <= 20; }}};
    }

    static std::vector<ReducePreset<Teacher>> reduce_presets() {
        return {{"фамилии в порядке обхода",
                 [](const BinarySearchTree<Teacher>& tree, const ListedTraversal& traversal) {
                     return gui_detail::join_labels<Teacher>(tree, traversal, label);
                 }}};
    }
};

#endif
