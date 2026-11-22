# Windows: mingw32-make. Linux: make.
CXX = g++
GTEST := googletest/googletest
GUI_FLAGS := -Ivendor/imgui -Ivendor/implot
GTEST_FLAGS := -I$(GTEST) -I$(GTEST)/include

ifeq ($(OS),Windows_NT)
PLATFORM := windows
EXT := .exe
LIBS := -static -lpsapi
remove_dir = if exist "$(subst /,\,$(1))" rmdir /s /q "$(subst /,\,$(1))"
GUI_LIBS := -lglfw3 -lopengl32 -lgdi32 -limm32
mkdir = if not exist "$(subst /,\,$@)" mkdir "$(subst /,\,$@)"
else
PLATFORM := linux
LIBS := -pthread
remove_dir = rm -rf "$(1)"
GUI_LIBS := -lglfw -lGL -ldl -lX11
mkdir = mkdir -p "$@"
endif

.DEFAULT_GOAL := all
ifeq ($(origin LAB),undefined)
.PHONY: all clean
clean:
	$(call remove_dir,build/$(PLATFORM))

all:
	$(MAKE) LAB=1 all
	$(MAKE) LAB=2 all
else
ifneq ($(LAB),1)
ifneq ($(LAB),2)
$(error LAB must be 1 or 2)
endif
endif

DIR := lr$(LAB)
COMMON := build/$(PLATFORM)/common
OUT := build/$(PLATFORM)/$(DIR)
FLAGS := -std=c++17 -Wall -Wextra -Ishared/include -I$(DIR)/include
HEADERS := $(wildcard shared/include/*/*.h shared/include/*/*.tpp $(DIR)/include/*/*.h $(DIR)/include/*/*.tpp)
SRC := $(wildcard $(DIR)/src/vfs/*.cpp)
TESTS := $(wildcard $(DIR)/tests/*.cpp shared/tests/*.cpp)
VENDOR := imgui imgui_draw imgui_tables imgui_widgets imgui_impl_glfw imgui_impl_opengl3 implot implot_items
GUI_OBJ := $(addprefix $(COMMON)/,$(addsuffix .o,$(VENDOR)))
GTEST_OBJ := $(COMMON)/gtest-all.o $(COMMON)/gtest_main.o
vpath %.cpp vendor/imgui vendor/implot

.PHONY: all gui benchmark test clean
clean:
	$(call remove_dir,$(OUT))
all: gui benchmark $(OUT)/tests$(EXT)
GUI_NAME := $(if $(filter 2,$(LAB)),vfs,gui)
gui: $(OUT)/$(GUI_NAME)$(EXT)
benchmark: $(OUT)/benchmark$(EXT)
test: $(OUT)/tests$(EXT)
	$(OUT)/tests$(EXT)

$(COMMON) $(OUT):
	$(mkdir)

$(COMMON)/gtest%.o: $(GTEST)/src/gtest%.cc | $(COMMON)
	$(CXX) -std=c++17 -O2 $(GTEST_FLAGS) -MMD -MP -c $< -o $@

$(COMMON)/%.o: %.cpp | $(COMMON)
	$(CXX) -std=c++17 -O2 $(GUI_FLAGS) -MMD -MP -c $< -o $@

-include $(GUI_OBJ:.o=.d) $(GTEST_OBJ:.o=.d)

$(OUT)/tests$(EXT): $(TESTS) $(SRC) $(GTEST_OBJ) $(HEADERS) | $(OUT)
	$(CXX) $(FLAGS) $(GTEST_FLAGS) $(TESTS) $(SRC) $(GTEST_OBJ) -o $@ $(LIBS)

$(OUT)/benchmark$(EXT): $(DIR)/benchmarks/benchmark_main.cpp $(SRC) $(HEADERS) | $(OUT)
	$(CXX) $(FLAGS) -O2 -DNDEBUG $< $(SRC) -o $@ $(LIBS)

$(OUT)/$(GUI_NAME)$(EXT): $(DIR)/src/gui_main.cpp $(SRC) $(GUI_OBJ) $(HEADERS) | $(OUT)
	$(CXX) $(FLAGS) -O2 $(GUI_FLAGS) $< $(SRC) $(GUI_OBJ) -o $@ $(GUI_LIBS) $(LIBS)
endif
