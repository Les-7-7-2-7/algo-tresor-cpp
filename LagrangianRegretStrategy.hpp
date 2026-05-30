#ifndef LAGRANGIAN_REGRET_STRATEGY_HPP
#define LAGRANGIAN_REGRET_STRATEGY_HPP

#include "Strategy.hpp"
#include <vector>

class LagrangianRegretStrategy final : public Strategy {
private:
	struct ScoredItem {
		int id;
		int index;
		double dynamicScore;
	};

	double alpha; // Balanced defensive feedback parameter
	std::vector<ScoredItem> evaluationPool;

public:
	explicit LagrangianRegretStrategy(double defensiveFactor = 0.6) noexcept
		: alpha(defensiveFactor) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // LAGRANGIAN_REGRET_STRATEGY_HPP