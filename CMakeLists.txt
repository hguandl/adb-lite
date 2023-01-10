cmake_minimum_required(VERSION 3.12)
project(adb-lite CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
  add_compile_options(/W4 /WX /MT)
  add_compile_options(-D_WIN32_WINNT=0x0601)
else()
  add_compile_options(-fPIC -Wno-deprecated-declarations)
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif (MSVC)

add_library(adb-lite STATIC src/client.cpp src/error.cpp src/protocol.cpp)
target_include_directories(adb-lite PRIVATE include/adb-lite)
target_include_directories(adb-lite INTERFACE include)

install(DIRECTORY include
         DESTINATION .
         FILES_MATCHING PATTERN "*.hpp")

install(TARGETS adb-lite
        PUBLIC_HEADER DESTINATION include
        DESTINATION lib)

if (BUILD_TEST)
  add_executable(adb-test examples/main.cpp)
  target_link_libraries(adb-test PRIVATE adb-lite)
endif (BUILD_TEST)

###### ASIO ######

include(FetchContent)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(
  asio
  URL https://sourceforge.net/projects/asio/files/asio/1.24.0%20%28Stable%29/asio-1.24.0.tar.bz2
  URL_HASH SHA256=8976812c24a118600f6fcf071a20606630a69afe4c0abee3b0dea528e682c585
)
FetchContent_MakeAvailable(asio)
target_include_directories(adb-lite PUBLIC ${asio_SOURCE_DIR}/include)

###### END ASIO ######

###### BEGIN expected ######

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
  cmake_policy(SET CMP0077 NEW)
endif()

FetchContent_Declare(
  expected
  URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.0.0.tar.gz
  URL_HASH SHA256=8f5124085a124113e75e3890b4e923e3a4de5b26a973b891b3deb40e19c03cee
)
set(EXPECTED_ENABLE_TESTS OFF)
FetchContent_MakeAvailable(expected)
target_include_directories(adb-lite PUBLIC ${expected_SOURCE_DIR}/include)

###### END expected ######
