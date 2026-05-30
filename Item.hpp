#ifndef ITEM_HPP
#define ITEM_HPP

#include <string_view>

struct alignas(16) Item {
	int id{ 0 };
	int size{ 0 };
	int weight{ 0 };
	int cost{ 0 };

	[[nodiscard]] constexpr bool operator==(const Item& other) const noexcept {
		return id == other.id;
	}
};

#endif // ITEM_HPP