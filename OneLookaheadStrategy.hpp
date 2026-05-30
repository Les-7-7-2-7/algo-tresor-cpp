#ifndef ONE_LOOKAHEAD_STRATEGY_HPP
#define ONE_LOOKAHEAD_STRATEGY_HPP

#include "Strategy.hpp"
#include <limits>
#include <vector>
#include <cstdint>

class OneLookaheadStrategy final : public Strategy {
private:
	struct Candidate {
		int id;
		int index;
		double density;
	};

	double gamma;
	int candidateLimit;

	std::vector<Candidate> candidatesPool;

	[[nodiscard]] [[gnu::always_inline]] inline double evaluateDensity(int cost, int size, int weight, int remS, int remW) const noexcept;

	[[nodiscard]] [[gnu::always_inline]] inline int bestOpponentIndex(
		const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
		const std::vector<uint64_t>& bitset, const std::vector<int>& idToIdx,
		int oppRemS, int oppRemW, int excludedId = -1) const noexcept;

public:
	explicit OneLookaheadStrategy(double g = 0.5, int limit = 60) noexcept
		: gamma(g), candidateLimit(limit) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // ONE_LOOKAHEAD_STRATEGY_HPP