#include "Game.hpp"
#include <chrono>
#include <iostream>

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

Game::~Game() = default;

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
	std::cerr << CYAN << "[preprocess] " << itemIds.size() << " items, sizeCapacity=" << sizeCapacity
		<< ", weightCapacity=" << weightCapacity << RESET << "\n";

	auto start = std::chrono::high_resolution_clock::now();
	strategy->preprocess(*this);
	auto end = std::chrono::high_resolution_clock::now();

	auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::cerr << CYAN << "[preprocess] done — " << elapsedMs << "ms elapsed, " << (5000 - elapsedMs)
		<< "ms remaining" << RESET << "\n";
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

		std::cerr << RED << "[opponent] took item " << id << " (size=" << itemSizes[index]
			<< ", weight=" << itemWeights[index] << ", value=" << itemCosts[index] << ")" << RESET << "\n";
	}
	else if (id == -1) {
		std::cerr << YELLOW << "[opponent] passed" << RESET << "\n";
	}
}

int Game::pickItem() {
	int chosenId = -1;
	auto start = std::chrono::high_resolution_clock::now();
	int proposedId = strategy->pickItem(*this);
	auto end = std::chrono::high_resolution_clock::now();
	[[maybe_unused]] auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

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

			std::cerr << GREEN << "[pick] took item " << proposedId << " (size=" << itemSizes[index]
				<< ", weight=" << itemWeights[index] << ", value=" << itemCosts[index] << ") | bag: size="
				<< currentSize << "/" << sizeCapacity << ", weight=" << currentWeight
				<< "/" << weightCapacity << RESET << "\n";
		}
	}

	if (chosenId == -1) {
		std::cerr << YELLOW << "[pick] passing" << RESET << "\n";
	}

	return chosenId;
}