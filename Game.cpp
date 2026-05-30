#include "Game.hpp"
#include <chrono>
#include <iostream>

Game::Game(int numberItems, int sCapacity, int wCapacity, std::unique_ptr<Strategy> strat)
	: sizeCapacity(sCapacity), weightCapacity(wCapacity), strategy(std::move(strat)) {
	rawItems.reserve(numberItems);

	// On initialise avec une taille de base, mais addItem s'adaptera dynamiquement si besoin
	int initialSize = numberItems * 2 + 1;
	isAvailable.resize(initialSize, false);
	idToRawIndex.resize(initialSize, -1);
}

// Requis dans le .cpp pour que std::unique_ptr puisse voir la définition complète de Strategy au moment de sa destruction
Game::~Game() = default;

void Game::addItem(const Item& item) {
	// Sécurité absolue & performance : Redimensionnement à la volée si l'ID dépasse les prévisions
	if (item.id >= static_cast<int>(isAvailable.size())) {
		int newSize = item.id + 100; // Marge pour éviter les réallocations successives
		isAvailable.resize(newSize, false);
		idToRawIndex.resize(newSize, -1);
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