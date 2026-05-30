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
	std::vector<std::vector<uint64_t>> layeredBitsets;

	[[nodiscard]] [[gnu::always_inline]] inline double evaluateDensity(int cost, int size, int weight, int remS, int remW) const noexcept;
	[[nodiscard]] [[gnu::always_inline]] inline double calculateDynamicGamma(int totalItems, int currentTurn) const noexcept;

	// True Alpha-Beta Minimax Engine evaluating absolute structural state value
	[[nodiscard]] double evaluateAlphaBeta(
		int depth, bool isMaxPlayer, double gamma, double alpha, double beta,
		const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
		int myRemS, int myRemW, int oppRemS, int oppRemW, double myAccumulatedScore, double oppAccumulatedScore) noexcept;

public:
	// depth = 4 is perfectly sustained by our ultra-fast C++ layout without timing out
	explicit OneLookaheadStrategy(double g = 0.5, int limit = 40, int depth = 4) noexcept
		: baseGamma(g), candidateLimit(limit), maxDepth(depth) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // ONE_LOOKAHEAD_STRATEGY_HPP