#ifndef ITEM_HPP
#define ITEM_HPP

struct alignas(16) Item {
	int id{ 0 };
	int size{ 0 };
	int weight{ 0 };
	int cost{ 0 };
};

#endif // ITEM_HPP