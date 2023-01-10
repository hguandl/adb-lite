#pragma once

#include <system_error>
#include <version>

#ifdef __cpp_lib_expected

#include <expected>

namespace adb {

template <class T = void> using expected = std::expected<T, std::error_code>;
using unexpected = std::unexpected<std::error_code>;

} // namespace adb

#else // __cpp_lib_expected

#include <tl/expected.hpp>

namespace adb {

template <class T = void> using expected = tl::expected<T, std::error_code>;
using unexpected = tl::unexpected<std::error_code>;

} // namespace adb

#endif // __cpp_lib_expected
