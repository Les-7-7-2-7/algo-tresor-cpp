#ifndef GAME_HPP
#define GAME_HPP

#include "Item.hpp"
#include "Strategy.hpp"
#include <vector>
#include <memory>
#include <string_view>
#include <cstdint>

class Game {
private:
	static constexpr std::string_view RESET = "\033[0m";
	static constexpr std::string_view CYAN = "\033[36m";
	static constexpr std::string_view GREEN = "\033[32m";
	static constexpr std::string_view RED = "\033[31m";
	static constexpr std::string_view YELLOW = "\033[33m";

	// Structure of Arrays (SoA) layout for high-density SIMD/AVX cache friendliness
	alignas(64) std::vector<int> itemIds;
	alignas(64) std::vector<int> itemSizes;
	alignas(64) std::vector<int> itemWeights;
	alignas(64) std::vector<int> itemCosts;

	// Bitset optimization using native uint64_t chunks instead of std::vector<bool>
	std::vector<uint64_t> availabilityBitset;
	std::vector<int> idToIndex;

	int sizeCapacity;
	int weightCapacity;
	std::unique_ptr<Strategy> strategy;

	std::vector<int> myItemIds;
	std::vector<int> opponentItemIds;

	int currentSize{ 0 };
	int currentWeight{ 0 };
	int currentScore{ 0 };
	int opponentScore{ 0 };
	int turnNumber{ 0 };

public:
	Game(int numberItems, int sCapacity, int wCapacity, std::unique_ptr<Strategy> strat);
	~Game();

	void addItem(const Item& item);
	void preprocess();
	void opponentTook(int id);
	int pickItem();

	// High performance getters exposing raw aligned pointers for SIMD operations
	[[nodiscard]] int getRemainingWeight() const noexcept { return weightCapacity - currentWeight; }
	[[nodiscard]] int getRemainingSize() const noexcept { return sizeCapacity - currentSize; }
	[[nodiscard]] int getSizeCapacity() const noexcept { return sizeCapacity; }
	[[nodiscard]] int getWeightCapacity() const noexcept { return weightCapacity; }

	[[nodiscard]] const std::vector<int>& getOpponentItemIds() const noexcept { return opponentItemIds; }
	[[nodiscard]] const std::vector<int>& getItemIds() const noexcept { return itemIds; }
	[[nodiscard]] const int* getSizesPtr() const noexcept { return itemSizes.data(); }
	[[nodiscard]] const int* getWeightsPtr() const noexcept { return itemWeights.data(); }
	[[nodiscard]] const int* getCostsPtr() const noexcept { return itemCosts.data(); }

	[[nodiscard]] const std::vector<uint64_t>& getBitset() const noexcept { return availabilityBitset; }
	[[nodiscard]] const std::vector<int>& getIdToIndexMap() const noexcept { return idToIndex; }

	[[nodiscard]] [[gnu::always_inline]] inline bool isItemAvailable(int id) const noexcept {
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		return (chunk < availabilityBitset.size()) && ((availabilityBitset[chunk] & (1ULL << bit)) != 0);
	}
};

#endif // GAME_HPP