#include "Game.hpp"

Game::Game(int numberItems, int sCapacity, int wCapacity, std::unique_ptr<Strategy> strat)
	: sizeCapacity(sCapacity), weightCapacity(wCapacity), strategy(std::move(strat)) {
	itemIds.reserve(numberItems);
	itemSizes.reserve(numberItems);
	itemWeights.reserve(numberItems);
	itemCosts.reserve(numberItems);

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

	idToIndex[item.id] = static_cast<int>(itemIds.size()) - 1;

	size_t chunk = static_cast<size_t>(item.id) >> 6;
	size_t bit = static_cast<size_t>(item.id) & 63;
	availabilityBitset[chunk] |= (1ULL << bit);
}

void Game::preprocess() {
	strategy->preprocess(*this);
}

void Game::opponentTook(int id) noexcept {
	turnNumber++;
	if (id != -1 && isItemAvailable(id)) {
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		availabilityBitset[chunk] &= ~(1ULL << bit);

		int index = idToIndex[id];
		opponentSize += itemSizes[index];
		opponentWeight += itemWeights[index];
		oppScore += itemCosts[index];
	}
}

int Game::pickItem() {
	int chosenId = -1;
	int proposedId = strategy->pickItem(*this);

	if (proposedId != -1 && isItemAvailable(proposedId)) {
		int index = idToIndex[proposedId];
		if ((currentWeight + itemWeights[index]) <= weightCapacity &&
			(currentSize + itemSizes[index]) <= sizeCapacity) {

			chosenId = proposedId;
			size_t chunk = static_cast<size_t>(chosenId) >> 6;
			size_t bit = static_cast<size_t>(chosenId) & 63;
			availabilityBitset[chunk] &= ~(1ULL << bit);

			currentSize += itemSizes[index];
			currentWeight += itemWeights[index];
			myScore += itemCosts[index];
			turnNumber++;
		}
	}
	return chosenId;
}