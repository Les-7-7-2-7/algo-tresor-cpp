#include "Item.hpp"
#include "Game.hpp"
#include "DMRStrategy.hpp"
#include <iostream>
#include <string>
#include <sstream>

int main() {
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(nullptr);

	int numItems = 0;
	int sizeCap = 0;
	int weightCap = 0;
	std::unique_ptr<Game> game = nullptr;

	std::string line;
	// Hermetic line-by-line parsing to eradicate IO drift and token desynchronization
	while (std::getline(std::cin, line)) {
		if (line.empty() || line[0] == '\r') {
			continue;
		}

		std::istringstream iss(line);
		std::string command;
		iss >> command;

		if (command == "n_items") {
			iss >> numItems;
		}
		else if (command == "size_capacity") {
			iss >> sizeCap;
		}
		else if (command == "weight_capacity") {
			iss >> weightCap;

			// Late initialization of the Game state machine once capacities are known
			auto strategy = std::make_unique<DMRStrategy>();
			game = std::make_unique<Game>(numItems, sizeCap, weightCap, std::move(strategy));
		}
		else if (command == "preprocessing") {
			int prepTime = 0;
			iss >> prepTime;
			if (game) {
				game->preprocess();
			}
		}
		else if (command == "taken") {
			int takenId = -1;
			if (iss >> takenId && game) {
				game->opponentTook(takenId);
			}
		}
		else if (command == "next_item") {
			int timeLimit = 0;
			iss >> timeLimit;
			if (game) {
				int choice = game->pickItem();
				std::cout << choice << "\n";
				std::cout.flush();
			}
		}
		else {
			// Unrecognized commands or item definition lines implicitly fall here
			// Validating item payload format: ID SIZE WEIGHT COST
			int id = -1, s = -1, w = -1, c = -1;

			// Attempt to parse line as an item if we already found the capacities
			if (game && !command.empty() && std::isdigit(command[0])) {
				std::istringstream itemIss(line);
				if (itemIss >> id >> s >> w >> c) {
					Item newItem{ id, s, w, c };
					game->addItem(newItem);
				}
			}
		}
	}

	return 0;
}