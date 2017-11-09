#pragma once

#include <string>
#include <iostream>
#include <iterator>
#include <queue>
#include <utility>


template <class CharT, class Traits>
auto ReadAllText(std::basic_istream<CharT, Traits> & ist) {
	return std::basic_string<CharT, Traits>(
		std::istream_iterator<CharT, CharT, Traits>(ist),
		std::istream_iterator<CharT, CharT, Traits>());
}


template <class Compare>
class ReverseCompare {
public:
	template <class ... Args>
	ReverseCompare(Args && ... args) : comp(std::forward(args)...) {};

	template <class U, class V>
	bool operator ()(U && u, V && v) & {
		return std::forward(comp(std::forward(v), std::forward(u)));
	}

	template <class U, class V>
	bool operator ()(U && u, V && v) && {
		return std::forward(comp(std::forward(v), std::forward(u)));
	}

	template <class U, class V>
	bool operator ()(U && u, V && v) const & {
		return std::forward(comp(std::forward(v), std::forward(u)));
	}

	template <class U, class V>
	bool operator ()(U && u, V && v) const && {
		return std::forward(comp(std::forward(v), std::forward(u)));
	}

private:
	Compare comp;
};

template <
	class T,
	class Container = std::vector<T>,
	class Compare = std::less<typename Container::value_type>
> class MinPriorityQueue
	: public std::priority_queue<T, Container, ReverseCompare<Compare>> {
};


template <class BidirIt, class Compare>
void insertion_sort(BidirIt first, BidirIt last, Compare comp) {
	if (first == last) {
		return;
	}
	for (auto curr = std::next(first); curr != last; ++curr) {
		for (auto a = curr, b = std::prev(curr); ; --a, --b) {
			if (comp(*a, *b)) {
				std::iter_swap(a, b);
			} else {
				break;
			}
			if (b == first) {
				break;
			}
		}
	}
}

template <class BidirIt>
void insertion_sort(BidirIt first, BidirIt last) {
	insertion_sort(first, last, std::less<>());
}
