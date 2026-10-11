cmake_minimum_required(VERSION 3.25)

project(
	IHarvestNG
	VERSION 1.0.5
	DESCRIPTION "Native SKSE reimplementation of IHarvest: a spell that sends wisps to harvest everything around you"
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "In-source builds are not allowed; use a preset (see CMakePresets.json).")
endif()

option(IHNG_BUILD_PLUGIN "Build the SKSE plugin DLL (requires CommonLibSSE-NG)" ON)
option(IHNG_BUILD_TESTS "Build the engine-independent unit tests" ON)

# Engine-independent logic (cast timing, wisp flight, settings model): no CommonLib, unit-testable.
add_subdirectory(src/core)

if(IHNG_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(IHNG_BUILD_PLUGIN)
	add_subdirectory(src/plugin)
endif()
