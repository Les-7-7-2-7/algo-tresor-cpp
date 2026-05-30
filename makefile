# Compilateur et options globales
CXX      := g++
CXXFLAGS := -std=c++23 -O3 -march=native -flto -fno-plt \
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion

# Dossiers du projet
SRC_DIR := .
OBJ_DIR := obj
ASM_DIR := asm

# Fichiers sources et cibles
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
ASMS := $(SRCS:$(SRC_DIR)/%.cpp=$(ASM_DIR)/%.s)

TARGET := algo_tresor

# Règle principale (Compile tout + Génère l'ASM)
all: $(TARGET) $(ASMS)

# Liaison de l'exécutable
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Règle de compilation pour les fichiers objets (.o)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Règle pour générer l'assembleur en syntaxe Intel (.s)
# -S : Génère l'assembleur sans lier
# -masm=intel : Force la syntaxe Intel au lieu d'AT&T
# -fverbose-asm : Ajoute les noms des variables et structures en commentaires dans l'ASM
$(ASM_DIR)/%.s: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -S -masm=intel -fverbose-asm $< -o $@

# Nettoyage des artefacts de compilation
clean:
	rm -rf $(OBJ_DIR) $(ASM_DIR) $(TARGET)

.PHONY: all clean