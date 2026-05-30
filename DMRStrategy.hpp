#ifndef DMR_STRATEGY_HPP
#define DMR_STRATEGY_HPP

#include "Strategy.hpp"
#include <vector>

class DMRStrategy final : public Strategy {
private:
	struct MarketItem {
		int index;
		double dynamicDensity;
	};

	double alphaDenial;
	std::vector<MarketItem> sortedMarket;

	[[nodiscard]] double simulateFractionalFill(
		int remS, int remW, int excludeIndex,
		const std::vector<MarketItem>& market,
		const int* sizes, const int* weights, const int* costs) const noexcept;

public:
	explicit DMRStrategy(double denialFactor = 0.6) noexcept
		: alphaDenial(denialFactor) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // DMR_STRATEGY_HPP