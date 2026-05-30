#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess(Game& game) {
	candidatesPool.reserve(game.getItemIds().size());
}

[[gnu::always_inline]] inline double OneLookaheadStrategy::evaluateScore(
	int cost, int size, int weight, int remS, int remW, double spaceUrgency) const noexcept {

	double score = 0.0;
	if (remS > 0 && remW > 0) {
		// Space structural friction penalty fraction
		double sizeCost = static_cast<double>(size) / remS;
		double weightCost = static_cast<double>(weight) / remW;

		// Combines items immediate cost value with a penalty for choking remaining space
		score = static_cast<double>(cost) / (1.0 + spaceUrgency * (sizeCost + weightCost));
	}
	return score;
}

int OneLookaheadStrategy::pickItem(Game& game) {
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

	// Dynamic factor determining how critical remaining space footprint is
	double mySpaceUrgency = 1.0 - (static_cast<double>(remS + remW) / static_cast<double>(totalS + totalW));
	double oppSpaceUrgency = 1.0 - (static_cast<double>(oppRemS + oppRemW) / static_cast<double>(totalS + totalW));

	candidatesPool.clear();
	const size_t numTotalItems = itemIds.size();

	// Find top structural choices for opponent beforehand in O(N) to gauge threat vectors
	int oppBestIdx1 = -1;
	double oppBestScore1 = -1.0;

#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable) {
			if (sizes[i] <= oppRemS && weights[i] <= oppRemW) {
				double s = evaluateScore(costs[i], sizes[i], weights[i], oppRemS, oppRemW, oppSpaceUrgency);
				if (s > oppBestScore1) {
					oppBestScore1 = s;
					oppBestIdx1 = static_cast<int>(i);
				}
			}
		}
	}

	// Evaluate my candidates using branchless marginal regret calculations
#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= remS && weights[i] <= remW) {
			double myScore = evaluateScore(costs[i], sizes[i], weights[i], remS, remW, mySpaceUrgency);

			// Regret minimization logic: how much do I deny the opponent by stealing this?
			double denialBonus = 0.0;
			if (oppBestIdx1 != -1) {
				// If I take their absolute favorite item, the regret we inflict is massive
				denialBonus = (static_cast<int>(i) == oppBestIdx1) ? oppBestScore1 * baseGamma : 0.0;
			}

			candidatesPool.push_back({ id, static_cast<int>(i), myScore + denialBonus });
		}
	}

	if (!candidatesPool.empty()) {
		const int totalCandidates = static_cast<int>(candidatesPool.size());
		const int limit = std::min(candidateLimit, totalCandidates);

		std::partial_sort(candidatesPool.begin(), candidatesPool.begin() + limit, candidatesPool.end(),
			[](const Candidate& a, const Candidate& b) noexcept {
				return a.score > b.score;
			}
		);

		finalChoice = candidatesPool[0].id;
	}

	return finalChoice;
}