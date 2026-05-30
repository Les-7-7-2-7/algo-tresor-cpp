#ifndef SUBSTITUTION_ORACLE_STRATEGY_HPP
#define SUBSTITUTION_ORACLE_STRATEGY_HPP

#include "Strategy.hpp"
#include <vector>

class SubstitutionOracleStrategy final : public Strategy {
private:
	struct ItemSortProxy {
		int index;
		double density;
	};

	std::vector<ItemSortProxy> sortedProxies;

public:
	explicit SubstitutionOracleStrategy() noexcept = default;

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // SUBSTITUTION_ORACLE_STRATEGY_HPP