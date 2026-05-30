#ifndef GAME_HPP
#define GAME_HPP

#include "Item.hpp"
#include "Strategy.hpp"
#include <vector>
#include <memory>
#include <iostream>

class Game {
private:
	static constexpr std::string_view RESET = "\033[0m";
	static constexpr std::string_view CYAN = "\033[36m";
	static constexpr std::string_view GREEN = "\033[32m";
	static constexpr std::string_view RED = "\033[31m";
	static constexpr std::string_view YELLOW = "\033[33m";

	std::vector<Item> rawItems;
	std::vector<bool> isAvailable; // Remplace le HashMap avec recherche O(1) sans allocation
	std::vector<int> idToRawIndex;  // Table de correspondance ID -> Index dans rawItems

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
	Game(int numberItems, int sCapacity, int wCapacity, std::unique_ptr<Strategy> strat)
		: sizeCapacity(sCapacity), weightCapacity(wCapacity), strategy(std::move(strat)) {
		rawItems.reserve(numberItems);
		// Supposons une borne raisonnable sur la valeur maximale de l'ID pour le masque d'indexation direct
		isAvailable.resize(numberItems * 2 + 1, false);
		idToRawIndex.resize(numberItems * 2 + 1, -1);
	}

	void addItem(const Item& item);
	void preprocess();
	void opponentTook(int id);
	int pickItem();

	// Getters
	[[nodiscard]] int getRemainingWeight() const noexcept { return weightCapacity - currentWeight; }
	[[nodiscard]] int getRemainingSize() const noexcept { return sizeCapacity - currentSize; }
	[[nodiscard]] int getSizeCapacity() const noexcept { return sizeCapacity; }
	[[nodiscard]] int getWeightCapacity() const noexcept { return weightCapacity; }
	[[nodiscard]] const std::vector<Item>& getOpponentItems() const noexcept { return opponentItems; }
	[[nodiscard]] const std::vector<Item>& getAvailableItemsRaw() const noexcept { return rawItems; }
	[[nodiscard]] const std::vector<bool>& getAvailableMask() const noexcept { return isAvailable; }
};

#endif // GAME_HPP