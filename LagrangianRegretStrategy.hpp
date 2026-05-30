#ifndef LAGRANGIAN_REGRET_STRATEGY_HPP
#define LAGRANGIAN_REGRET_STRATEGY_HPP

#include "Strategy.hpp"
#include <vector>
#include <random>

class LagrangianRegretStrategy final : public Strategy {
private:
	struct ThreadWorker {
		std::mt19937 rng;
		std::vector<int> localMarket;
	};

	int numCores;
	std::vector<ThreadWorker> workers;

	// Cleared signature tracking to strictly eradicate compile-time alerts
	[[nodiscard]] double runRollout(
		int remS, int remW, int oppRemS, int oppRemW, int startIdx,
		const int* sizes, const int* weights, const int* costs,
		std::vector<int>& localMarket, std::mt19937& rng) const noexcept;

public:
	explicit LagrangianRegretStrategy() noexcept;

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // LAGRANGIAN_REGRET_STRATEGY_HPP