CXX      := g++
CXXFLAGS := -std=c++23 -O3 -march=native -flto -fno-plt \
            -funroll-loops -finline-functions \
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion

SRC_DIR := .
OBJ_DIR := obj
ASM_DIR := asm

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
ASMS := $(SRCS:$(SRC_DIR)/%.cpp=$(ASM_DIR)/%.s)

TARGET := algo_tresor

all: $(TARGET) $(ASMS)

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ASM_DIR)/%.s: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -S -masm=intel -fverbose-asm $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(ASM_DIR) $(TARGET)

.PHONY: all clean