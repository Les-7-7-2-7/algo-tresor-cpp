#include "Item.hpp"
#include "Game.hpp"
#include "OneLookaheadStrategy.hpp"
#include <iostream>
#include <string>

int main() {
	// Optimisation extrême des Entrées/Sorties standards pour g++ sous Debian
	std::ios_base::sync_with_stdio(false);
	std::cin.tie(nullptr);

	std::string token;

	// Lecture des configurations initiales
	std::cin >> token; // "n_items"
	int n = 0;
	std::cin >> n;

	std::cin >> token; // "size_capacity"
	int sizeCapacity = 0;
	std::cin >> sizeCapacity;

	std::cin >> token; // "weight_capacity"
	int weightCapacity = 0;
	std::cin >> weightCapacity;

	// Instanciation de la stratégie demandée
	auto strategy = std::make_unique<OneLookaheadStrategy>();
	Game game(n, sizeCapacity, weightCapacity, std::move(strategy));

	// Remplissage sans allocation inutile
	for (int i = 0; i < n; ++i) {
		Item item;
		std::cin >> item.id >> item.size >> item.weight >> item.cost;
		game.addItem(item);
	}

	std::cin >> token; // "preprocessing"
	std::cin >> token; // "5000"
	game.preprocess();

	// Boucle principale globale (Respect strict SESE)
	bool running = (std::cin >> token) ? true : false;
	while (running) {
		if (token == "taken") {
			int takenId = 0;
			std::cin >> takenId;
			game.opponentTook(takenId);
		}
		else if (token == "next_item") {
			std::cin >> token; // Consomme la contrainte de temps (ex: "500")

			int choice = game.pickItem();
			std::cout << choice << "\n";
			std::cout.flush();
		}

		running = (std::cin >> token) ? true : false;
	}

	return 0;
}