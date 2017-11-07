#pragma once

#include <string>
#include <iostream>
#include <iterator>


template <class CharT, class Traits>
auto ReadAllText(std::basic_istream<CharT, Traits> & ist) {
	return std::basic_string<CharT, Traits>(
		std::istream_iterator<CharT, CharT, Traits>(ist),
		std::istream_iterator<CharT, CharT, Traits>());
}
