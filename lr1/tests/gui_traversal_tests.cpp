#include "gui/gui_values.h"
#include <gtest/gtest.h>
#include <vector>

// Стратегия из списка интерфейса сама собрана из готовых: по номеру отдаёт перебор
// DepthFirst с нужным порядком или LevelOrder
TEST(BstTraversal, ListedTraversalDelegates) {
    int items[] = {4, 2, 6, 1, 3, 5, 7};
    BinarySearchTree<int> tree(items, 7);
    auto collect = [](UnqPtr<IEnumerator<int>> iter) {
        std::vector<int> values;
        while (iter->move_next()) values.push_back(iter->get_current());
        return values;
    };
    for (int index = 0; index < gui_detail::TRAVERSAL_COUNT; index++) {
        std::vector<int> expected = index == gui_detail::LEVEL_ORDER_INDEX
            ? collect(tree.get_enumerator(LevelOrder()))
            : collect(tree.get_enumerator(
                  DepthFirst(gui_detail::TRAVERSAL_NAMES[index])));
        EXPECT_EQ(collect(tree.get_enumerator(ListedTraversal(index))), expected) << index;
    }
}

