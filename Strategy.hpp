#ifndef STRATEGY_HPP
#define STRATEGY_HPP

class Game;

class Strategy {
public:
	virtual ~Strategy() = default;
	virtual void preprocess(Game& game) = 0;
	virtual int pickItem(Game& game) = 0;
};

#endif // STRATEGY_HPP