#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess(Game& game) {
	size_t totalItems = game.getItemIds().size();
	candidatesPool.reserve(totalItems);

	layeredBitsets.resize(static_cast<size_t>(maxDepth) + 1);
#pragma GCC unroll 4
	for (int i = 0; i <= maxDepth; ++i) {
		layeredBitsets[static_cast<size_t>(i)].resize(game.getBitset().size(), 0ULL);
	}
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

	if (progress < 0.20) {
		gamma = baseGamma * 0.5; // Early game: Focus mostly on your own high value capacity
	}
	else if (progress > 0.80) {
		gamma = 0.0;             // Endgame: Avoid defensive plays, just fill remaining spots
	}
	else {
		gamma = baseGamma * 1.5; // Midgame: Highly aggressive denial to destroy opponent options
	}
	return gamma;
}

double OneLookaheadStrategy::evaluateAlphaBeta(
	int depth, bool isMaxPlayer, double gamma, double alpha, double beta,
	const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
	int myRemS, int myRemW, int oppRemS, int oppRemW, double myAccumulatedScore, double oppAccumulatedScore) noexcept {

	double outputValue = 0.0;

	// Terminal node evaluation: Returns actual absolute terminal state delta score
	if (depth == maxDepth || (myRemS <= 0 && myRemW <= 0 && oppRemS <= 0 && oppRemW <= 0)) {
		outputValue = myAccumulatedScore - (gamma * oppAccumulatedScore);
	}
	else {
		size_t currentLayer = static_cast<size_t>(depth);
		size_t nextLayer = currentLayer + 1;
		const auto& currentBitset = layeredBitsets[currentLayer];
		auto& nextBitset = layeredBitsets[nextLayer];

		std::copy(currentBitset.begin(), currentBitset.end(), nextBitset.begin());
		size_t numTotalItems = itemIds.size();

		if (isMaxPlayer) {
			double maxEval = -std::numeric_limits<double>::infinity();
			bool hasValidMove = false;
			double localAlpha = alpha;

			for (size_t i = 0; i < numTotalItems; ++i) {
				int id = itemIds[i];
				size_t chunk = static_cast<size_t>(id) >> 6;
				size_t bit = static_cast<size_t>(id) & 63;
				bool isAvailable = (currentBitset[chunk] & (1ULL << bit)) != 0;

				if (isAvailable && sizes[i] <= myRemS && weights[i] <= myRemW) {
					hasValidMove = true;
					nextBitset[chunk] &= ~(1ULL << bit); // Execute move

					double eval = evaluateAlphaBeta(
						depth + 1, false, gamma, localAlpha, beta, itemIds, sizes, weights, costs,
						myRemS - sizes[i], myRemW - weights[i], oppRemS, oppRemW,
						myAccumulatedScore + costs[i], oppAccumulatedScore
					);

					maxEval = std::max(maxEval, eval);
					localAlpha = std::max(localAlpha, eval);
					nextBitset[chunk] |= (1ULL << bit); // Backtrack

					if (beta <= localAlpha) {
						break; // Beta cut-off
					}
				}
			}

			// Single entry, single exit assignment mapping
			outputValue = hasValidMove ? maxEval : evaluateAlphaBeta(
				depth + 1, false, gamma, localAlpha, beta, itemIds, sizes, weights, costs,
				myRemS, myRemW, oppRemS, oppRemW, myAccumulatedScore, oppAccumulatedScore
			);

		}
		else {
			double minEval = std::numeric_limits<double>::infinity();
			bool hasValidMove = false;
			double localBeta = beta;

			for (size_t i = 0; i < numTotalItems; ++i) {
				int id = itemIds[i];
				size_t chunk = static_cast<size_t>(id) >> 6;
				size_t bit = static_cast<size_t>(id) & 63;
				bool isAvailable = (currentBitset[chunk] & (1ULL << bit)) != 0;

				if (isAvailable && sizes[i] <= oppRemS && weights[i] <= oppRemW) {
					hasValidMove = true;
					nextBitset[chunk] &= ~(1ULL << bit);

					double eval = evaluateAlphaBeta(
						depth + 1, true, gamma, alpha, localBeta, itemIds, sizes, weights, costs,
						myRemS, myRemW, oppRemS - sizes[i], oppRemW - weights[i],
						myAccumulatedScore, oppAccumulatedScore + costs[i]
					);

					minEval = std::min(minEval, eval);
					localBeta = std::min(localBeta, eval);
					nextBitset[chunk] |= (1ULL << bit);

					if (localBeta <= alpha) {
						break; // Alpha cut-off
					}
				}
			}

			outputValue = hasValidMove ? minEval : evaluateAlphaBeta(
				depth + 1, true, gamma, alpha, localBeta, itemIds, sizes, weights, costs,
				myRemS, myRemW, oppRemS, oppRemW, myAccumulatedScore, oppAccumulatedScore
			);
		}
	}

	return outputValue;
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

		// Sort candidates to optimize Alpha-Beta cuts (Pruning triggers significantly faster)
		std::partial_sort(candidatesPool.begin(), candidatesPool.begin() + limit, candidatesPool.end(),
			[](const Candidate& a, const Candidate& b) noexcept {
				return a.density > b.density;
			}
		);

		double bestScore = -std::numeric_limits<double>::infinity();
		int bestItemId = -1;

		std::copy(bitset.begin(), bitset.end(), layeredBitsets[0].begin());

		for (int i = 0; i < limit; ++i) {
			const auto& cand = candidatesPool[i];
			int idx = cand.index;
			size_t chunk = static_cast<size_t>(cand.id) >> 6;
			size_t bit = static_cast<size_t>(cand.id) & 63;

			layeredBitsets[1][chunk] = layeredBitsets[0][chunk] & ~(1ULL << bit);

			// Invoke absolute state Alpha-Beta search instead of local densities aggregation
			double score = evaluateAlphaBeta(
				1, false, dynamicGamma, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
				itemIds, sizes, weights, costs,
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