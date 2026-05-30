#ifndef ONE_LOOKAHEAD_STRATEGY_HPP
#define ONE_LOOKAHEAD_STRATEGY_HPP

#include "Strategy.hpp"
#include <vector>

class OneLookaheadStrategy final : public Strategy {
private:
	struct Candidate {
		int id;
		int index;
		double score;
	};

	double baseGamma;
	std::vector<Candidate> candidatesPool;

public:
	explicit OneLookaheadStrategy(double g = 0.5) noexcept : baseGamma(g) {}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // ONE_LOOKAHEAD_STRATEGY_HPP