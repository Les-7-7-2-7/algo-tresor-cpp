#ifndef ONE_LOOKAHEAD_STRATEGY_HPP
#define ONE_LOOKAHEAD_STRATEGY_HPP

#include "Strategy.hpp"
#include <limits>
#include <vector>

class OneLookaheadStrategy final : public Strategy {
private:
	struct Candidate {
		int id;
		int index;
		double score;
	};

	double baseGamma;
	int candidateLimit;

	std::vector<Candidate> candidatesPool;

	[[nodiscard]] [[gnu::always_inline]] inline double evaluateScore(
		int cost, int size, int weight, int remS, int remW, double spaceUrgency) const noexcept;

public:
	explicit OneLookaheadStrategy(double g = 0.5, int limit = 40) noexcept
		: baseGamma(g), candidateLimit(limit) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // ONE_LOOKAHEAD_STRATEGY_HPP