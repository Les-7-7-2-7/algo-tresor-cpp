#ifndef ONE_LOOKAHEAD_STRATEGY_HPP
#define ONE_LOOKAHEAD_STRATEGY_HPP

#include "Strategy.hpp"
#include "Item.hpp"
#include <limits>

class OneLookaheadStrategy final : public Strategy {
private:
	double gamma;
	int candidateLimit;

	[[nodiscard]] static constexpr double adaptiveDensity(const Item& i, int remS, int remW) noexcept {
		double result = 0.0;
		if (remS > 0 && remW > 0) {
			result = static_cast<double>(i.cost) /
				((static_cast<double>(i.size) / remS) + (static_cast<double>(i.weight) / remW));
		}
		return result;
	}

	[[nodiscard]] const Item* bestOppPick(const std::vector<Item>& availableItems,
		const std::vector<bool>& isAvailable,
		int oppRemS, int oppRemW, int excludedId = -1) const noexcept;

public:
	explicit OneLookaheadStrategy(double g = 0.5, int limit = 60) noexcept
		: gamma(g), candidateLimit(limit) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // ONE_LOOKAHEAD_STRATEGY_HPP