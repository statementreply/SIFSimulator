#pragma once

#if __has_include(<optional>)
#include <optional>
using std::optional;
using std::nullopt;
#else
#include <experimental/optional>
using std::experimental::optional;
using std::experimental::nullopt;
#endif
