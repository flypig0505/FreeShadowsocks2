cmake_minimum_required(VERSION 3.0)

project(FreeShadowsocks)

# compile in release  mode by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(BUILD_CPR_TESTS OFF CACHE BOOL "Turn off tests for cpr")
set(BUILD_CURL_EXE OFF CACHE BOOL "Turn off build cURL executable.")
set(BUILD_CURL_TESTS OFF CACHE BOOL "Turn off build cURL tests.")
#set(CURL_STATICLIB ON CACHE BOOL "Turn on to build libcurl with static linking.")

add_subdirectory(deps/cpr) # defines CPR_INCLUDE_DIRS and CPR_LIBRARIES

add_definitions(-DRAPIDJSON_HAS_STDSTRING=1)
include_directories(${CPR_INCLUDE_DIRS} deps/rapidjson/include)
add_executable(FreeShadowsocks src/main.cpp)
source_group(src FILES src/main.cpp)
target_link_libraries(FreeShadowsocks ${CPR_LIBRARIES})
