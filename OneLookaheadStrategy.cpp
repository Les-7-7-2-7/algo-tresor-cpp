#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess(Game& game) {
	size_t totalItems = game.getItemIds().size();
	candidatesPool.reserve(totalItems);
	rolloutBitset.resize(game.getBitset().size(), 0ULL);
}

[[gnu::always_inline]] inline double OneLookaheadStrategy::evaluateDensity(int cost, int size, int weight, int remS, int remW) const noexcept {
	double result = 0.0;
	if (remS > 0 && remW > 0) {
		result = static_cast<double>(cost) /
			((static_cast<double>(size) / remS) + (static_cast<double>(weight) / remW));
	}
	return result;
}

[[gnu::always_inline]] inline double OneLookaheadStrategy::calculateDynamicGamma(int totalItems, int currentTurn) const noexcept {
	double progress = static_cast<double>(currentTurn) / static_cast<double>(std::max(1, totalItems));
	double gamma = baseGamma;

	if (progress < 0.15) {
		gamma = baseGamma * 0.4; // Early game: Focus heavily on own inventory optimization
	}
	else if (progress > 0.80) {
		gamma = 0.0;             // Endgame: Absolute greed, ignore opponent entirely
	}
	else {
		gamma = baseGamma * 1.6; // Midgame: Maximum defensive denial interference
	}
	return gamma;
}

double OneLookaheadStrategy::computeDeterministicRollout(
	double gamma, const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
	int initialMyRemS, int initialMyRemW, int initialOppRemS, int initialOppRemW,
	double initialMyScore, double initialOppScore) noexcept {

	int myRemS = initialMyRemS;
	int myRemW = initialMyRemW;
	int oppRemS = initialOppRemS;
	int oppRemW = initialOppRemW;
	double myScore = initialMyScore;
	double oppScore = initialOppScore;

	const size_t numTotalItems = itemIds.size();
	bool gameStillActive = true;

	// High speed flash rollout loop simulating endgame mechanics
	while (gameStillActive) {
		gameStillActive = false;

		int bestMyIdx = -1;
		double bestMyDensity = -1.0;
		int bestOppIdx = -1;
		double bestOppDensity = -1.0;

		// Linear scan leveraging cached SoA layout pointers in CPU L1 cache
#pragma GCC unroll 4
		for (size_t i = 0; i < numTotalItems; ++i) {
			int id = itemIds[i];
			size_t chunk = static_cast<size_t>(id) >> 6;
			size_t bit = static_cast<size_t>(id) & 63;
			bool isAvailable = (rolloutBitset[chunk] & (1ULL << bit)) != 0;

			if (isAvailable) {
				// Evaluate best choice for me in this simulation state
				if (sizes[i] <= myRemS && weights[i] <= myRemW) {
					double d = evaluateDensity(costs[i], sizes[i], weights[i], myRemS, myRemW);
					if (d > bestMyDensity) {
						bestMyDensity = d;
						bestMyIdx = static_cast<int>(i);
					}
				}
				// Evaluate best choice for the opponent in this simulation state
				if (sizes[i] <= oppRemS && weights[i] <= oppRemW) {
					double d = evaluateDensity(costs[i], sizes[i], weights[i], oppRemS, oppRemW);
					if (d > bestOppDensity) {
						bestOppDensity = d;
						bestOppIdx = static_cast<int>(i);
					}
				}
			}
		}

		// Apply best greedy move for Me if available
		if (bestMyIdx != -1) {
			int id = itemIds[static_cast<size_t>(bestMyIdx)];
			rolloutBitset[static_cast<size_t>(id) >> 6] &= ~(1ULL << (static_cast<size_t>(id) & 63));
			myRemS -= sizes[bestMyIdx];
			myRemW -= weights[bestMyIdx];
			myScore += costs[bestMyIdx];
			gameStillActive = true;
		}

		// Apply best greedy move for the Opponent if available
		if (bestOppIdx != -1) {
			int id = itemIds[static_cast<size_t>(bestOppIdx)];
			// Verify item wasn't just taken by me in the same phase step
			size_t chunk = static_cast<size_t>(id) >> 6;
			size_t bit = static_cast<size_t>(id) & 63;
			if ((rolloutBitset[chunk] & (1ULL << bit)) != 0) {
				rolloutBitset[chunk] &= ~(1ULL << bit);
				oppRemS -= sizes[bestOppIdx];
				oppRemW -= weights[bestOppIdx];
				oppScore += costs[bestOppIdx];
				gameStillActive = true;
			}
		}
	}

	return myScore - (gamma * oppScore);
}

int OneLookaheadStrategy::pickItem(Game& game) {
	int finalChoice = -1;

	const int remS = game.getRemainingSize();
	const int remW = game.getRemainingWeight();

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

	const int oppRemS = game.getSizeCapacity() - oppUsedS;
	const int oppRemW = game.getWeightCapacity() - oppUsedW;

	double dynamicGamma = calculateDynamicGamma(static_cast<int>(itemIds.size()), game.getTurnNumber());

	candidatesPool.clear();
	const size_t numTotalItems = itemIds.size();

#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= remS && weights[i] <= remW) {
			candidatesPool.push_back({ id, static_cast<int>(i), evaluateDensity(costs[i], sizes[i], weights[i], remS, remW) });
		}
	}

	if (!candidatesPool.empty()) {
		const int totalCandidates = static_cast<int>(candidatesPool.size());
		const int limit = std::min(candidateLimit, totalCandidates);

		std::partial_sort(candidatesPool.begin(), candidatesPool.begin() + limit, candidatesPool.end(),
			[](const Candidate& a, const Candidate& b) noexcept {
				return a.density > b.density;
			}
		);

		double bestScore = -std::numeric_limits<double>::infinity();
		int bestItemId = -1;

		// Evaluate each candidate through deep flash rollout projections
		for (int i = 0; i < limit; ++i) {
			const auto& cand = candidatesPool[i];
			int idx = cand.index;
			size_t chunk = static_cast<size_t>(cand.id) >> 6;
			size_t bit = static_cast<size_t>(cand.id) & 63;

			// Prepare memory state for rollout simulation
			std::copy(bitset.begin(), bitset.end(), rolloutBitset.begin());
			rolloutBitset[chunk] &= ~(1ULL << bit); // Take item

			double score = computeDeterministicRollout(
				dynamicGamma, itemIds, sizes, weights, costs,
				remS - sizes[idx], remW - weights[idx], oppRemS, oppRemW,
				static_cast<double>(costs[idx]), 0.0
			);

			if (score > bestScore) {
				bestScore = score;
				bestItemId = cand.id;
			}
		}

		finalChoice = bestItemId;
	}

	return finalChoice;
}