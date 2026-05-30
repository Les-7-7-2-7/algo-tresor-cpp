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

	std::vector<Candidate> candidatesPool;
	// Persistant state buffers to avoid allocations during fast rollouts
	std::vector<uint64_t> rolloutBitset;

	[[nodiscard]] [[gnu::always_inline]] inline double evaluateDensity(int cost, int size, int weight, int remS, int remW) const noexcept;
	[[nodiscard]] [[gnu::always_inline]] inline double calculateDynamicGamma(int totalItems, int currentTurn) const noexcept;

	// Simulates the entire rest of the game deterministically to find the true final outcome
	[[nodiscard]] double computeDeterministicRollout(
		double gamma, const std::vector<int>& itemIds, const int* sizes, const int* weights, const int* costs,
		int initialMyRemS, int initialMyRemW, int initialOppRemS, int initialOppRemW,
		double initialMyScore, double initialOppScore) noexcept;

public:
	explicit OneLookaheadStrategy(double g = 0.5, int limit = 40) noexcept
		: baseGamma(g), candidateLimit(limit) {
	}

	void preprocess(Game& game) override;
	int pickItem(Game& game) override;
};

#endif // ONE_LOOKAHEAD_STRATEGY_HPP