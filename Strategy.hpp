#ifndef STRATEGY_HPP
#define STRATEGY_HPP

#include <vector>

// Forward declaration pour briser la dépendance cyclique
class Game;

// Utilisation d'une classe de base abstraite pure minimale pour le contrat, 
// mais nous l'utiliserons de manière optimisée sans pointeurs nus.
class Strategy {
public:
	virtual ~Strategy() default;
	virtual void preprocess(Game& game) = 0;
	virtual int pickItem(Game& game) = 0;
};

#endif // STRATEGY_HPP