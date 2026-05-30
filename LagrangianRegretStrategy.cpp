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

	// 1. Calculate Lagrangian resource pressure vectors across the entire board
	double totalAvailableSize = 0.0;
	double totalAvailableWeight = 0.0;
#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		totalAvailableSize += sizes[i];
		totalAvailableWeight += weights[i];
	}

	// Shadow cost multipliers mirroring multi-dimensional structural scarcity
	double lambdaSize = static_cast<double>(game.getSizeCapacity()) / (totalAvailableSize + 1.0);
	double lambdaWeight = static_cast<double>(game.getWeightCapacity()) / (totalAvailableWeight + 1.0);

	// Invert to compute the strict penalty of consumption
	double muS = 1.0 / (lambdaSize + 1e-5);
	double muW = 1.0 / (lambdaWeight + 1e-5);
	double muTotal = muS + muW;
	muS /= muTotal;
	muW /= muTotal;

	// 2. Map structural utility bounds onto the SoA structure
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
	const int* costs = game.getCostsPtr();
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

	// Dynamic scale tracking inventory emptiness ratios
	double myFullness = 1.0 - (static_cast<double>(remS + remW) / static_cast<double>(totalS + totalW));
	double currentAlpha = alpha;

	// Endgame state mutation: drop defensive gameplay when space becomes extremely critical
	if (myFullness > 0.80) {
		currentAlpha = 0.0;
	}

	evaluationPool.clear();
	const size_t numTotalItems = itemIds.size();

	// 1. Single scan tracking the opponent's maximum structural utility targets
	int oppTargetIdx = -1;
	double maxOppUtility = -1.0;
#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= oppRemS && weights[i] <= oppRemW) {
			if (utilities[i] > maxOppUtility) {
				maxOppUtility = utilities[i];
				oppTargetIdx = static_cast<int>(i);
			}
		}
	}

	// 2. Evaluate my structural assets while calculating the linear regret matrix values
#pragma GCC unroll 4
	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && sizes[i] <= remS && weights[i] <= remW) {
			double myBaseScore = utilities[i];

			// Regret mitigation factor: if I steal their favorite option, calculate the denial vector weight
			double denialRegretValue = (static_cast<int>(i) == oppTargetIdx) ? maxOppUtility * currentAlpha : 0.0;

			// Adaptive volume packing modifier (penalizes large objects if they consume too much of what remains)
			double spacingPenalty = (static_cast<double>(sizes[i]) / remS) + (static_cast<double>(weights[i]) / remW);
			double dynamicScore = myBaseScore + denialRegretValue - (myFullness * spacingPenalty * 0.1);

			evaluationPool.push_back({ id, static_cast<int>(i), dynamicScore });
		}
	}

	if (!evaluationPool.empty()) {
		// High efficiency branchless O(N) max extraction loop replacing STL heap overhead
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