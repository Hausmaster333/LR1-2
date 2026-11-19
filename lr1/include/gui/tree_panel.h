#ifndef TREE_PANEL_H
#define TREE_PANEL_H

#include "bench/tree_benchmark.h"
#include "gui/gui_values.h"
#include "imgui.h"
#include <cstdio>
#include <string>
#include <vector>

class ITreePanel {
    public:
        virtual void draw() = 0;
        virtual const char* type_name() const = 0;
        virtual ~ITreePanel() = default;
};

namespace panel_detail {
    const int MAX_DRAWN_NODES = 5000; // Больше - неразборчиво и медленно рисовать каждый кадр
    const int MAX_LISTED_VALUES = 300;
    const std::size_t MIN_STRING_BUFFER = 64 * 1024;

    inline std::string trim(const char* text) {
        std::string result(text);
        std::size_t begin = result.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return "";
        std::size_t end = result.find_last_not_of(" \t\r\n");
        return result.substr(begin, end - begin + 1);
    }

    template <class Preset>
    bool preset_combo(const char* label, int& index, const std::vector<Preset>& presets) {
        if (presets.empty()) return false;
        return ImGui::Combo(label, &index, [](void* data, int idx) {
            return static_cast<const std::vector<Preset>*>(data)->at(idx).name;
        }, const_cast<std::vector<Preset>*>(&presets), static_cast<int>(presets.size()));
    }
}

template <class T>
class TreePanel : public ITreePanel {
    private:
        struct LogEntry {
            std::string text;
            bool error;
        };

        struct DrawnNode {
            std::string label;
            std::string full;
            int depth;
            int parent;
        };

        BinarySearchTree<T> tree;
        BinarySearchTree<T> second; // Результат map/where/поддерева или дерево для поиска вхождения
        std::string second_title = "пока пусто";
        Option<T> highlighted;
        std::vector<LogEntry> log;
        bool scroll_log = false;

        char value_input[256] = "";
        int index_input = 0;
        int fill_count = 15;
        int fill_range = 99;
        int fill_seed = 1;

        int order_index = gui_detail::IN_ORDER_INDEX;
        std::string traversal_text;
        int thread_index = gui_detail::IN_ORDER_INDEX;
        int threaded_index = gui_detail::IN_ORDER_INDEX; // Каким обходом дерево прошито
        bool show_threads = true;

        std::vector<MapPreset<T>> map_presets = GuiValue<T>::map_presets();
        std::vector<WherePreset<T>> where_presets = GuiValue<T>::where_presets();
        std::vector<ReducePreset<T>> reduce_presets = GuiValue<T>::reduce_presets();
        int map_index = 0;
        int where_index = 0;
        int reduce_index = 0;
        std::string reduce_text;

        char format_input[64] = "{К}(Л)[П]";
        std::vector<char> string_buffer = std::vector<char>(panel_detail::MIN_STRING_BUFFER, '\0');
        float zoom = 1.0f;

        void add_log(const std::string& text, bool error = false) {
            log.push_back({text, error});
            if (log.size() > 300) log.erase(log.begin());
            scroll_log = true;
        }

        template <class F>
        void run_action(const char* name, F action) {
            try {
                action();
            } catch (const std::exception& error) {
                add_log(std::string(name) + ": " + error.what(), true);
            }
        }

        T parse_input() const {
            return ValueCodec<T>::from_string(panel_detail::trim(value_input));
        }

        void set_string_buffer(const std::string& text) {
            std::size_t size = text.size() + panel_detail::MIN_STRING_BUFFER;
            string_buffer.assign(size, '\0');
            text.copy(string_buffer.data(), text.size());
        }

        void set_second(BinarySearchTree<T>&& result, const std::string& title) {
            second = std::move(result);
            second_title = title;
            add_log(title + ": " + std::to_string(second.get_count()) + " элементов");
        }

        // ---------- Управление ----------

        void draw_elements_section() {
            if (!ImGui::CollapsingHeader("Элементы", ImGuiTreeNodeFlags_DefaultOpen)) return;

            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##value", GuiValue<T>::INPUT_HINT, value_input,
                                     sizeof(value_input));
            if (ImGui::Button("Вставить")) {
                run_action("Вставка", [&] {
                    T value = parse_input();
                    int before = tree.get_count();
                    tree += value;
                    highlighted = Option<T>::Some(value);
                    add_log(tree.get_count() > before ? "Вставлено: " + GuiValue<T>::label(value)
                                                      : "Уже есть: " + GuiValue<T>::label(value));
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Удалить")) {
                run_action("Удаление", [&] {
                    T value = parse_input();
                    bool removed = tree.remove(value);
                    highlighted = Option<T>::None();
                    add_log((removed ? "Удалено: " : "Нет элемента: ") + GuiValue<T>::label(value));
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Найти")) {
                run_action("Поиск", [&] {
                    T value = parse_input();
                    highlighted = tree.try_find(value);
                    if (highlighted.has_value()) {
                        add_log("Найдено на позиции " + std::to_string(tree.index_of(value))
                                + ": " + ValueCodec<T>::to_string(highlighted.get_value()));
                    } else {
                        add_log("Не найдено: " + GuiValue<T>::label(value));
                    }
                });
            }

            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("##index", &index_input);
            ImGui::SameLine();
            if (ImGui::Button("tree[i] - i-й по возрастанию")) {
                run_action("Индекс", [&] {
                    const T& value = tree[index_input];
                    highlighted = Option<T>::Some(value);
                    add_log("tree[" + std::to_string(index_input) + "] = "
                            + ValueCodec<T>::to_string(value));
                });
            }
        }

        void draw_fill_section() {
            if (!ImGui::CollapsingHeader("Автозаполнение", ImGuiTreeNodeFlags_DefaultOpen)) return;

            ImGui::InputInt("количество", &fill_count);
            ImGui::InputInt("диапазон", &fill_range);
            ImGui::InputInt("seed", &fill_seed);
            fill_count = std::clamp(fill_count, 1, 2000000);
            fill_range = std::clamp(fill_range, 1, 1000000000);

            if (ImGui::Button("Заполнить случайно")) {
                run_action("Заполнение", [&] {
                    std::mt19937 random(static_cast<unsigned>(fill_seed));
                    BinarySearchTree<T> filled;
                    long long attempts = 0;
                    double elapsed = bench::time_ms([&] {
                        // Повторы отбрасываются, поэтому даём запас попыток
                        while (filled.get_count() < fill_count && attempts < 20LL * fill_count) {
                            filled.insert(GuiValue<T>::random(random, fill_range));
                            attempts++;
                        }
                    });
                    tree = std::move(filled);
                    highlighted = Option<T>::None();
                    char message[160];
                    std::snprintf(message, sizeof(message),
                                  "Случайное дерево: %d элементов, высота %d, %.2f мс",
                                  tree.get_count(), tree.get_height(), elapsed);
                    add_log(message);
                    if (tree.get_count() < fill_count) {
                        add_log("Диапазон слишком мал для такого числа различных элементов",
                                true);
                    }
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Очистить")) {
                tree.clear();
                highlighted = Option<T>::None();
                add_log("Дерево очищено");
            }
        }

        void draw_traversal_section() {
            using namespace panel_detail;
            using namespace gui_detail;
            if (!ImGui::CollapsingHeader("Обходы и прошивка", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            ImGui::SetNextItemWidth(130);
            ImGui::Combo("##order", &order_index, TRAVERSAL_NAMES, TRAVERSAL_COUNT);
            ImGui::SameLine();
            if (ImGui::Button("Обойти")) {
                run_action("Обход", [&] {
                    traversal_text = std::string(TRAVERSAL_NAMES[order_index]) + ": ";
                    auto iter = tree.get_enumerator(ListedTraversal(order_index));
                    int listed = 0;
                    while (iter->move_next() && listed < MAX_LISTED_VALUES) {
                        if (listed++ > 0) traversal_text += ", ";
                        traversal_text += GuiValue<T>::label(iter->get_current());
                    }
                    if (tree.get_count() > listed) {
                        traversal_text += " ... (ещё " + std::to_string(tree.get_count() - listed)
                                        + ")";
                    }
                    add_log("Обход " + std::string(TRAVERSAL_NAMES[order_index]) + " готов");
                });
            }

            ImGui::SetNextItemWidth(130);
            ImGui::Combo("##thread", &thread_index, TRAVERSAL_NAMES, TRAVERSAL_COUNT);
            ImGui::SameLine();
            if (ImGui::Button("Прошить")) {
                run_action("Прошивка", [&] {
                    tree.thread(ListedTraversal(thread_index));
                    threaded_index = thread_index;
                    add_log("Дерево прошито по обходу " + std::string(TRAVERSAL_NAMES[thread_index]));
                });
            }
            ImGui::SameLine();
            ImGui::Checkbox("показывать нити", &show_threads);
            if (tree.is_threaded()) {
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
                                   "Прошито: %s (нити обновляются при изменениях)",
                                   TRAVERSAL_NAMES[threaded_index]);
            } else {
                ImGui::TextDisabled("Не прошито");
            }
        }

        void draw_functional_section() {
            using namespace panel_detail;
            using namespace gui_detail;
            if (!ImGui::CollapsingHeader("map / where / reduce", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            ImGui::SetNextItemWidth(220);
            preset_combo("##map", map_index, map_presets);
            ImGui::SameLine();
            if (ImGui::Button("map")) {
                run_action("map", [&] {
                    const MapPreset<T>& preset = map_presets.at(map_index);
                    set_second(tree.map(preset.func), std::string("map(") + preset.name + ")");
                });
            }

            ImGui::SetNextItemWidth(220);
            preset_combo("##where", where_index, where_presets);
            ImGui::SameLine();
            if (ImGui::Button("where")) {
                run_action("where", [&] {
                    const WherePreset<T>& preset = where_presets.at(where_index);
                    set_second(tree.where(preset.func), std::string("where(") + preset.name + ")");
                });
            }

            ImGui::SetNextItemWidth(220);
            preset_combo("##reduce", reduce_index, reduce_presets);
            ImGui::SameLine();
            if (ImGui::Button("reduce")) {
                run_action("reduce", [&] {
                    const ReducePreset<T>& preset = reduce_presets.at(reduce_index);
                    reduce_text = preset.run(tree, ListedTraversal(order_index));
                    add_log(std::string("reduce(") + preset.name + ", "
                            + TRAVERSAL_NAMES[order_index] + ") = " + reduce_text);
                });
            }
            ImGui::TextDisabled("reduce идёт в порядке обхода, выбранного выше");
            if (!reduce_text.empty()) ImGui::TextWrapped("= %s", reduce_text.c_str());
        }

        void draw_subtree_section() {
            if (!ImGui::CollapsingHeader("Поддеревья", ImGuiTreeNodeFlags_DefaultOpen)) return;

            if (ImGui::Button("Извлечь поддерево с корнем в введённом значении")) {
                run_action("Поддерево", [&] {
                    T value = parse_input();
                    set_second(tree.get_subtree(value),
                               "Поддерево с корнем " + GuiValue<T>::label(value));
                });
            }
            if (ImGui::Button("Входит ли второе дерево в основное?")) {
                bool found = tree.contains_subtree(second);
                add_log(found ? "Второе дерево - поддерево основного"
                              : "Второе дерево не входит в основное как поддерево");
            }
        }

        void draw_string_section() {
            if (!ImGui::CollapsingHeader("Строка", ImGuiTreeNodeFlags_DefaultOpen)) return;

            ImGui::SetNextItemWidth(160);
            ImGui::InputText("формат", format_input, sizeof(format_input));
            ImGui::TextDisabled("К/Л/П или N/L/R, вокруг каждой части ограничители");

            if (ImGui::Button("Сохранить в строку")) {
                run_action("Сохранение", [&] {
                    std::string text = tree.to_string(TraversalFormat(format_input));
                    set_string_buffer(text);
                    add_log("Строка: " + std::to_string(text.size()) + " байт (вкладка «Строка»)");
                });
            }
            if (ImGui::Button("Прочитать в основное")) {
                run_action("Чтение", [&] {
                    tree = BinarySearchTree<T>::from_string(string_buffer.data(),
                                                            TraversalFormat(format_input));
                    highlighted = Option<T>::None();
                    add_log("Прочитано в основное: " + std::to_string(tree.get_count())
                            + " элементов");
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Прочитать во второе")) {
                run_action("Чтение", [&] {
                    set_second(BinarySearchTree<T>::from_string(string_buffer.data(),
                                                                TraversalFormat(format_input)),
                               "Прочитано из строки");
                });
            }
        }

        // ---------- Отрисовка дерева ----------

        // Узел по горизонтали стоит на своей позиции по возрастанию, по вертикали - на глубине
        void draw_tree(const BinarySearchTree<T>& shown, bool is_main) {
            using namespace panel_detail;
            int count = shown.get_count();
            if (count == 0) {
                ImGui::TextDisabled("Дерево пусто");
                return;
            }
            if (count > MAX_DRAWN_NODES) {
                ImGui::TextDisabled("%d узлов - слишком много для отрисовки (максимум %d). "
                                    "Операции и строки работают как обычно.",
                                    count, MAX_DRAWN_NODES);
                return;
            }

            std::vector<DrawnNode> nodes(count);
            float max_label = 0.0f;
            shown.visit_nodes([&](const T& value, int depth, int index, int parent) {
                std::string label = GuiValue<T>::label(value);
                if (label.size() > 14) label = label.substr(0, 12) + "..";
                max_label = std::max(max_label, ImGui::CalcTextSize(label.c_str()).x);
                nodes[index] = {label, ValueCodec<T>::to_string(value), depth, parent};
            });

            ImGui::SetNextItemWidth(160);
            ImGui::SliderFloat("масштаб (Ctrl + колесо)", &zoom, 0.3f, 2.5f, "%.2f");

            float font_size = ImGui::GetFontSize() * zoom;
            float node_width = (max_label + 16.0f) * zoom;
            float node_height = font_size + 10.0f * zoom;
            float step_x = node_width + 8.0f * zoom;
            float step_y = node_height + 34.0f * zoom;
            float margin = 20.0f;

            ImGui::BeginChild(is_main ? "##main_canvas" : "##second_canvas", ImVec2(0, 0),
                              ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            float visible_left = ImGui::GetWindowPos().x - step_x;
            float visible_right = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() + step_x;

            auto center = [&](int index) {
                return ImVec2(origin.x + margin + index * step_x + node_width / 2,
                              origin.y + margin + nodes[index].depth * step_y + node_height / 2);
            };
            auto visible = [&](float x) { return x >= visible_left && x <= visible_right; };

            ImU32 edge_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            int max_depth = 0;
            for (int index = 0; index < count; index++) {
                max_depth = std::max(max_depth, nodes[index].depth);
                int parent = nodes[index].parent;
                if (parent < 0) continue;
                ImVec2 from = center(parent);
                ImVec2 to = center(index);
                if (visible(from.x) || visible(to.x)) {
                    draw->AddLine(from, to, edge_color, 1.5f * zoom);
                }
            }

            int highlight = -1;
            if (is_main && highlighted.has_value()) highlight = shown.index_of(highlighted.get_value());

            if (is_main && show_threads && shown.is_threaded()) draw_threads(shown, center);

            int hovered = -1;
            ImVec2 mouse = ImGui::GetIO().MousePos;
            for (int index = 0; index < count; index++) {
                ImVec2 middle = center(index);
                if (!visible(middle.x)) continue;

                ImVec2 top_left(middle.x - node_width / 2, middle.y - node_height / 2);
                ImVec2 bottom_right(middle.x + node_width / 2, middle.y + node_height / 2);
                bool is_hovered = ImGui::IsWindowHovered() && mouse.x >= top_left.x
                                  && mouse.x <= bottom_right.x && mouse.y >= top_left.y
                                  && mouse.y <= bottom_right.y;
                if (is_hovered) hovered = index;

                ImU32 fill = index == highlight ? IM_COL32(230, 180, 40, 255)
                           : is_hovered ? IM_COL32(90, 130, 200, 255)
                                        : IM_COL32(55, 85, 140, 255);
                draw->AddRectFilled(top_left, bottom_right, fill, 6.0f * zoom);
                draw->AddRect(top_left, bottom_right, IM_COL32(150, 180, 230, 255), 6.0f * zoom);

                const char* label = nodes[index].label.c_str();
                ImVec2 text_size = ImGui::CalcTextSize(label);
                draw->AddText(nullptr, font_size,
                              ImVec2(middle.x - text_size.x * zoom / 2,
                                     middle.y - font_size / 2),
                              IM_COL32(255, 255, 255, 255), label);
            }

            ImGui::Dummy(ImVec2(margin * 2 + count * step_x, margin * 2 + (max_depth + 1) * step_y));

            if (hovered >= 0) {
                ImGui::SetTooltip("%s\nпозиция %d, глубина %d\nклик - подставить в поле ввода",
                                  nodes[hovered].full.c_str(), hovered, nodes[hovered].depth);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    std::snprintf(value_input, sizeof(value_input), "%s",
                                  nodes[hovered].full.c_str());
                    if (is_main) highlighted = Option<T>::Some(shown.get(hovered));
                }
            }
            if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl
                && ImGui::GetIO().MouseWheel != 0.0f) {
                zoom = std::clamp(zoom * (ImGui::GetIO().MouseWheel > 0 ? 1.1f : 0.9f), 0.3f, 2.5f);
            }
            ImGui::EndChild();
        }

        // Нить - дуга от узла к следующему в прошитом обходе, изогнутая вбок от рёбер,
        // со стрелкой в середине; первый узел обведён
        template <class Center>
        void draw_threads(const BinarySearchTree<T>& shown, Center center) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImU32 color = IM_COL32(255, 150, 40, 200);
            auto iter = shown.get_threaded_enumerator();
            int previous = -1;
            while (iter->move_next()) {
                int current = shown.index_of(iter->get_current());
                if (previous < 0) {
                    draw->AddCircle(center(current), 22.0f * zoom, color, 0, 3.0f);
                    previous = current;
                    continue;
                }

                ImVec2 from = center(previous);
                ImVec2 to = center(current);
                ImVec2 delta(to.x - from.x, to.y - from.y);
                ImVec2 normal(-delta.y * 0.3f, delta.x * 0.3f);
                ImVec2 control(from.x + delta.x / 2 + normal.x, from.y + delta.y / 2 + normal.y);
                draw->AddBezierQuadratic(from, control, to, color, 2.0f);

                // Точка и касательная квадратичной кривой при t = 0.5
                ImVec2 middle((from.x + 2 * control.x + to.x) / 4,
                              (from.y + 2 * control.y + to.y) / 4);
                float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (length < 1.0f) {
                    previous = current;
                    continue;
                }
                ImVec2 dir(delta.x / length, delta.y / length);
                float size = 7.0f * zoom;
                draw->AddTriangleFilled(
                    ImVec2(middle.x + dir.x * size, middle.y + dir.y * size),
                    ImVec2(middle.x - dir.x * size - dir.y * size * 0.7f,
                           middle.y - dir.y * size + dir.x * size * 0.7f),
                    ImVec2(middle.x - dir.x * size + dir.y * size * 0.7f,
                           middle.y - dir.y * size - dir.x * size * 0.7f),
                    color);
                previous = current;
            }
        }

        void draw_log() {
            ImGui::BeginChild("##log");
            for (const LogEntry& entry : log) {
                if (entry.error) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", entry.text.c_str());
                } else {
                    ImGui::TextWrapped("%s", entry.text.c_str());
                }
            }
            if (scroll_log) {
                ImGui::SetScrollHereY(1.0f);
                scroll_log = false;
            }
            ImGui::EndChild();
        }
    public:
        TreePanel() {
            std::mt19937 random(7);
            for (int idx = 0; idx < 12; idx++) tree.insert(GuiValue<T>::random(random, 99));
        }

        const char* type_name() const override { return GuiValue<T>::TYPE_NAME; }

        void draw() override {
            ImGui::BeginChild("##controls", ImVec2(460.0f * ImGui::GetStyle().FontScaleDpi, 0),
                              ImGuiChildFlags_Borders);
            draw_elements_section();
            draw_fill_section();
            draw_traversal_section();
            draw_functional_section();
            draw_subtree_section();
            draw_string_section();
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##view");
            ImGui::Text("Элементов: %d   Высота: %d   (AVL: высота <= 1.44 log2(n + 2))",
                        tree.get_count(), tree.get_height());

            float log_height = 130.0f * ImGui::GetStyle().FontScaleDpi;
            ImGui::BeginChild("##tabs", ImVec2(0, -log_height));
            if (ImGui::BeginTabBar("##view_tabs")) {
                if (ImGui::BeginTabItem("Основное дерево")) {
                    draw_tree(tree, true);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Второе дерево")) {
                    ImGui::Text("%s", second_title.c_str());
                    ImGui::SameLine();
                    if (ImGui::Button("Сделать основным")) {
                        tree = second;
                        highlighted = Option<T>::None();
                        add_log("Второе дерево стало основным");
                    }
                    draw_tree(second, false);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Строка")) {
                    ImGui::InputTextMultiline("##string", string_buffer.data(),
                                              string_buffer.size(), ImVec2(-1, -1),
                                              ImGuiInputTextFlags_WordWrap);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Обход")) {
                    ImGui::TextWrapped("%s", traversal_text.empty() ? "Нажмите «Обойти»"
                                                                    : traversal_text.c_str());
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::EndChild();

            ImGui::SeparatorText("Журнал");
            draw_log();
            ImGui::EndChild();
        }
};

#endif
