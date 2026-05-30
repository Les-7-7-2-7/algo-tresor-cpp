#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess(Game& game) {
	size_t totalItems = game.getItemIds().size();
	candidatesPool.reserve(totalItems);

	// Allocate bitsets storage space for each recursive layer of the minimax tree
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

	// Phase-Aware context shifting logic
	if (progress < 0.25) {
		gamma = baseGamma * 0.7; // Early game: Focus slightly more on personal accumulation
	}
	else if (progress > 0.75) {
		gamma = 0.0;             // Endgame: Avoid blocking, greedily pack remaining empty slots
	}
	else {
		gamma = baseGamma * 1.4; // Midgame: Aggressively deny critical resources
	}
	return gamma;
}

double OneLookaheadStrategy::evaluateMinimax(
	int depth, bool isMaxPlayer, double gamma,
	const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
	int myRemS, int myRemW, int oppRemS, int oppRemW) noexcept {

	double outputValue = 0.0;

	// Base condition evaluation
	if (depth == maxDepth || (myRemS <= 0 && myRemW <= 0 && oppRemS <= 0 && oppRemW <= 0)) {
		outputValue = 0.0;
	}
	else {
		size_t currentLayer = static_cast<size_t>(depth);
		size_t nextLayer = currentLayer + 1;
		const auto& currentBitset = layeredBitsets[currentLayer];
		auto& nextBitset = layeredBitsets[nextLayer];

		// Sync next state mask with current state mask
		std::copy(currentBitset.begin(), currentBitset.end(), nextBitset.begin());

		size_t numTotalItems = itemIds.size();
		int activeCandidacyLimit = 4; // Intentionally narrow tree width to keep it O(1) matching real-time constraints

		struct ScoredNode {
			int index;
			double heuristic;
		};
		alignas(16) ScoredNode branches[16];
		int branchCount = 0;

		if (isMaxPlayer) {
			outputValue = -std::numeric_limits<double>::infinity();

#pragma GCC unroll 4
			for (size_t i = 0; i < numTotalItems; ++i) {
				int id = itemIds[i];
				size_t chunk = static_cast<size_t>(id) >> 6;
				size_t bit = static_cast<size_t>(id) & 63;
				bool isAvailable = (currentBitset[chunk] & (1ULL << bit)) != 0;

				if (isAvailable && sizes[i] <= myRemS && weights[i] <= myRemW) {
					double h = evaluateDensity(costs[i], sizes[i], weights[i], myRemS, myRemW);
					if (branchCount < activeCandidacyLimit) {
						branches[branchCount++] = { static_cast<int>(i), h };
					}
				}
			}

			if (branchCount > 0) {
#pragma GCC unroll 4
				for (int i = 0; i < branchCount; ++i) {
					int idx = branches[i].index;
					int id = itemIds[static_cast<size_t>(idx)];
					size_t chunk = static_cast<size_t>(id) >> 6;
					size_t bit = static_cast<size_t>(id) & 63;

					nextBitset[chunk] &= ~(1ULL << bit); // Move simulation

					double value = branches[i].heuristic + evaluateMinimax(
						depth + 1, false, gamma, itemIds, sizes, weights, costs,
						myRemS - sizes[idx], myRemW - weights[idx], oppRemS, oppRemW
					);

					outputValue = std::max(outputValue, value);
					nextBitset[chunk] |= (1ULL << bit); // Unmove backtracking
				}
			}
			else {
				// If forced to pass, evaluate opponent reactions directly
				outputValue = evaluateMinimax(depth + 1, false, gamma, itemIds, sizes, weights, costs, myRemS, myRemW, oppRemS, oppRemW);
			}

		}
		else {
			// Opponent's turn minification evaluation layer
			outputValue = std::numeric_limits<double>::infinity();

#pragma GCC unroll 4
			for (size_t i = 0; i < numTotalItems; ++i) {
				int id = itemIds[i];
				size_t chunk = static_cast<size_t>(id) >> 6;
				size_t bit = static_cast<size_t>(id) & 63;
				bool isAvailable = (currentBitset[chunk] & (1ULL << bit)) != 0;

				if (isAvailable && sizes[i] <= oppRemS && weights[i] <= oppRemW) {
					double h = evaluateDensity(costs[i], sizes[i], weights[i], oppRemS, oppRemW);
					if (branchCount < activeCandidacyLimit) {
						branches[branchCount++] = { static_cast<int>(i), h };
					}
				}
			}

			if (branchCount > 0) {
#pragma GCC unroll 4
				for (int i = 0; i < branchCount; ++i) {
					int idx = branches[i].index;
					int id = itemIds[static_cast<size_t>(idx)];
					size_t chunk = static_cast<size_t>(id) >> 6;
					size_t bit = static_cast<size_t>(id) & 63;

					nextBitset[chunk] &= ~(1ULL << bit);

					// Adversary's value scales down overall game value based on dynamic denial weight parameter
					double value = (-gamma * branches[i].heuristic) + evaluateMinimax(
						depth + 1, true, gamma, itemIds, sizes, weights, costs,
						myRemS, myRemW, oppRemS - sizes[idx], oppRemW - weights[idx]
					);

					outputValue = std::min(outputValue, value);
					nextBitset[chunk] |= (1ULL << bit);
				}
			}
			else {
				outputValue = evaluateMinimax(depth + 1, true, gamma, itemIds, sizes, weights, costs, myRemS, myRemW, oppRemS, oppRemW);
			}
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

	// Contextual Phase-Aware parameter mutation
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
		int bestItemId = candidatesPool[0].id;

		// Synchronize base layer state mask before expanding minimax tree branches
		std::copy(bitset.begin(), bitset.end(), layeredBitsets[0].begin());

#pragma GCC unroll 4
		for (int i = 0; i < limit; ++i) {
			const auto& cand = candidatesPool[i];
			int idx = cand.index;
			size_t chunk = static_cast<size_t>(cand.id) >> 6;
			size_t bit = static_cast<size_t>(cand.id) & 63;

			// Invalidate item in simulated layer 1
			layeredBitsets[1][chunk] = layeredBitsets[0][chunk] & ~(1ULL << bit);

			// Branch computation: evaluate lookahead depth trajectories
			double score = cand.density + evaluateMinimax(
				1, false, dynamicGamma, itemIds, sizes, weights, costs,
				remS - sizes[idx], remW - weights[idx], oppRemS, oppRemW
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