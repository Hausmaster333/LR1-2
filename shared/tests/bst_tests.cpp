#include "tree/bst.h"
#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <utility>
#include "core/array_sequence.h"

namespace {
    BinarySearchTree<int> make_tree(std::initializer_list<int> items) {
        BinarySearchTree<int> tree;
        for (int item : items) tree.insert(item);
        return tree;
    }

    ArraySequence<int> collect(UnqPtr<IEnumerator<int>> iter) {
        ArraySequence<int> result;
        while (iter->move_next()) result.push_back(iter->get_current());
        return result;
    }

    ArraySequence<int> to_sequence(const DynamicArray<int>& array) {
        ArraySequence<int> result;
        for (int idx = 0; idx < array.get_size(); idx++) result.push_back(array.get(idx));
        return result;
    }

    bool height_is_avl(int height, int count) {
        return height <= 1.45 * std::log2(count + 2.0);
    }

    template <class Node>
    class RightSpine {
        private:
            Node node;
        public:
            RightSpine(const Node& root) : node(root) {}

            Node next() {
                Node current = node;
                if (node) node = node.right();
                return current;
            }
    };

    template <class Node>
    class MyBFS {
        private:
            DynamicArray<Node> queue{1};
            int head = 0;
            int tail = 0;

            void enqueue(Node node) {
                if (!node) return;
                
                if (tail == queue.get_size())
                    queue.resize(2 * tail);
                queue.set(tail++, std::move(node));
            }

        public:
            MyBFS(const Node& root) {
                enqueue(root);
            }

            Node next() {
                if (head == tail) return {};

                Node current = queue.get(head);
                queue.set(head++, Node{});
                enqueue(current.left());
                enqueue(current.right());

                return current;
            }
    };

    template <class Node>
    class OnlyLeft {
        private:
            Node node;
        
        public:
            OnlyLeft(const Node& root) : node(root) {}

            Node next() {
                Node current = node;
                if (node)
                    node = current.left();
                return current;
            }
    };

    template <class Node>
    class LeavesOnly {
        private:
            DFS<Node> walker;
        public:
            LeavesOnly(const Node& root) : walker(root) {}

            Node next() {
                while (Node node = walker.next()) {
                    if (!node.left() && !node.right()) return node;
                }
                return Node();
            }
    };
}

// ---------- Базовые операции ----------

TEST(BstBasic, EmptyTree) {
    BinarySearchTree<int> tree;
    EXPECT_EQ(tree.get_count(), 0);
    EXPECT_EQ(tree.get_height(), 0);
    EXPECT_FALSE(tree.contains(1));
    EXPECT_FALSE(tree.remove(1));
    EXPECT_THROW(tree.get(0), std::out_of_range);
    EXPECT_EQ(tree.to_string(), "");
}

TEST(BstBasic, InsertAndContains) {
    BinarySearchTree<int> tree = make_tree({5, 3, 8, 1, 4});
    EXPECT_EQ(tree.get_count(), 5);
    EXPECT_TRUE(tree.contains(4));
    EXPECT_FALSE(tree.contains(7));
}

TEST(BstBasic, DuplicatesAreRejected) {
    BinarySearchTree<int> tree = make_tree({1, 2});
    EXPECT_FALSE(tree.insert(2));
    EXPECT_EQ(tree.get_count(), 2);
}

TEST(BstBasic, RemoveNodeWithTwoChildren) {
    BinarySearchTree<int> tree = make_tree({5, 3, 8, 1, 4});
    EXPECT_TRUE(tree.remove(3)); // У 3 дети 1 и 4
    EXPECT_FALSE(tree.contains(3));
    EXPECT_EQ(to_sequence(tree.to_array()), (ArraySequence<int>{1, 4, 5, 8}));
}

TEST(BstBasic, RemovingRootKeepsSuccessorsRightChild) {
    auto tree = make_tree({8, 4, 14, 2, 6, 10, 16, 11});
    EXPECT_TRUE(tree.remove(8)); // На место 8 переходит 10, его прежнее место занимает 11
    EXPECT_EQ(to_sequence(tree.to_array()), (ArraySequence<int>{2, 4, 6, 10, 11, 14, 16}));
    EXPECT_EQ(tree.index_of(11), 4);
}

TEST(BstBasic, RemoveLeafAndNodeWithOneChild) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3, 4});
    EXPECT_TRUE(tree.remove(4)); // Лист
    EXPECT_TRUE(tree.remove(1));
    EXPECT_TRUE(tree.remove(2)); // Остался один ребёнок 3
    EXPECT_EQ(to_sequence(tree.to_array()), (ArraySequence<int>{3}));
    EXPECT_TRUE(tree.remove(3));
    EXPECT_EQ(tree.get_count(), 0);
}

TEST(BstBasic, TryFind) {
    BinarySearchTree<int> tree = make_tree({1, 2, 3});
    EXPECT_TRUE(tree.try_find(2).has_value());
    EXPECT_EQ(tree.try_find(2).get_value(), 2);
    EXPECT_FALSE(tree.try_find(9).has_value());
}

TEST(BstBasic, ConstructFromArraySkipsDuplicates) {
    int items[] = {3, 1, 2, 3};
    BinarySearchTree<int> tree(items, 4);
    EXPECT_EQ(tree.get_count(), 3);
}

TEST(BstBasic, ConstructFromBadArrayThrows) {
    int items[] = {1, 2};
    EXPECT_THROW(BinarySearchTree<int>(items, -1), std::invalid_argument);
    EXPECT_THROW(BinarySearchTree<int>(nullptr, 2), std::invalid_argument);
    EXPECT_NO_THROW(BinarySearchTree<int>(nullptr, 0));
}

TEST(BstBasic, GetByIndexInSortedOrder) {
    BinarySearchTree<int> tree = make_tree({50, 10, 40, 20, 30});
    EXPECT_EQ(tree.get(0), 10);
    EXPECT_EQ(tree.get(1), 20);
    EXPECT_EQ(tree.get(2), 30);
    EXPECT_EQ(tree.get(3), 40);
    EXPECT_EQ(tree.get(4), 50);
    EXPECT_THROW(tree.get(5), std::out_of_range);
    EXPECT_THROW(tree.get(-1), std::out_of_range);
}

TEST(BstBasic, ClearDropsEverything) {
    BinarySearchTree<int> tree = make_tree({1, 2, 3});
    tree.clear();
    EXPECT_EQ(tree.get_count(), 0);
    EXPECT_TRUE(tree.insert(1));
}

// ---------- Балансировка ----------

// Без балансировки отсортированный ввод дал бы высоту 1000
TEST(BstBalance, SortedInsertStaysLogarithmic) {
    BinarySearchTree<int> tree;
    for (int value = 0; value < 1000; value++) tree.insert(value);
    EXPECT_EQ(tree.get_height(), 10); // Меньше нельзя: в 9 уровней влезает только 511 узлов
}

TEST(BstBalance, AllRotationCases) {
    EXPECT_EQ(make_tree({3, 2, 1}).get_height(), 2); // Лево-лево
    EXPECT_EQ(make_tree({1, 2, 3}).get_height(), 2); // Право-право
    EXPECT_EQ(make_tree({3, 1, 2}).get_height(), 2); // Лево-право
    EXPECT_EQ(make_tree({1, 3, 2}).get_height(), 2); // Право-лево
}

// Случайные вставки и удаления повторяем на std::set: ответы и итог должны совпасть
TEST(BstBalance, RandomOperationsMatchStdSet) {
    std::mt19937 random(42);
    std::uniform_int_distribution<int> values(0, 2000);
    BinarySearchTree<int> tree;
    std::set<int> reference;

    for (int step = 0; step < 20000; step++) {
        int value = values(random);
        if (random() % 3 == 0) {
            EXPECT_EQ(tree.remove(value), reference.erase(value) == 1);
        } else {
            EXPECT_EQ(tree.insert(value), reference.insert(value).second);
        }
    }

    ASSERT_EQ(tree.get_count(), static_cast<int>(reference.size()));
    EXPECT_TRUE(height_is_avl(tree.get_height(), tree.get_count()));
    const auto ordered = tree.to_array();
    ASSERT_EQ(ordered.get_size(), static_cast<int>(reference.size()));
    int index = 0;
    for (int value : reference) EXPECT_EQ(ordered.get(index++), value);
}

// ---------- Обходы и перечислители ----------

//         4
//      2     6
//     1 3   5 7
// Перечислитель и to_array должны давать один и тот же порядок
TEST(BstOrders, AllSixOrdersAgree) {
    struct Case {
        const char* order;
        ArraySequence<int> expected;
    };
    const Case cases[] = {
        {"КЛП", {4, 2, 1, 3, 6, 5, 7}},
        {"КПЛ", {4, 6, 7, 5, 2, 3, 1}},
        {"ЛПК", {1, 3, 2, 5, 7, 6, 4}},
        {"ЛКП", {1, 2, 3, 4, 5, 6, 7}},
        {"ПЛК", {7, 5, 6, 3, 1, 2, 4}},
        {"ПКЛ", {7, 6, 5, 4, 3, 2, 1}},
    };
    for (const Case& test : cases) {
        BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});

        std::string traversal(test.order);

        EXPECT_EQ(collect(tree.get_enumerator<DFS>(traversal)), test.expected) << test.order;
        EXPECT_EQ(to_sequence(tree.to_array<DFS>(traversal)), test.expected) << test.order;
    }
}

// ---------- Свои стратегии обхода ----------

// Другая стратегия, тем же параметром: по уровням. После remove(1) у 2 нет левого сына:
//         4
//      2     6
//       3   5 7
TEST(BstTraversal, BFS) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    EXPECT_EQ(collect(tree.get_enumerator<BFS>()), (ArraySequence<int>{4, 2, 6, 1, 3, 5, 7}));
    tree.remove(1);
    EXPECT_EQ(collect(tree.get_enumerator<BFS>()), (ArraySequence<int>{4, 2, 6, 3, 5, 7}));
    EXPECT_TRUE(collect(BinarySearchTree<int>().get_enumerator<BFS>()).empty());
}

// Стратегии, написанные снаружи дерева, работают так же, как встроенные
TEST(BstTraversal, CustomStrategies) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    EXPECT_EQ(collect(tree.get_enumerator<RightSpine>()), (ArraySequence<int>{4, 6, 7}));
    EXPECT_EQ(collect(tree.get_enumerator<OnlyLeft>()), (ArraySequence<int>{4, 2, 1}));
    EXPECT_EQ(collect(tree.get_enumerator<MyBFS>()), collect(tree.get_enumerator<BFS>()));
    EXPECT_EQ(collect(tree.get_enumerator<LeavesOnly>()), (ArraySequence<int>{1, 3, 5, 7}));
}

// reset() возвращает любую стратегию в начало: перечислитель хранит её начальную копию
TEST(BstTraversal, CustomStrategyResets) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6});
    auto iter = tree.get_enumerator<BFS>();
    while (iter->move_next()) {}
    iter->reset();
    ASSERT_TRUE(iter->move_next());
    EXPECT_EQ(iter->get_current(), 4);
}

// Защита от изменения дерева общая для всех стратегий, своя её не обходит
TEST(BstTraversal, CustomStrategyInvalidatedByInsert) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6});
    auto iter = tree.get_enumerator<RightSpine>();
    ASSERT_TRUE(iter->move_next());
    tree.insert(8);
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

TEST(BstEnumerator, WalksAndResets) {
    BinarySearchTree<int> tree = make_tree({2, 1});
    auto iter = tree.get_enumerator();
    EXPECT_THROW(iter->get_current(), std::logic_error); // До первого move_next() элемента нет
    EXPECT_TRUE(iter->move_next());
    EXPECT_TRUE(iter->move_next());
    EXPECT_FALSE(iter->move_next());

    iter->reset();
    EXPECT_TRUE(iter->move_next());
    EXPECT_EQ(iter->get_current(), 1);
}

TEST(BstEnumerator, EmptyTree) {
    BinarySearchTree<int> empty;
    EXPECT_FALSE(empty.get_enumerator()->move_next());
}

// Перечислитель смотрит на удалённый узел: дальше идти нельзя
TEST(BstEnumerator, RemoveInvalidatesEnumerator) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3});
    auto iter = tree.get_enumerator();
    ASSERT_TRUE(iter->move_next()); // Стоит на 1
    tree.remove(1);
    EXPECT_THROW(iter->get_current(), std::logic_error);
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

TEST(BstEnumerator, FailedInsertKeepsEnumerator) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3});
    auto iter = tree.get_enumerator();
    EXPECT_FALSE(tree.insert(2)); // Дерево не изменилось
    EXPECT_TRUE(iter->move_next());
}

TEST(BstEnumerator, DestroyedTreeInvalidatesEnumerator) {
    UnqPtr<IEnumerator<int>> iter;
    {
        BinarySearchTree<int> tree = make_tree({1, 2});
        iter = tree.get_enumerator();
    }
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

TEST(BstEnumerator, AssignmentInvalidatesEnumerator) {
    BinarySearchTree<int> tree = make_tree({1, 2});
    auto iter = tree.get_enumerator();
    tree = make_tree({3}); // Старые узлы уничтожены
    EXPECT_THROW(iter->move_next(), std::logic_error);
}

// Узлы переехали в другое дерево целиком, перечислитель идёт по ним дальше
TEST(BstEnumerator, MoveKeepsEnumeratorValid) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3});
    auto iter = tree.get_enumerator();
    BinarySearchTree<int> moved = std::move(tree);
    EXPECT_EQ(collect(std::move(iter)), (ArraySequence<int>{1, 2, 3}));
}

TEST(BstEnumerator, MovedFromTreeIsEmptyAndUsable) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3});
    BinarySearchTree<int> moved = std::move(tree);
    EXPECT_FALSE(tree.get_enumerator()->move_next());
    tree.insert(5);
    EXPECT_EQ(collect(tree.get_enumerator()), (ArraySequence<int>{5}));
}

TEST(TraversalOrderParse, LatinAliases) {
    EXPECT_EQ(TraversalOrder::from_string("NLR"), TraversalOrder::from_string("КЛП"));
    EXPECT_EQ(TraversalOrder::from_string("LKP").to_string(), "ЛКП");
}

TEST(TraversalOrderParse, BadOrdersThrow) {
    EXPECT_THROW(TraversalOrder::from_string("КК П"), std::invalid_argument); // Пробел
    EXPECT_THROW(TraversalOrder::from_string("КЛ"), std::invalid_argument);   // Нет П
    EXPECT_THROW(TraversalOrder::from_string("КЛПК"), std::invalid_argument); // Лишняя буква
    EXPECT_THROW(TraversalOrder::from_string("ККП"), std::invalid_argument);  // К дважды
    EXPECT_THROW(BinarySearchTree<int>().get_enumerator<DFS>("КЛ"), std::invalid_argument); // Стратегия проверяет порядок так же
}

TEST(DynamicArrayEnumerator, WalksArray) {
    int items[] = {1, 2, 3};
    DynamicArray<int> array(items, 3);
    EXPECT_EQ(collect(array.get_enumerator()), (ArraySequence<int>{1, 2, 3}));
    EXPECT_TRUE(collect(DynamicArray<int>().get_enumerator()).empty());
}

// Перечислитель читает через get(), поэтому после resize() не смотрит в старую память
TEST(DynamicArrayEnumerator, SurvivesResize) {
    int items[] = {1, 2, 3};
    DynamicArray<int> array(items, 3);
    auto iter = array.get_enumerator();
    EXPECT_THROW(iter->get_current(), std::out_of_range);
    ASSERT_TRUE(iter->move_next()); // Стоит на 1

    array.resize(5);
    array.set(4, 9);
    ASSERT_TRUE(iter->move_next());
    EXPECT_EQ(iter->get_current(), 2);

    array.resize(2); // Массив кончился на текущем элементе
    EXPECT_FALSE(iter->move_next());
    EXPECT_FALSE(iter->move_next());
    EXPECT_THROW(iter->get_current(), std::out_of_range);

    iter->reset();
    ASSERT_TRUE(iter->move_next());
    EXPECT_EQ(iter->get_current(), 1);
}

// set(T&&) забирает ShrdPtr, а не копирует: копия оставила бы исходный указатель
// непустым и подняла бы счётчик до 2
TEST(DynamicArrayMove, SetTakesValue) {
    DynamicArray<ShrdPtr<int>> array(1);
    auto value = ShrdPtr<int>::make(5);
    array.set(0, std::move(value));
    EXPECT_FALSE(value);
    EXPECT_EQ(array.get(0).use_count(), 1u);
}

// resize перемещает элементы в новый буфер и ничего не теряет
TEST(DynamicArrayMove, ResizeKeepsMovedElements) {
    DynamicArray<ShrdPtr<int>> array(1);
    array.set(0, ShrdPtr<int>::make(5));
    array.resize(4);
    EXPECT_EQ(*array.get(0), 5);
    EXPECT_EQ(array.get(0).use_count(), 1u);
    EXPECT_FALSE(array.get(3));
}

// ---------- map / where / reduce ----------

TEST(BstFunctional, ReduceExampleFromTask) {
    // f(x1, x2) = 2*x1 + 3*x2, reduce(f, [1, 2, 3], 4) = f(3, f(2, f(1, 4))).
    // В задании ответ 132 из-за арифметической ошибки: 2*2 + 3*14 = 46, а не 42.
    // Верно: f(1, 4) = 14, f(2, 14) = 46, f(3, 46) = 144
    BinarySearchTree<int> tree = make_tree({1, 2, 3});
    int result = tree.reduce([](int elem, int acc) { return 2 * elem + 3 * acc; }, 4);
    EXPECT_EQ(result, 144);
}

TEST(BstFunctional, ReduceRespectsOrderAndType) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3});
    auto concat = [](int elem, std::string acc) { return acc + std::to_string(elem); };
    EXPECT_EQ(tree.reduce(concat, std::string()), "123");
    EXPECT_EQ(tree.reduce<DFS>(concat, std::string(), "КПЛ"), "231");
    EXPECT_EQ(BinarySearchTree<int>().reduce(concat, std::string("x")), "x");
}

// Обход в reduce и to_array задаётся так же, как в перечислителе: любой стратегией
TEST(BstFunctional, ReduceAndToArrayTakeAnyStrategy) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    auto concat = [](int elem, std::string acc) { return acc + std::to_string(elem); };
    EXPECT_EQ(tree.reduce<BFS>(concat, std::string()), "4261357");
    EXPECT_EQ(tree.reduce<RightSpine>(concat, std::string()), "467");
    EXPECT_EQ(to_sequence(tree.to_array<BFS>()), (ArraySequence<int>{4, 2, 6, 1, 3, 5, 7}));
    EXPECT_EQ(to_sequence(tree.to_array<LeavesOnly>()), (ArraySequence<int>{1, 3, 5, 7}));
}

// Аккумулятор передаётся перемещением: подходит и некопируемый
TEST(BstFunctional, ReduceWithMoveOnlyAccumulator) {
    BinarySearchTree<int> tree = make_tree({1, 2, 3});
    UnqPtr<int> sum = tree.reduce([](int elem, UnqPtr<int> acc) {
        *acc += elem;
        return acc;
    }, UnqPtr<int>::make(0));
    EXPECT_EQ(*sum, 6);
}

// map строит дерево заново: порядок по новым значениям, совпавшие значения сливаются
TEST(BstFunctional, MapRebuildsOrderAndMergesDuplicates) {
    BinarySearchTree<int> tree = make_tree({-2, -1, 0, 1, 2});
    BinarySearchTree<int> negated = tree.map([](int value) { return -value; });
    EXPECT_EQ(negated, tree);

    BinarySearchTree<int> squares = tree.map([](int value) { return value * value; });
    EXPECT_EQ(to_sequence(squares.to_array()), (ArraySequence<int>{0, 1, 4}));
}

TEST(BstFunctional, MapChangesType) {
    BinarySearchTree<int> tree = make_tree({1, 2});
    BinarySearchTree<double> halves = tree.map([](int value) { return value / 2.0; });
    EXPECT_DOUBLE_EQ(halves.get(0), 0.5);
    EXPECT_DOUBLE_EQ(halves.get(1), 1.0);
}

TEST(BstFunctional, MapOwnsReferenceResultsAndSkipsEmptyTree) {
    BinarySearchTree<std::string> tree;
    int calls = 0;
    auto identity = [&](const std::string& value) -> const std::string& {
        ++calls;
        return value;
    };
    EXPECT_EQ(tree.map(identity).get_count(), 0);
    EXPECT_EQ(calls, 0);

    tree.insert("one");
    auto copy = tree.map(identity);
    tree.clear();
    EXPECT_EQ(copy.get(0), "one");
    EXPECT_EQ(calls, 1);
}

TEST(BstFunctional, WhereIsBalanced) {
    BinarySearchTree<int> tree;
    for (int value = 0; value < 1000; value++) tree.insert(value);
    BinarySearchTree<int> even = tree.where([](int value) { return value % 2 == 0; });
    EXPECT_EQ(even.get_count(), 500);
    EXPECT_EQ(even.get(1), 2);
    EXPECT_TRUE(height_is_avl(even.get_height(), even.get_count()));
    EXPECT_EQ(tree.where([](int) { return false; }).get_count(), 0);
}

// ---------- Поддеревья ----------

TEST(BstSubtree, ExtractByRoot) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    BinarySearchTree<int> left = tree.get_subtree(2);
    EXPECT_EQ(to_sequence(left.to_array()), (ArraySequence<int>{1, 2, 3}));
    EXPECT_EQ(tree.get_subtree(7).get_count(), 1);
    EXPECT_THROW(tree.get_subtree(100), std::invalid_argument);
}

TEST(BstSubtree, ContainsSubtree) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    EXPECT_TRUE(tree.contains_subtree(tree.get_subtree(6)));
    EXPECT_TRUE(tree.contains_subtree(tree));
    EXPECT_TRUE(tree.contains_subtree(BinarySearchTree<int>()));
}

TEST(BstSubtree, DoesNotContainOtherTrees) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    EXPECT_FALSE(tree.contains_subtree(make_tree({2, 1}))); // В дереве у 2 есть ещё ребёнок 3
    EXPECT_FALSE(tree.contains_subtree(make_tree({8})));
    EXPECT_FALSE(tree.contains_subtree(make_tree({7}).map([](int v) { return v + 1; })));
}

// ---------- Строки ----------

TEST(BstString, DefaultFormat) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3});
    EXPECT_EQ(tree.to_string(), "{2}({1}()[])[{3}()[]]");
}

TEST(BstString, CustomFormatAndRoundTrip) {
    BinarySearchTree<int> tree = make_tree({2, 1, 3});
    TraversalFormat format("<Л>П|К|");
    EXPECT_EQ(format.get_order().to_string(), "ЛПК");
    std::string text = tree.to_string(format);
    EXPECT_EQ(text, "<<>|1|><>|3||2|");

    BinarySearchTree<int> restored = BinarySearchTree<int>::from_string(text, format);
    EXPECT_EQ(restored.to_string(format), text);
}

TEST(BstString, RoundTripLargeTreeInSeveralFormats) {
    BinarySearchTree<int> tree;
    for (int value = 0; value < 2000; value += 3) tree.insert(value);
    for (const char* pattern : {"{К}(Л)[П]", "{Л}(К)[П]", "(П)[Л]<К>", "{N}(L)[R]"}) {
        TraversalFormat format(pattern);
        BinarySearchTree<int> restored =
            BinarySearchTree<int>::from_string(tree.to_string(format), format);
        EXPECT_EQ(restored.to_string(), tree.to_string()) << pattern;
    }
}

TEST(BstString, EmptyString) {
    EXPECT_EQ(BinarySearchTree<int>::from_string("").get_count(), 0);
}

// Строка описывает вырожденную цепочку 1-2-3-4, а дерево всё равно сбалансировано
TEST(BstString, UnbalancedInputIsRebalanced) {
    BinarySearchTree<int> tree =
        BinarySearchTree<int>::from_string("{1}()[{2}()[{3}()[{4}()[]]]]");
    EXPECT_EQ(tree.get_count(), 4);
    EXPECT_EQ(tree.get_height(), 3);
    EXPECT_EQ(to_sequence(tree.to_array()), (ArraySequence<int>{1, 2, 3, 4}));
}

TEST(BstString, MalformedInput) {
    using Tree = BinarySearchTree<int>;
    EXPECT_THROW(Tree::from_string("{1}("), std::invalid_argument);         // Оборвана
    EXPECT_THROW(Tree::from_string("{1}()[]x"), std::invalid_argument);     // Хвост после дерева
    EXPECT_THROW(Tree::from_string("1"), std::invalid_argument);            // Без скобок
    EXPECT_THROW(Tree::from_string("{x}()[]"), std::invalid_argument);      // Не число
    EXPECT_THROW(Tree::from_string("{}()[]"), std::invalid_argument);       // Пустое значение
    EXPECT_THROW(Tree::from_string("{2}({3}()[])[]"), std::invalid_argument); // Слева больше корня
    EXPECT_THROW(Tree::from_string("{1}({1}()[])[]"), std::invalid_argument); // Повтор
}

// Разбор рекурсивный: без предела глубины злой ввод переполнил бы стек
TEST(BstString, TooDeepInput) {
    std::string text;
    for (int depth = 0; depth < 1100; depth++) text += "{" + std::to_string(depth) + "}()[";
    for (int depth = 0; depth < 1100; depth++) text += "]";
    EXPECT_THROW(BinarySearchTree<int>::from_string(text), std::length_error);
}

// Скобки и '\' внутри строк экранируются и не ломают разбор
TEST(BstString, StringValuesAreEscaped) {
    BinarySearchTree<std::string> tree;
    for (const char* value : {"a{b", "x\\y", "}(", "", "plain"}) tree.insert(value);
    std::string text = tree.to_string();
    BinarySearchTree<std::string> restored = BinarySearchTree<std::string>::from_string(text);
    EXPECT_EQ(restored, tree);
    EXPECT_TRUE(restored.contains("}("));
    EXPECT_TRUE(restored.contains(""));
}

TEST(TraversalFormatParse, Validation) {
    EXPECT_THROW(TraversalFormat("КЛП"), std::invalid_argument);         // Нет ограничителей
    EXPECT_THROW(TraversalFormat("{К}(Л)"), std::invalid_argument);      // Нет П
    EXPECT_THROW(TraversalFormat("{К}(Л)[П]{К}"), std::invalid_argument); // К дважды
    EXPECT_THROW(TraversalFormat("(К)Л(П)"), std::invalid_argument);     // Неоднозначно
    EXPECT_THROW(TraversalFormat("{К}\\Л/[П]"), std::invalid_argument);  // '\' занят экранированием
    EXPECT_EQ(TraversalFormat("{N}(L)[R]").to_string(), "{К}(Л)[П]");
}

// ---------- Операторы и копирование ----------

TEST(BstOperators, PlusMinusAndIndex) {
    BinarySearchTree<int> tree;
    tree += 3;
    tree += 1;
    tree += 2;
    tree -= 3;
    EXPECT_EQ(tree.get_count(), 2);
    EXPECT_EQ(tree[0], 1);
    EXPECT_EQ(tree[1], 2);
}

// += и -= возвращают само дерево, как у встроенных типов
TEST(BstOperators, PlusMinusChain) {
    BinarySearchTree<int> tree;
    (tree += 1) -= 1;
    EXPECT_EQ(tree.get_count(), 0);
}

TEST(BstOperators, EqualityIgnoresInsertOrder) {
    EXPECT_EQ(make_tree({1, 2, 3}), make_tree({3, 2, 1}));
    EXPECT_NE(make_tree({1, 2, 3}), make_tree({1, 2}));
    EXPECT_NE(make_tree({1, 2, 3}), make_tree({1, 2, 4}));
}

TEST(BstCopy, CopyIsDeep) {
    BinarySearchTree<int> original = make_tree({1, 2, 3});
    BinarySearchTree<int> copy = original;
    copy.insert(4);
    EXPECT_EQ(original.get_count(), 3);
}

TEST(BstCopy, CopyAssignmentAndSelfAssignment) {
    BinarySearchTree<int> source = make_tree({1, 2, 3});
    BinarySearchTree<int> assigned;
    assigned = source;
    assigned = assigned;
    EXPECT_EQ(assigned, source);
}

TEST(BstCopy, MoveEmptiesSource) {
    BinarySearchTree<int> source = make_tree({1, 2, 3, 4});
    BinarySearchTree<int> moved = std::move(source);
    EXPECT_EQ(moved.get_count(), 4);
    EXPECT_EQ(source.get_count(), 0);
}

TEST(BstCollection, WorksThroughInterfaces) {
    BinarySearchTree<int> tree = make_tree({3, 1, 2});
    const ICollection<int>& collection = tree;
    EXPECT_EQ(collection.get_count(), 3);
    EXPECT_EQ(collection.get(2), 3);
    EXPECT_TRUE(collection.contains(1));

    const IEnumerable<int>& enumerable = tree;
    EXPECT_EQ(collect(enumerable.get_enumerator()), (ArraySequence<int>{1, 2, 3}));
}

// ---------- Раскладка для GUI ----------

TEST(BstLayout, IndexOf) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    EXPECT_EQ(tree.index_of(1), 0);
    EXPECT_EQ(tree.index_of(6), 5);
    EXPECT_EQ(tree.index_of(100), -1);
}

//         4            глубина 0
//      2     6         глубина 1
//     1 3   5 7        глубина 2
// index - номер в порядке ЛКП, у значений 1..7 это value - 1
TEST(BstLayout, VisitNodesGivesDepthAndParent) {
    BinarySearchTree<int> tree = make_tree({4, 2, 6, 1, 3, 5, 7});
    ArraySequence<int> depths(7), parents(7);
    tree.visit_nodes([&](int value, int depth, int index, int parent_index) {
        EXPECT_EQ(index, value - 1);
        depths[index] = depth;
        parents[index] = parent_index;
    });
    EXPECT_EQ(depths, (ArraySequence<int>{2, 1, 2, 0, 2, 1, 2}));
    EXPECT_EQ(parents, (ArraySequence<int>{1, 3, 1, -1, 5, 3, 5}));
}

// ---------- Любые типы ----------

namespace {
    // Ключ умеет только сравниваться через <: ни ==, ни копирования, ни конструктора по умолчанию
    class OnlyLess {
        private:
            int id;
        public:
            explicit OnlyLess(int id) : id(id) {}
            OnlyLess(OnlyLess&&) = default;
            OnlyLess(const OnlyLess&) = delete;

            bool operator<(const OnlyLess& other) const { return id < other.id; }
            int get_id() const { return id; }
    };

    // Значение не умеет ничего, кроме перемещения
    class Resource {
        private:
            std::string name;
        public:
            explicit Resource(std::string name) : name(std::move(name)) {}
            Resource(Resource&&) = default;
            Resource(const Resource&) = delete;

            const std::string& get_name() const { return name; }
            void rename(std::string new_name) { name = std::move(new_name); }
    };
}

TEST(BstAnyType, KeyNeedsOnlyLess) {
    BinarySearchTree<OnlyLess> tree;
    for (int id : {3, 1, 2}) EXPECT_TRUE(tree.insert(OnlyLess(id)));
    EXPECT_FALSE(tree.insert(OnlyLess(2)));
    EXPECT_TRUE(tree.contains_key(OnlyLess(1)));
    EXPECT_EQ(tree.get(0).get_id(), 1);
    EXPECT_EQ(tree.index_of(OnlyLess(3)), 2);

    EXPECT_TRUE(tree.remove(OnlyLess(1)));
    auto digits = [](const OnlyLess& key, int acc) { return acc * 10 + key.get_id(); };
    EXPECT_EQ(tree.reduce(digits, 0), 23);
    EXPECT_EQ(tree.map([](const OnlyLess& key) { return key.get_id() * 2; }).get(1), 6);
}

TEST(BstAnyType, ValueNeedsOnlyMove) {
    BinarySearchTree<int, Resource> tree;
    EXPECT_TRUE(tree.insert({2, Resource("b")}));
    EXPECT_TRUE(tree.insert({1, Resource("a")}));
    EXPECT_FALSE(tree.insert({1, Resource("other")})); // Повтор: дерево не меняется

    tree.find_value(1).rename("A"); // Значение меняется прямо в дереве
    EXPECT_EQ(tree.find(1).value.get_name(), "A");

    tree.insert({3, Resource("c")});
    std::string names;
    auto iter = tree.get_enumerator<DFS>("ПКЛ");
    while (iter->move_next()) names += iter->get_current().value.get_name();
    EXPECT_EQ(names, "cbA");

    BinarySearchTree<int, Resource> moved = std::move(tree);
    EXPECT_TRUE(moved.remove(2));
    EXPECT_EQ(moved.get_count(), 2);
}

// Значением может быть функция: и указатель, и лямбда со своим безымянным типом
TEST(BstAnyType, FunctionsAsValues) {
    BinarySearchTree<std::string, int (*)(int)> pointers;
    pointers.insert({"double", [](int x) { return 2 * x; }});
    pointers.insert({"negate", [](int x) { return -x; }});
    EXPECT_EQ(pointers.find("double").value(21), 42);
    EXPECT_EQ(pointers.find("negate").value(5), -5);

    int offset = 10;
    auto shift = [offset](int x) { return x + offset; }; // Без конструктора по умолчанию и без =
    BinarySearchTree<int, decltype(shift)> closures;
    closures.insert({1, shift});
    EXPECT_EQ(closures.find(1).value(5), 15);
}

TEST(BstBasic, FindReturnsStoredElement) {
    BinarySearchTree<int> tree = make_tree({1, 2, 3});
    EXPECT_EQ(&tree.find(2), &tree.find(2));
    EXPECT_EQ(tree.find(3), 3);
    EXPECT_THROW(tree.find(9), std::out_of_range);
}
