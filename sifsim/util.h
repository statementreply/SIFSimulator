#pragma once
#include "configure.h"

#include "nativechar.h"
#include <string>
#include <cstdio>
#include <iterator>
#include <queue>
#include <utility>
#include <type_traits>
#include <stdexcept>


#define MACRO_STRING_1(x) #x
#define MACRO_STRING(x) MACRO_STRING_1(x)
#define FILE_LOC __FILE__ ":" MACRO_STRING(__LINE__)


class CFileWrapper {
public:
	CFileWrapper(const char * filename, const char * mode) {
		fp = NativeFopen(ToNative(filename).c_str(), ToNative(mode).c_str());
		if (!fp) {
			throw std::runtime_error("Cannot open file");
		}
	}
	~CFileWrapper() { if (fp) std::fclose(fp); }
	CFileWrapper(const CFileWrapper &) = delete;
	CFileWrapper & operator=(const CFileWrapper &) = delete;
	CFileWrapper(CFileWrapper && other) noexcept : fp(std::exchange(other.fp, nullptr)) {}
	CFileWrapper & operator=(CFileWrapper && other) noexcept {
		FILE * old = std::exchange(fp, std::exchange(other.fp, nullptr));
		if (old) std::fclose(old);
		return *this;
	}
	operator FILE *() { return fp; }
private:
	std::FILE * fp;
};


template <class Compare>
class ReverseComparer {
public:
	ReverseComparer() = default;
	ReverseComparer(const ReverseComparer &) = default;
	ReverseComparer(ReverseComparer &&) = default;
	ReverseComparer & operator =(const ReverseComparer &) = default;
	ReverseComparer & operator =(ReverseComparer &&) = default;

	explicit ReverseComparer(const Compare & comp) : comp(comp) {}
	explicit ReverseComparer(Compare && comp) : comp(std::forward<Compare>(comp)) {}

	template <class U, class V>
	constexpr bool operator ()(U && u, V && v) const {
		return comp(std::forward<V>(v), std::forward<U>(u));
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


template <class IntType, class IntType2 = IntType>
typename std::enable_if<
	std::is_convertible<IntType2, IntType>::value,
	void
>::type swapBits(IntType & a, IntType & b, IntType2 mask) {
	IntType t = (a ^ b) & mask;
	a ^= t;
	b ^= t;
}


#if USE_SSE_4_1_ROUND
#include <smmintrin.h>
inline double Floor(double x) {
	__m128d m = _mm_set_sd(x);
	return _mm_cvtsd_f64(_mm_floor_sd(m, m));
}

inline double Ceil(double x) {
	__m128d m = _mm_set_sd(x);
	return _mm_cvtsd_f64(_mm_ceil_sd(m, m));
}
#else
#include <cmath>
inline double Floor(double x) {
	return std::floor(x);
}

inline double Ceil(double x) {
	return std::ceil(x);

}
#endif
