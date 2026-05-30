# Compiler and global optimization flags
CXX      := g++
CXXFLAGS := -std=c++23 -O3 -march=native -flto -fno-plt \
            -funroll-loops -finline-functions \
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion

# Isolated flags for assembly export (WITHOUT -flto to ensure clean, readable text assembly)
ASM_FLAGS := -std=c++23 -O3 -march=native -fno-plt \
             -funroll-loops -finline-functions \
             -Wall -Wextra -Wpedantic -Wshadow -Wconversion

# Project directory structure
SRC_DIR := .
OBJ_DIR := obj
ASM_DIR := asm

# Source and target discovery mapping
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
ASMS := $(SRCS:$(SRC_DIR)/%.cpp=$(ASM_DIR)/%.s)

TARGET := algo_tresor

# Main default target
all: $(TARGET) $(ASMS)

# Binary linkage rule (utilizes global CXXFLAGS with -flto)
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Object compilation rule (utilizes global CXXFLAGS with -flto)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Assembly generation rule (uses explicit ASM_FLAGS to remove -flto)
# -S            : Generates assembly code only, skips linkage step
# -masm=intel   : Enforces readable Intel assembly syntax layout
# -fverbose-asm : Injects source variable names into assembly comments
$(ASM_DIR)/%.s: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(ASM_FLAGS) -S -masm=intel -fverbose-asm $< -o $@

# Cleanup artifact tracking database
clean:
	rm -rf $(OBJ_DIR) $(ASM_DIR) $(TARGET)

.PHONY: all clean