#pragma once
#include "configure.h"

#include <string>
#include <iostream>
#include <iterator>
#include <queue>
#include <utility>


template <class CharT, class Traits>
auto readAllText(std::basic_istream<CharT, Traits> & ist) {
	return std::basic_string<CharT, Traits>(
		std::istream_iterator<CharT, CharT, Traits>(ist),
		std::istream_iterator<CharT, CharT, Traits>());
}

template <class CharT, class Traits>
auto readAllText(std::basic_istream<CharT, Traits> && ist) {
	return readAllText(ist);
}


template <class Compare>
class ReverseComparer {
public:
	ReverseComparer() = default;
	ReverseComparer(const ReverseComparer &) = default;
	ReverseComparer(ReverseComparer &&) = default;
	ReverseComparer & operator =(const ReverseComparer &) = default;
	ReverseComparer & operator =(ReverseComparer &&) = default;

	explicit ReverseComparer(const Compare & comp) : comp(comp) {}
	explicit ReverseComparer(Compare && comp) : comp(comp) {}

	template <class U, class V>
	constexpr bool operator ()(U && u, V && v) & {
		return comp(std::forward<U>(v), std::forward<V>(u));
	}

	template <class U, class V>
	bool operator ()(U && u, V && v) && {
		return comp(std::forward<U>(v), std::forward<V>(u));
	}

	template <class U, class V>
	bool operator ()(U && u, V && v) const & {
		return comp(std::forward<U>(v), std::forward<V>(u));
	}

	template <class U, class V>
	bool operator ()(U && u, V && v) const && {
		return comp(std::forward<U>(v), std::forward<V>(u));
	}

private:
	Compare comp;
};

template <class Compare>
ReverseComparer<Compare> makeReverseComparer(Compare comp) {
	return ReverseComparer<Compare>(comp);
}

template <
	class T,
	class Container = std::vector<T>,
	class Compare = std::less<typename Container::value_type>
> using MinPriorityQueue = std::priority_queue<T, Container, ReverseComparer<Compare>>;

template <class T, class Container, class Compare>
void clear(std::priority_queue<T, Container, Compare> & queue) {
	while (!queue.empty()) {
		queue.pop();
	}
}


template <class BidirIt, class Compare>
void insertionSort(BidirIt first, BidirIt last, Compare comp) {
	for (auto curr = first; curr != last; ++curr) {
		for (auto a = curr; a != first ; --a) {
			auto b = std::prev(a);
			if (!comp(*a, *b)) {
				break;
			}
			std::iter_swap(a, b);
		}
	}
}

template <class BidirIt>
void insertionSort(BidirIt first, BidirIt last) {
	insertionSort(first, last, std::less<>());
}


template <class IntType>
void swapBits(IntType & a, IntType & b, IntType mask) {
	IntType t = (a ^ b) & mask;
	a ^= t;
	b ^= t;
}
