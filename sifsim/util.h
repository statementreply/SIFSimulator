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

template <class CharT, class Traits>
auto ReadAllText(std::basic_istream<CharT, Traits> && ist) {
	return ReadAllText(ist);
}


template <class Compare>
class ReverseComparer {
public:
	ReverseComparer() = default;
	ReverseComparer(const ReverseComparer<Compare> &) = default;
	ReverseComparer(ReverseComparer<Compare> &&) = default;
	ReverseComparer<Compare> & operator =(const ReverseComparer<Compare> &) = default;
	ReverseComparer<Compare> & operator =(ReverseComparer<Compare> &&) = default;

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
ReverseComparer<Compare> MakeReverseComparer(Compare comp) {
	return ReverseComparer<Compare>(comp);
}

template <
	class T,
	class Container = std::vector<T>,
	class Compare = std::less<typename Container::value_type>
> using MinPriorityQueue = std::priority_queue<T, Container, ReverseComparer<Compare>>;


template <class BidirIt, class Compare>
void insertion_sort(BidirIt first, BidirIt last, Compare comp) {
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
void insertion_sort(BidirIt first, BidirIt last) {
	insertion_sort(first, last, std::less<>());
}
