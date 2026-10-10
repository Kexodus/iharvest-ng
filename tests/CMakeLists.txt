# Unit tests for ihng_core (no game, no CommonLib).
find_package(Catch2 3 CONFIG REQUIRED)

file(GLOB_RECURSE IHNG_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(ihng_tests ${IHNG_TEST_SOURCES})
target_link_libraries(ihng_tests PRIVATE ihng::core Catch2::Catch2WithMain)
if(MSVC)
	target_compile_options(ihng_tests PRIVATE /W4 /permissive- /utf-8)
endif()

include(Catch)
catch_discover_tests(ihng_tests)
