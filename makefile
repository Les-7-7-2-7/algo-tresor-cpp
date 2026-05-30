CXX      := g++
CXXFLAGS := -std=c++23 -O3 -march=native -flto -fno-plt \
            -funroll-loops -finline-functions \
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion

ASM_FLAGS := -std=c++23 -O3 -march=native -fno-plt \
             -funroll-loops -finline-functions \
             -Wall -Wextra -Wpedantic -Wshadow -Wconversion

OBJ_DIR := obj
ASM_DIR := asm

SRCS := Game.cpp Main.cpp DMRStrategy.cpp
OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
ASMS := $(SRCS:%.cpp=$(ASM_DIR)/%.s)

TARGET := algo_tresor

all: $(TARGET) $(ASMS)

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ASM_DIR)/%.s: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(ASM_FLAGS) -S -masm=intel -fverbose-asm $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(ASM_DIR) $(TARGET)

.PHONY: all clean