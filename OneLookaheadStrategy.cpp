#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess(Game& game) {
	// Pré-réserver la mémoire du pool en fonction du nombre total d'objets
	candidatesPool.reserve(game.getAvailableItemsRaw().size());
}

const Item* OneLookaheadStrategy::bestOppPick(const std::vector<Item>& availableItems,
	const std::vector<bool>& isAvailable,
	int oppRemS, int oppRemW, int excludedId) const noexcept {
	const Item* best = nullptr;
	double maxDensity = -1.0;

	const size_t numItems = availableItems.size();
	for (size_t i = 0; i < numItems; ++i) {
		const auto& item = availableItems[i];
		if (isAvailable[item.id] && item.id != excludedId && item.size <= oppRemS && item.weight <= oppRemW) {
			double density = adaptiveDensity(item, oppRemS, oppRemW);
			if (density > maxDensity) {
				maxDensity = density;
				best = &item;
			}
		}
	}
	return best;
}

int OneLookaheadStrategy::pickItem(Game& game) {
	int finalChoice = -1;

	const int remS = game.getRemainingSize();
	const int remW = game.getRemainingWeight();

	int oppUsedS = 0;
	int oppUsedW = 0;
	const auto& oppItems = game.getOpponentItems();
	const size_t numOppItems = oppItems.size();
	for (size_t i = 0; i < numOppItems; ++i) {
		oppUsedS += oppItems[i].size;
		oppUsedW += oppItems[i].weight;
	}

	const int oppRemS = game.getSizeCapacity() - oppUsedS;
	const int oppRemW = game.getWeightCapacity() - oppUsedW;

	const auto& availableItems = game.getAvailableItemsRaw();
	const auto& isAvailable = game.getAvailableMask();

	// Réutilisation du pool de mémoire globale (Évite l'allocation système sur le tas)
	candidatesPool.clear();

	const size_t numTotalItems = availableItems.size();
	for (size_t i = 0; i < numTotalItems; ++i) {
		const auto& item = availableItems[i];
		if (isAvailable[item.id] && item.size <= remS && item.weight <= remW) {
			candidatesPool.push_back({ &item, adaptiveDensity(item, remS, remW) });
		}
	}

	if (!candidatesPool.empty()) {
		const int totalCandidates = static_cast<int>(candidatesPool.size());
		const int limit = std::min(candidateLimit, totalCandidates);

		// OPTIMISATION CRITIQUE : partial_sort au lieu de sort complet (Complexité réduite)
		std::partial_sort(candidatesPool.begin(), candidatesPool.begin() + limit, candidatesPool.end(),
			[](const Candidate& a, const Candidate& b) noexcept {
				return a.density > b.density;
			}
		);

		const Item* oppBaseline = bestOppPick(availableItems, isAvailable, oppRemS, oppRemW);
		const double oppBaselineGain = (oppBaseline != nullptr) ? oppBaseline->cost : 0.0;

		double bestScore = -std::numeric_limits<double>::infinity();
		const Item* bestItem = candidatesPool[0].item;

		for (int i = 0; i < limit; ++i) {
			const auto& cand = candidatesPool[i];
			double oppGainAfter = 0.0;

			if (oppRemS > 0 && oppRemW > 0) {
				const Item* oppAfter = bestOppPick(availableItems, isAvailable, oppRemS, oppRemW, cand.item->id);
				oppGainAfter = (oppAfter != nullptr) ? oppAfter->cost : 0.0;
			}

			const double denial = oppBaselineGain - oppGainAfter;
			const double score = cand.density + (gamma * denial);

			if (score > bestScore) {
				bestScore = score;
				bestItem = cand.item;
			}
		}

		finalChoice = bestItem->id;
	}

	return finalChoice;
}