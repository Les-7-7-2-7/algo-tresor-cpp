#include "LagrangianRegretStrategy.hpp"
#include "Game.hpp"
#include <algorithm>
#include <limits>

void LagrangianRegretStrategy::preprocess(Game& game) {
	const auto& itemIds = game.getItemIds();
	const int* sizes = game.getSizesPtr();
	const int* weights = game.getWeightsPtr();
	const int* costs = game.getCostsPtr();
	size_t numItems = itemIds.size();

	evaluationPool.reserve(numItems);

	double totalAvailableSize = 0.0;
	double totalAvailableWeight = 0.0;
#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		totalAvailableSize += sizes[i];
		totalAvailableWeight += weights[i];
	}

	double lambdaSize = static_cast<double>(game.getSizeCapacity()) / (totalAvailableSize + 1.0);
	double lambdaWeight = static_cast<double>(game.getWeightCapacity()) / (totalAvailableWeight + 1.0);

	double muS = 1.0 / (lambdaSize + 1e-5);
	double muW = 1.0 / (lambdaWeight + 1e-5);
	double muTotal = muS + muW;
	muS /= muTotal;
	muW /= muTotal;

#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		double displacementCost = (muS * sizes[i]) + (muW * weights[i]);
		double utility = static_cast<double>(costs[i]) / (displacementCost + 1e-5);
		game.updateUtilityInSoA(static_cast<int>(i), utility);
	}
}

int LagrangianRegretStrategy::pickItem(Game& game) {
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
	const double* utilities = game.getUtilitiesPtr();
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

	// Calculate real-time item pool depletion ratios
	double remainingPoolSize = 0.0;
	double remainingPoolWeight = 0.0;
	const size_t numTotalItems = itemIds.size();

#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable) {
			remainingPoolSize += sizes[i];
			remainingPoolWeight += weights[i];
		}
	}

	double currentScarcityS = static_cast<double>(remS + oppRemS) / (remainingPoolSize + 1.0);
	double currentScarcityW = static_cast<double>(remW + oppRemW) / (remainingPoolWeight + 1.0);

	double dynamicMuS = 1.0 / (currentScarcityS + 1e-5);
	double dynamicMuW = 1.0 / (currentScarcityW + 1e-5);
	double dynamicMuTotal = dynamicMuS + dynamicMuW;
	dynamicMuS /= dynamicMuTotal;
	dynamicMuW /= dynamicMuTotal;

	double myFullness = 1.0 - (static_cast<double>(remS + remW) / static_cast<double>(totalS + totalW));
	double currentAlpha = alpha;

	if (myFullness > 0.82) {
		currentAlpha = 0.0;
	}

	evaluationPool.clear();

	// 1. Scan to find the opponent's absolute highest target based on integrated utility matrix
	int oppTargetIdx = -1;
	double maxOppValueScore = -1.0;
#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= oppRemS && weights[i] <= oppRemW) {
			double oppOppCost = (dynamicMuS * sizes[i]) + (dynamicMuW * weights[i]);
			// Multiplies the baseline structural utility by the instant market volatility multiplier
			double oppScore = utilities[i] * (1.0 / (oppOppCost + 1e-5));
			if (oppScore > maxOppValueScore) {
				maxOppValueScore = oppScore;
				oppTargetIdx = static_cast<int>(i);
			}
		}
	}

	// 2. Evaluate my choices combining precalculated utility, time tension, and dynamic regret bonus
#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= remS && weights[i] <= remW) {
			double myOppCost = (dynamicMuS * sizes[i]) + (dynamicMuW * weights[i]);
			// Integrated score: base lagrangian utility dynamically adjusted by the current turn friction
			double myDynamicScore = utilities[i] * (1.0 / (myOppCost + 1e-5));

			double denialRegretValue = (static_cast<int>(i) == oppTargetIdx) ? maxOppValueScore * currentAlpha : 0.0;
			double spacingPenalty = (static_cast<double>(sizes[i]) / remS) + (static_cast<double>(weights[i]) / remW);

			double dynamicFinalScore = myDynamicScore + denialRegretValue - (myFullness * spacingPenalty * 0.15);

			evaluationPool.push_back({ id, static_cast<int>(i), dynamicFinalScore });
		}
	}

	if (!evaluationPool.empty()) {
		double maximalScore = -std::numeric_limits<double>::infinity();
		int targetPoolIndex = 0;
		size_t poolSize = evaluationPool.size();

#pragma GCC unroll 4
		for (size_t i = 0; i < poolSize; ++i) {
			if (evaluationPool[i].dynamicScore > maximalScore) {
				maximalScore = evaluationPool[i].dynamicScore;
				targetPoolIndex = static_cast<int>(i);
			}
		}
		finalChoice = evaluationPool[static_cast<size_t>(targetPoolIndex)].id;
	}

	return finalChoice;
}