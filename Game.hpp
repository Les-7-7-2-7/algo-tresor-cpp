#ifndef GAME_HPP
#define GAME_HPP

#include "Item.hpp"
#include "Strategy.hpp"
#include <vector>
#include <memory>
#include <cstdint>

class Game {
private:
	alignas(64) std::vector<int> itemIds;
	alignas(64) std::vector<int> itemSizes;
	alignas(64) std::vector<int> itemWeights;
	alignas(64) std::vector<int> itemCosts;

	std::vector<uint64_t> availabilityBitset;
	std::vector<int> idToIndex;

	int sizeCapacity;
	int weightCapacity;
	std::unique_ptr<Strategy> strategy;

	int currentSize{ 0 };
	int currentWeight{ 0 };
	int opponentSize{ 0 };
	int opponentWeight{ 0 };

	int myScore{ 0 };
	int oppScore{ 0 };
	int turnNumber{ 0 };

public:
	Game(int numberItems, int sCapacity, int wCapacity, std::unique_ptr<Strategy> strat);
	~Game() = default;

	void addItem(const Item& item);
	void preprocess();
	void opponentTook(int id) noexcept;
	int pickItem();

	[[nodiscard]] int getRemainingWeight() const noexcept { return weightCapacity - currentWeight; }
	[[nodiscard]] int getRemainingSize() const noexcept { return sizeCapacity - currentSize; }
	[[nodiscard]] int getOpponentRemainingWeight() const noexcept { return weightCapacity - opponentWeight; }
	[[nodiscard]] int getOpponentRemainingSize() const noexcept { return sizeCapacity - opponentSize; }
	[[nodiscard]] int getSizeCapacity() const noexcept { return sizeCapacity; }
	[[nodiscard]] int getWeightCapacity() const noexcept { return weightCapacity; }

	[[nodiscard]] const std::vector<int>& getItemIds() const noexcept { return itemIds; }
	[[nodiscard]] const int* getSizesPtr() const noexcept { return itemSizes.data(); }
	[[nodiscard]] const int* getWeightsPtr() const noexcept { return itemWeights.data(); }
	[[nodiscard]] const int* getCostsPtr() const noexcept { return itemCosts.data(); }

	[[nodiscard]] const std::vector<uint64_t>& getBitset() const noexcept { return availabilityBitset; }
	[[nodiscard]] const std::vector<int>& getIdToIndexMap() const noexcept { return idToIndex; }

	[[nodiscard]] [[gnu::always_inline]] inline bool isItemAvailable(int id) const noexcept {
		bool available = false;
		if (id >= 0 && id < static_cast<int>(idToIndex.size())) {
			size_t chunk = static_cast<size_t>(id) >> 6;
			size_t bit = static_cast<size_t>(id) & 63;
			if (chunk < availabilityBitset.size()) {
				available = ((availabilityBitset[chunk] & (1ULL << bit)) != 0);
			}
		}
		return available;
	}
};

#endif // GAME_HPP