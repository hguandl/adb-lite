#pragma once

#include <system_error>
#include <version>

#ifdef __cpp_lib_expected
#include <expected>
#else
#include <tl/expected.hpp>

namespace std {

template <class E> using unexpected = tl::unexpected<E>;

template <class E> using bad_expected_access = tl::bad_expected_access<E>;

using unexpect_t = tl::unexpect_t;

template <class T, class E> using expected = tl::expected<T, E>;

} // namespace std
#endif // __cpp_lib_expected

namespace adb {

template <class T = void> using expected = std::expected<T, std::error_code>;
using unexpected = std::unexpected<std::error_code>;

} // namespace adb
