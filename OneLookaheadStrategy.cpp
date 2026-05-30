#include "OneLookaheadStrategy.hpp"
#include "Game.hpp"
#include <algorithm>

void OneLookaheadStrategy::preprocess([[maybe_unused]] Game& game) {}

const Item* OneLookaheadStrategy::bestOppPick(const std::vector<Item>& availableItems,
	const std::vector<bool>& isAvailable,
	int oppRemS, int oppRemW, int excludedId) const noexcept {
	const Item* best = nullptr;
	double maxDensity = -1.0;

	for (const auto& item : availableItems) {
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
	for (const auto& item : game.getOpponentItems()) {
		oppUsedS += item.size;
		oppUsedW += item.weight;
	}

	const int oppRemS = game.getSizeCapacity() - oppUsedS;
	const int oppRemW = game.getWeightCapacity() - oppUsedW;

	const auto& availableItems = game.getAvailableItemsRaw();
	const auto& isAvailable = game.getAvailableMask();

	// Collecte et tri des candidats
	struct Candidate {
		const Item* item;
		double density;
	};

	std::vector<Candidate> candidates;
	candidates.reserve(availableItems.size());

	for (const auto& item : availableItems) {
		if (isAvailable[item.id] && item.size <= remS && item.weight <= remW) {
			candidates.push_back({ &item, adaptiveDensity(item, remS, remW) });
		}
	}

	if (!candidates.empty()) {
		std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) noexcept {
			return a.density > b.density;
			});

		if (static_cast<int>(candidates.size()) > candidateLimit) {
			candidates.resize(candidateLimit);
		}

		const Item* oppBaseline = bestOppPick(availableItems, isAvailable, oppRemS, oppRemW);
		const double oppBaselineGain = (oppBaseline != nullptr) ? oppBaseline->cost : 0.0;

		double bestScore = -std::numeric_limits<double>::infinity();
		const Item* bestItem = candidates[0].item;

		for (const auto& cand : candidates) {
			double oppGainAfter = 0.0;
			if (oppRemS > 0 && oppRemW > 0) {
				const Item* oppAfter = bestOppPick(availableItems, isAvailable, oppRemS, oppRemW, cand.item->id);
				oppGainAfter = (oppAfter != nullptr) ? oppAfter->cost : 0.0;
			}

			double denial = oppBaselineGain - oppGainAfter;
			double score = cand.density + gamma * denial;

			if (score > bestScore) {
				bestScore = score;
				bestItem = cand.item;
			}
		}

		finalChoice = bestItem->id;
	}

	return finalChoice;
}