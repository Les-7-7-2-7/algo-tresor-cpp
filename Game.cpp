#include "Game.hpp"
#include <chrono>

void Game::addItem(const Item& item) {
	if (item.id >= static_cast<int>(isAvailable.size())) {
		isAvailable.resize(item.id + 1, false);
		idToRawIndex.resize(item.id + 1, -1);
	}
	rawItems.push_back(item);
	isAvailable[item.id] = true;
	idToRawIndex[item.id] = static_cast<int>(rawItems.size()) - 1;
}

void Game::preprocess() {
	std::cerr << CYAN << "[preprocess] " << rawItems.size() << " items, sizeCapacity=" << sizeCapacity
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
	if (id == -1) {
		std::cerr << YELLOW << "[opponent] passed" << RESET << "\n";
	}
	else {
		if (id < static_cast<int>(isAvailable.size()) && isAvailable[id]) {
			isAvailable[id] = false;
			const Item& item = rawItems[idToRawIndex[id]];
			opponentItems.push_back(item);
			opponentScore += item.cost;
			std::cerr << RED << "[opponent] took item " << id << " (size=" << item.size
				<< ", weight=" << item.weight << ", value=" << item.cost << ")" << RESET << "\n";
		}
	}
}

int Game::pickItem() {
	int chosenId = -1;
	auto start = std::chrono::high_resolution_clock::now();
	int proposedId = strategy->pickItem(*this);
	auto end = std::chrono::high_resolution_clock::now();
	[[maybe_unused]] auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	if (proposedId != -1) {
		if (proposedId < static_cast<int>(isAvailable.size()) && isAvailable[proposedId]) {
			const Item& item = rawItems[idToRawIndex[proposedId]];
			if ((currentWeight + item.weight) <= weightCapacity && (currentSize + item.size) <= sizeCapacity) {
				chosenId = proposedId;
				isAvailable[chosenId] = false;
				myItems.push_back(item);
				currentSize += item.size;
				currentWeight += item.weight;
				currentScore += item.cost;

				std::cerr << GREEN << "[pick] took item " << proposedId << " (size=" << item.size
					<< ", weight=" << item.weight << ", value=" << item.cost << ") | bag: size="
					<< currentSize << "/" << sizeCapacity << ", weight=" << currentWeight
					<< "/" << weightCapacity << RESET << "\n";
			}
		}
	}

	if (chosenId == -1) {
		std::cerr << YELLOW << "[pick] passing" << RESET << "\n";
	}

	return chosenId;
}