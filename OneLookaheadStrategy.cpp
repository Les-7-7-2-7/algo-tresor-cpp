#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess(Game& game) {
	candidatesPool.reserve(game.getItemIds().size());
}

[[gnu::always_inline]] inline double OneLookaheadStrategy::evaluateDensity(int cost, int size, int weight, int remS, int remW) const noexcept {
	double result = 0.0;
	if (remS > 0 && remW > 0) {
		result = static_cast<double>(cost) /
			((static_cast<double>(size) / remS) + (static_cast<double>(weight) / remW));
	}
	return result;
}

[[gnu::always_inline]] inline int OneLookaheadStrategy::bestOpponentIndex(
	const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
	const std::vector<uint64_t>& bitset, [[maybe_unused]] const std::vector<int>& idToIdx,
	int oppRemS, int oppRemW, int excludedId) const noexcept {

	int bestIndex = -1;
	double maxDensity = -1.0;
	const size_t numItems = itemIds.size();

	// Attribut placé sur la même ligne que le for pour éviter d'être ignoré par g++
	[[gnu::unroll(4)]] for (size_t i = 0; i < numItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable && id != excludedId && sizes[i] <= oppRemS && weights[i] <= oppRemW) {
			double density = evaluateDensity(costs[i], sizes[i], weights[i], oppRemS, oppRemW);
			if (density > maxDensity) {
				maxDensity = density;
				bestIndex = static_cast<int>(i);
			}
		}
	}
	return bestIndex;
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
	[[gnu::unroll(4)]] for (size_t i = 0; i < numOppItems; ++i) {
		int idx = idToIdx[oppItemIds[i]];
		oppUsedS += sizes[idx];
		oppUsedW += weights[idx];
	}

	const int oppRemS = game.getSizeCapacity() - oppUsedS;
	const int oppRemW = game.getWeightCapacity() - oppUsedW;

	candidatesPool.clear();
	const size_t numTotalItems = itemIds.size();

	[[gnu::unroll(4)]] for (size_t i = 0; i < numTotalItems; ++i) {
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

		int oppBestIdx1 = -1;
		int oppBestIdx2 = -1;
		double maxDensity1 = -1.0;
		double maxDensity2 = -1.0;

		[[gnu::unroll(4)]] for (size_t i = 0; i < numTotalItems; ++i) {
			int id = itemIds[i];
			size_t chunk = static_cast<size_t>(id) >> 6;
			size_t bit = static_cast<size_t>(id) & 63;
			bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

			if (isAvailable && sizes[i] <= oppRemS && weights[i] <= oppRemW) {
				double density = evaluateDensity(costs[i], sizes[i], weights[i], oppRemS, oppRemW);
				if (density > maxDensity1) {
					maxDensity2 = maxDensity1;
					oppBestIdx2 = oppBestIdx1;
					maxDensity1 = density;
					oppBestIdx1 = static_cast<int>(i);
				}
				else if (density > maxDensity2) {
					maxDensity2 = density;
					oppBestIdx2 = static_cast<int>(i);
				}
			}
		}

		const double oppBaselineGain = (oppBestIdx1 != -1) ? costs[oppBestIdx1] : 0.0;
		double bestScore = -std::numeric_limits<double>::infinity();
		int bestItemId = candidatesPool[0].id;

		[[gnu::unroll(4)]] for (int i = 0; i < limit; ++i) {
			const auto& cand = candidatesPool[i];
			double oppGainAfter = 0.0;

			if (oppRemS > 0 && oppRemW > 0) {
				const bool isStealingBest1 = (oppBestIdx1 != -1 && cand.id == itemIds[oppBestIdx1]);

				oppGainAfter = isStealingBest1
					? ((oppBestIdx2 != -1) ? costs[oppBestIdx2] : 0.0)
					: ((oppBestIdx1 != -1) ? costs[oppBestIdx1] : 0.0);

				if (__builtin_expect((oppBestIdx1 != -1 && cand.id == itemIds[oppBestIdx1] && oppBestIdx2 == -1), false)) {
					int fallbackIdx = bestOpponentIndex(itemIds, sizes, weights, costs, bitset, idToIdx, oppRemS, oppRemW, cand.id);
					oppGainAfter = (fallbackIdx != -1) ? costs[fallbackIdx] : 0.0;
				}
			}

			const double denial = oppBaselineGain - oppGainAfter;
			const double score = cand.density + (gamma * denial);

			if (score > bestScore) {
				bestScore = score;
				bestItemId = cand.id;
			}
		}

		finalChoice = bestItemId;
	}

	return finalChoice;
}