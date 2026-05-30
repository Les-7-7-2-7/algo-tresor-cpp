#include "LagrangianRegretStrategy.hpp"
#include "Game.hpp"
#include <algorithm>
#include <limits>
#include <thread>
#include <future>

LagrangianRegretStrategy::LagrangianRegretStrategy() noexcept : numCores(1) {
	unsigned int cores = std::thread::hardware_concurrency();
	if (cores > 0) {
		numCores = static_cast<int>(cores);
	}
}

void LagrangianRegretStrategy::preprocess(Game& game) {
	size_t totalItems = game.getItemIds().size();
	workers.resize(static_cast<size_t>(numCores));

	std::random_device rd;
	for (int i = 0; i < numCores; ++i) {
		workers[static_cast<size_t>(i)].rng.seed(rd());
		workers[static_cast<size_t>(i)].localMarket.reserve(totalItems);
	}
}

double LagrangianRegretStrategy::runRollout(
	int remS, int remW, int oppRemS, int oppRemW, int startIdx,
	const int* sizes, const int* weights, const int* costs,
	std::vector<int>& localMarket, std::mt19937& rng) const noexcept {

	int myS = remS - sizes[startIdx];
	int myW = remW - weights[startIdx];
	int oppS = oppRemS;
	int oppW = oppRemW;

	double myScore = costs[startIdx];
	double oppScore = 0.0;

	std::shuffle(localMarket.begin(), localMarket.end(), rng);
	size_t marketSize = localMarket.size();

	for (size_t i = 0; i < marketSize; ++i) {
		int idx = localMarket[i];
		if (idx != startIdx) {
			bool processed = false;

			if (sizes[idx] <= myS && weights[idx] <= myW) {
				myS -= sizes[idx];
				myW -= weights[idx];
				myScore += costs[idx];
				processed = true;
			}
			if (!processed && sizes[idx] <= oppS && weights[idx] <= oppW) {
				oppS -= sizes[idx];
				oppW -= weights[idx];
				oppScore += costs[idx];
			}
		}
	}

	return myScore - (0.5 * oppScore);
}

int LagrangianRegretStrategy::pickItem(Game& game) {
	int finalChoice = -1;

	const int remS = game.getRemainingSize();
	const int remW = game.getRemainingWeight();
	const int totalS = game.getSizeCapacity();
	const int totalW = game.getWeightCapacity();

	int oppUsedS = 0;
	int oppUsedW = 0;
	const auto& oppItemIds = game.getOpponentItemIds();
	const auto& idToIdx = game.getIdToIndexMap();
	const int* sizes = game.getSizesPtr();
	const int* weights = game.getWeightsPtr();
	const int* costs = game.getCostsPtr();
	const auto& itemIds = game.getItemIds();
	const auto& bitset = game.getBitset();

	const size_t numOppItems = oppItemIds.size();
#pragma GCC unroll 4
	for (size_t i = 0; i < numOppItems; ++i) {
		int idx = idToIdx[oppItemIds[i]];
		oppUsedS += sizes[idx];
		oppUsedW += weights[idx];
	}

	const int oppRemS = totalS - oppUsedS;
	const int oppRemW = totalW - oppUsedW;
	const size_t numTotalItems = itemIds.size();

	std::vector<int> globalAvailablePool;
	globalAvailablePool.reserve(numTotalItems);
	std::vector<int> validCandidatesIndices;
	validCandidatesIndices.reserve(numTotalItems);

	for (size_t i = 0; i < numTotalItems; ++i) {
		int id = itemIds[i];
		size_t chunk = static_cast<size_t>(id) >> 6;
		size_t bit = static_cast<size_t>(id) & 63;
		bool isAvailable = (bitset[chunk] & (1ULL << bit)) != 0;

		if (isAvailable) {
			globalAvailablePool.push_back(static_cast<int>(i));
			if (sizes[i] <= remS && weights[i] <= remW) {
				validCandidatesIndices.push_back(static_cast<int>(i));
			}
		}
	}

	if (!validCandidatesIndices.empty()) {
		size_t numCandidates = validCandidatesIndices.size();
		std::vector<double> candidateScores(numCandidates, 0.0);
		std::vector<std::future<void>> asyncFutures;
		asyncFutures.reserve(static_cast<size_t>(numCores));

		auto workerTask = [this, &validCandidatesIndices, &globalAvailablePool, &candidateScores,
			sizes, weights, costs, remS, remW, oppRemS, oppRemW](int threadId) {
			size_t tId = static_cast<size_t>(threadId);
			auto& localBuffer = workers[tId].localMarket;
			auto& localRng = workers[tId].rng;

			size_t totalCand = validCandidatesIndices.size();
			size_t chunkSize = (totalCand + static_cast<size_t>(numCores) - 1) / static_cast<size_t>(numCores);
			size_t startCandIdx = tId * chunkSize;
			size_t endCandIdx = std::min(startCandIdx + chunkSize, totalCand);

			const int iterationsPerCandidate = 250;

			for (size_t c = startCandIdx; c < endCandIdx; ++c) {
				int candItemIdx = validCandidatesIndices[c];
				double totalSimulatedScore = 0.0;

				for (int sim = 0; sim < iterationsPerCandidate; ++sim) {
					localBuffer = globalAvailablePool;
					totalSimulatedScore += runRollout(
						remS, remW, oppRemS, oppRemW, candItemIdx,
						sizes, weights, costs, localBuffer, localRng
					);
				}
				candidateScores[c] = totalSimulatedScore / iterationsPerCandidate;
			}
			};

		for (int i = 0; i < numCores; ++i) {
			asyncFutures.push_back(std::async(std::launch::async, workerTask, i));
		}

		for (auto& fut : asyncFutures) {
			fut.wait();
		}

		double highestScore = -std::numeric_limits<double>::infinity();
		int bestItemIndexOnBoard = validCandidatesIndices[0];

		for (size_t i = 0; i < numCandidates; ++i) {
			if (candidateScores[i] > highestScore) {
				highestScore = candidateScores[i];
				bestItemIndexOnBoard = validCandidatesIndices[i];
			}
		}
		finalChoice = itemIds[static_cast<size_t>(bestItemIndexOnBoard)];
	}

	return finalChoice;
}