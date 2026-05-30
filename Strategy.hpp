#ifndef STRATEGY_HPP
#define STRATEGY_HPP

class Game; // Forward declaration pure, aucun include ici.

class Strategy {
public:
	virtual ~Strategy() {};
	virtual void preprocess(Game& game) = 0;
	virtual int pickItem(Game& game) = 0;
};

#endif // STRATEGY_HPP