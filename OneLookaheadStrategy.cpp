#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess(Game& game) {
	const auto& itemIds = game.getItemIds();
	const int* sizes = game.getSizesPtr();
	const int* weights = game.getWeightsPtr();
	const int* costs = game.getCostsPtr();
	size_t numItems = itemIds.size();

	candidatesPool.reserve(numItems);

	// 1. Core Macro Resource Scarcity Analysis
	double totalGameSize = 0.0;
	double totalGameWeight = 0.0;
#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		totalGameSize += sizes[i];
		totalGameWeight += weights[i];
	}

	// Determine which dimension restricts the board configuration state space the most
	double sizeScarcityFactor = static_cast<double>(game.getSizeCapacity()) / (totalGameSize + 1.0);
	double weightScarcityFactor = static_cast<double>(game.getWeightCapacity()) / (totalGameWeight + 1.0);

	// Low factor means the resource is extremely rare and highly contested
	double alphaS = 1.0 / (sizeScarcityFactor + 1e-6);
	double alphaW = 1.0 / (weightScarcityFactor + 1e-6);
	double totalAlpha = alphaS + alphaW;
	alphaS /= totalAlpha;
	alphaW /= totalAlpha;

	// 2. Oracle Bounds Valuation Graph mapping
#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		double resourceCost = (alphaS * sizes[i]) + (alphaW * weights[i]);
		double score = static_cast<double>(costs[i]) / (resourceCost + 1e-6);
		game.updateOracleScoreInSoA(static_cast<int>(i), score);
	}
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
	const double* oracleScores = game.getOracleScoresPtr();
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

	candidatesPool.clear();
	const size_t numTotalItems = itemIds.size();

	// Find the opponent's absolute highest threat target in O(N)
	int oppBestIdx1 = -1;
	double oppMaxOracle = -1.0;
#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= oppRemS && weights[i] <= oppRemW) {
			if (oracleScores[i] > oppMaxOracle) {
				oppMaxOracle = oracleScores[i];
				oppBestIdx1 = static_cast<int>(i);
			}
		}
	}

	// Evaluate my valid choices incorporating branchless threat interference bonuses
#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= remS && weights[i] <= remW) {
			double myBaseScore = oracleScores[i];

			// Strategic denial logic: if this item is exactly what opponent wants most, increase priority
			double denialRegretBonus = (static_cast<int>(i) == oppBestIdx1) ? oppMaxOracle * baseGamma : 0.0;

			candidatesPool.push_back({ id, static_cast<int>(i), myBaseScore + denialRegretBonus });
		}
	}

	if (!candidatesPool.empty()) {
		// Linear single-scan extraction of the absolute best element (Faster than sorting)
		double maxCandidateScore = -1.0;
		int bestPoolIndex = 0;
		size_t poolSize = candidatesPool.size();

#pragma GCC unroll 4
		for (size_t i = 0; i < poolSize; ++i) {
			if (candidatesPool[i].score > maxCandidateScore) {
				maxCandidateScore = candidatesPool[i].score;
				bestPoolIndex = static_cast<int>(i);
			}
		}
		finalChoice = candidatesPool[static_cast<size_t>(bestPoolIndex)].id;
	}

	return finalChoice;
}