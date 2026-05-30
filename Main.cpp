#include "Item.hpp"
#include "Game.hpp"
#include "OneLookaheadStrategy.hpp"
#include <iostream>

int main() {
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(nullptr);

	char labelBuffer[128];

	if (std::cin >> labelBuffer) { // "n_items"
		int n = 0;
		std::cin >> n;

		std::cin >> labelBuffer; // "size_capacity"
		int sizeCapacity = 0;
		std::cin >> sizeCapacity;

		std::cin >> labelBuffer; // "weight_capacity"
		int weightCapacity = 0;
		std::cin >> weightCapacity;

		auto strategy = std::make_unique<OneLookaheadStrategy>();
		Game game(n, sizeCapacity, weightCapacity, std::move(strategy));

		for (int i = 0; i < n; ++i) {
			Item item;
			std::cin >> item.id >> item.size >> item.weight >> item.cost;
			game.addItem(item);
		}

		std::cin >> labelBuffer; // "preprocessing"
		std::cin >> labelBuffer; // "5000"
		game.preprocess();

		bool running = (std::cin >> labelBuffer) ? true : false;
		while (running) {
			if (labelBuffer[0] == 't') {
				int takenId = 0;
				std::cin >> takenId;
				game.opponentTook(takenId);
			}
			else if (labelBuffer[0] == 'n') {
				std::cin >> labelBuffer;

				int choice = game.pickItem();
				std::cout << choice << "\n";
				std::cout.flush();
			}
			running = (std::cin >> labelBuffer) ? true : false;
		}
	}
	return 0;
}