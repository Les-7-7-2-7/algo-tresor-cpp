#include "Game.hpp"
#include <iostream>

Game::Game(int numberItems, int sCapacity, int wCapacity, std::unique_ptr<Strategy> strat)
	: sizeCapacity(sCapacity), weightCapacity(wCapacity), strategy(std::move(strat)) {
	itemIds.reserve(numberItems);
	itemSizes.reserve(numberItems);
	itemWeights.reserve(numberItems);
	itemCosts.reserve(numberItems);
	itemIdealMask.resize(static_cast<size_t>(numberItems), false);

	int initialIdCapacity = numberItems * 2 + 1;
	idToIndex.resize(initialIdCapacity, -1);
	availabilityBitset.resize((initialIdCapacity >> 6) + 1, 0ULL);
}

void Game::addItem(const Item& item) {
	if (item.id >= static_cast<int>(idToIndex.size())) {
		int newSize = item.id + 128;
		idToIndex.resize(newSize, -1);
		availabilityBitset.resize((newSize >> 6) + 1, 0ULL);
	}

	itemIds.push_back(item.id);
	itemSizes.push_back(item.size);
	itemWeights.push_back(item.weight);
	itemCosts.push_back(item.cost);
	if (itemIds.size() > itemIdealMask.size()) {
		itemIdealMask.push_back(false);
	}

	idToIndex[item.id] = static_cast<int>(itemIds.size()) - 1;

	size_t chunk = static_cast<size_t>(item.id) >> 6;
	size_t bit = static_cast<size_t>(item.id) & 63;
	availabilityBitset[chunk] |= (1ULL << bit);
}

void Game::preprocess() {
	strategy->preprocess(*this);
}

void Game::opponentTook(int id) {
	turnNumber++;
	if (id != -1 && id < static_cast<int>(idToIndex.size()) && isItemAvailable(id)) {
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		availabilityBitset[chunk] &= ~(1ULL << bit);

		int index = idToIndex[id];
		opponentItemIds.push_back(id);
		opponentScore += itemCosts[index];
	}
}

int Game::pickItem() {
	int chosenId = -1;
	int proposedId = strategy->pickItem(*this);

	if (proposedId != -1 && proposedId < static_cast<int>(idToIndex.size()) && isItemAvailable(proposedId)) {
		int index = idToIndex[proposedId];
		if ((currentWeight + itemWeights[index]) <= weightCapacity && (currentSize + itemSizes[index]) <= sizeCapacity) {
			chosenId = proposedId;
			size_t chunk = static_cast<size_t>(chosenId) >> 6;
			size_t bit = static_cast<size_t>(chosenId) & 63;
			availabilityBitset[chunk] &= ~(1ULL << bit);

			myItemIds.push_back(chosenId);
			currentSize += itemSizes[index];
			currentWeight += itemWeights[index];
			currentScore += itemCosts[index];
		}
	}
	return chosenId;
}