#ifndef GAME_HPP
#define GAME_HPP

#include "Item.hpp"
#include "Strategy.hpp"
#include <vector>
#include <memory>
#include <string_view>

class Game {
private:
	static constexpr std::string_view RESET = "\033[0m";
	static constexpr std::string_view CYAN = "\033[36m";
	static constexpr std::string_view GREEN = "\033[32m";
	static constexpr std::string_view RED = "\033[31m";
	static constexpr std::string_view YELLOW = "\033[33m";

	std::vector<Item> rawItems;
	std::vector<bool> isAvailable;
	std::vector<int> idToRawIndex;

	int sizeCapacity;
	int weightCapacity;
	std::unique_ptr<Strategy> strategy;

	std::vector<Item> myItems;
	std::vector<Item> opponentItems;

	int currentSize{ 0 };
	int currentWeight{ 0 };
	int currentScore{ 0 };
	int opponentScore{ 0 };
	int turnNumber{ 0 };

public:
	// Le constructeur est uniquement déclaré ici pour éviter d'instancier 
	// std::unique_ptr avec un type incomplet lors des inclusions.
	Game(int numberItems, int sCapacity, int wCapacity, std::unique_ptr<Strategy> strat);
	~Game(); // Destructeur explicite obligatoire pour std::unique_ptr avec forward declarations

	void addItem(const Item& item);
	void preprocess();
	void opponentTook(int id);
	int pickItem();

	[[nodiscard]] int getRemainingWeight() const noexcept { return weightCapacity - currentWeight; }
	[[nodiscard]] int getRemainingSize() const noexcept { return sizeCapacity - currentSize; }
	[[nodiscard]] int getSizeCapacity() const noexcept { return sizeCapacity; }
	[[nodiscard]] int getWeightCapacity() const noexcept { return weightCapacity; }
	[[nodiscard]] const std::vector<Item>& getOpponentItems() const noexcept { return opponentItems; }
	[[nodiscard]] const std::vector<Item>& getAvailableItemsRaw() const noexcept { return rawItems; }
	[[nodiscard]] const std::vector<bool>& getAvailableMask() const noexcept { return isAvailable; }
};

#endif // GAME_HPP