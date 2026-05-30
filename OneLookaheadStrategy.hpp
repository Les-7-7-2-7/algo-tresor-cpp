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

	double baseGamma;
	int candidateLimit;
	int maxDepth;

	std::vector<Candidate> candidatesPool;
	// Layered bitset pool to simulate inventory modification without dynamic allocation overhead
	std::vector<std::vector<uint64_t>> layeredBitsets;

	[[nodiscard]] [[gnu::always_inline]] inline double evaluateDensity(int cost, int size, int weight, int remS, int remW) const noexcept;

	[[nodiscard]] [[gnu::always_inline]] inline double calculateDynamicGamma(int totalItems, int currentTurn) const noexcept;

	// Recursive Minimax evaluation engine with narrow candidate branching
	[[nodiscard]] double evaluateMinimax(
		int depth, bool isMaxPlayer, double gamma,
		const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
		int myRemS, int myRemW, int oppRemS, int oppRemW) noexcept;

public:
	explicit OneLookaheadStrategy(double g = 0.5, int limit = 30, int depth = 3) noexcept
		: baseGamma(g), candidateLimit(limit), maxDepth(depth) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // ONE_LOOKAHEAD_STRATEGY_HPP