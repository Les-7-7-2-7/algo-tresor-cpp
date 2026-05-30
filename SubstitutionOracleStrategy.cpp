#include "SubstitutionOracleStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void SubstitutionOracleStrategy::preprocess(Game& game) {
	const auto& itemIds = game.getItemIds();
	const int* sizes = game.getSizesPtr();
	const int* weights = game.getWeightsPtr();
	const int* costs = game.getCostsPtr();
	size_t numItems = itemIds.size();

	sortedProxies.reserve(numItems);

	// 1. Map sorting proxies based on bounded composite multidimensional densities
#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		double d = static_cast<double>(costs[i]) /
			((static_cast<double>(sizes[i]) / static_cast<double>(game.getSizeCapacity())) +
				(static_cast<double>(weights[i]) / static_cast<double>(game.getWeightCapacity())) + 1e-6);
		sortedProxies.push_back({ static_cast<int>(i), d });
	}

	// High speed preprocessing sort to isolate the standalone structural upper bounds
	std::sort(sortedProxies.begin(), sortedProxies.end(), [](const ItemSortProxy& a, const ItemSortProxy& b) noexcept {
		return a.density > b.density;
		});

	// 2. Build the autonomous Ideal Reference Solution Plan
	int simulatedS = 0;
	int simulatedW = 0;
	int capS = game.getSizeCapacity();
	int capW = game.getWeightCapacity();

	for (size_t i = 0; i < numItems; ++i) {
		int idx = sortedProxies[i].index;
		if ((simulatedS + sizes[idx] <= capS) && (simulatedW + weights[idx] <= capW)) {
			simulatedS += sizes[idx];
			simulatedW += weights[idx];
			game.markItemAsIdeal(idx, true); // This item belongs to our core target matrix
		}
	}
}

int SubstitutionOracleStrategy::pickItem(Game& game) {
	int finalChoice = -1;

	const int remS = game.getRemainingSize();
	const int remW = game.getRemainingWeight();
	const int totalS = game.getSizeCapacity();
	const int totalW = game.getWeightCapacity();

	int oppUsedS = 0;
	int oppUsedW = 0;
	const auto& oppItemIds = game.getOpponentItemIds();
	const auto& idToIdx = game.getIdToIndexMap();
	const int* sizes = game.getSizesPtr();
	const int* weights = game.getWeightsPtr();
	const int* costs = game.getCostsPtr();
	const auto& itemIds = game.getItemIds();
	const auto& bitset = game.getBitset();

	const size_t numOppItems = oppItemIds.size();
#pragma GCC unroll 4
	for (size_t i = 0; i < numOppItems; ++i) {
		int idx = idToIdx[oppItemIds[i]];
		oppUsedS += sizes[idx];
		oppUsedW += weights[idx];
	}

	const int oppRemS = totalS - oppUsedS;
	const int oppRemW = totalW - oppUsedW;

	const size_t numTotalItems = itemIds.size();

	// TARGET HIERARCHY SELECTION TREE (SESE compliant)
	int bestIdealChoiceId = -1;
	int maxIdealCost = -1;

	int bestTacticalInterferenceId = -1;
	int maxInterferenceCost = -1;

	int fallbackId = -1;
	double maxFallbackDensity = -1.0;

#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable) {
			bool fitsInMyBag = (sizes[i] <= remS && weights[i] <= remW);
			bool fitsInOppBag = (sizes[i] <= oppRemS && weights[i] <= oppRemW);
			bool isIdeal = game.isItemIdeal(static_cast<int>(i));

			// Track Tier 1: Best available items from our own precalculated target plan
			if (fitsInMyBag && isIdeal) {
				if (costs[i] > maxIdealCost) {
					maxIdealCost = costs[i];
					bestIdealChoiceId = id;
				}
			}

			// Track Tier 2: Best available item from opponent's potential greedy vision (Denial)
			if (fitsInMyBag && fitsInOppBag && !isIdeal) {
				if (costs[i] > maxInterferenceCost) {
					maxInterferenceCost = costs[i];
					bestTacticalInterferenceId = id;
				}
			}

			// Track Tier 3: Emergency fallback using dynamic residual capacity footprint
			if (fitsInMyBag) {
				double d = static_cast<double>(costs[i]) /
					((static_cast<double>(sizes[i]) / remS) + (static_cast<double>(weights[i]) / remW) + 1e-6);
				if (d > maxFallbackDensity) {
					maxFallbackDensity = d;
					fallbackId = id;
				}
			}
		}
	}

	// Branchless hierarchical matching cascade to determine final choice assignment
	if (bestIdealChoiceId != -1) {
		finalChoice = bestIdealChoiceId;
	}
	else if (bestTacticalInterferenceId != -1 && maxInterferenceCost > (maxIdealCost * 2)) {
		// High-yield active obstruction trigger: block opponent if their gain is abnormally high
		finalChoice = bestTacticalInterferenceId;
	}
	else {
		finalChoice = fallbackId;
	}

	return finalChoice;
}