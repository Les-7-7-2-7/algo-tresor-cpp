#include "DMRStrategy.hpp"
#include "Game.hpp"
#include <algorithm>
#include <limits>

void DMRStrategy::preprocess(Game& game) {
	sortedMarket.reserve(game.getItemIds().size());
}

double DMRStrategy::simulateFractionalFill(
	int remS, int remW, int excludeIndex,
	const std::vector<MarketItem>& market,
	const int* sizes, const int* weights, const int* costs) const noexcept {

	double projectedScore = 0.0;
	int currentS = remS;
	int currentW = remW;
	size_t marketSize = market.size();

	for (size_t i = 0; i < marketSize; ++i) {
		int idx = market[i].index;

		if (idx != excludeIndex) {
			int itemS = sizes[idx];
			int itemW = weights[idx];

			if (itemS <= currentS && itemW <= currentW) {
				currentS -= itemS;
				currentW -= itemW;
				projectedScore += static_cast<double>(costs[idx]);
			}
			else {
				double ratioS = (itemS > 0) ? (static_cast<double>(currentS) / itemS) : 1.0;
				double ratioW = (itemW > 0) ? (static_cast<double>(currentW) / itemW) : 1.0;
				double minRatio = std::min({ ratioS, ratioW, 1.0 });

				if (minRatio > 0.0) {
					projectedScore += minRatio * static_cast<double>(costs[idx]);
				}
				break;
			}
		}
	}

	return projectedScore;
}

int DMRStrategy::pickItem(Game& game) {
	int finalChoice = -1;

	const int remS = game.getRemainingSize();
	const int remW = game.getRemainingWeight();
	const int oppRemS = game.getOpponentRemainingSize();
	const int oppRemW = game.getOpponentRemainingWeight();

	const auto& itemIds = game.getItemIds();
	const int* sizes = game.getSizesPtr();
	const int* weights = game.getWeightsPtr();
	const int* costs = game.getCostsPtr();

	const size_t numItems = itemIds.size();

	double totalAvailableS = 0.0;
	double totalAvailableW = 0.0;

#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		int id = itemIds[i];
		if (game.isItemAvailable(id)) {
			totalAvailableS += sizes[i];
			totalAvailableW += weights[i];
		}
	}

	double pressureS = 1.0 / (static_cast<double>(remS + oppRemS) + 1.0);
	double pressureW = 1.0 / (static_cast<double>(remW + oppRemW) + 1.0);

	sortedMarket.clear();

#pragma GCC unroll 4
	for (size_t i = 0; i < numItems; ++i) {
		int id = itemIds[i];
		if (game.isItemAvailable(id)) {
			double footprintCost = (pressureS * sizes[i]) + (pressureW * weights[i]);
			double density = static_cast<double>(costs[i]) / (footprintCost + 1e-6);
			sortedMarket.push_back({ static_cast<int>(i), density });
		}
	}

	std::sort(sortedMarket.begin(), sortedMarket.end(), [](const MarketItem& a, const MarketItem& b) noexcept {
		return a.dynamicDensity > b.dynamicDensity;
		});

	double bestEvaluationScore = -std::numeric_limits<double>::infinity();
	int bestCandidateId = -1;

	for (size_t i = 0; i < numItems; ++i) {
		int id = itemIds[i];

		if (game.isItemAvailable(id) && sizes[i] <= remS && weights[i] <= remW) {
			int idx = static_cast<int>(i);

			double myProjectedFuture = static_cast<double>(costs[idx]) +
				simulateFractionalFill(remS - sizes[idx], remW - weights[idx], idx, sortedMarket, sizes, weights, costs);

			double oppProjectedFuture = 0.0;
			if (sizes[idx] <= oppRemS && weights[idx] <= oppRemW) {
				oppProjectedFuture = static_cast<double>(costs[idx]) +
					simulateFractionalFill(oppRemS - sizes[idx], oppRemW - weights[idx], idx, sortedMarket, sizes, weights, costs);
			}

			double evaluationScore = myProjectedFuture + (alphaDenial * oppProjectedFuture);

			if (evaluationScore > bestEvaluationScore) {
				bestEvaluationScore = evaluationScore;
				bestCandidateId = id;
			}
		}
	}

	if (bestCandidateId != -1) {
		finalChoice = bestCandidateId;
	}

	return finalChoice;
}