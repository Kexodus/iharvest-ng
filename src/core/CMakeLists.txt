# ihng_core: engine-independent logic (no CommonLib / game headers).
file(GLOB_RECURSE IHNG_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(ihng_core STATIC ${IHNG_CORE_SOURCES})
add_library(ihng::core ALIAS ihng_core)
target_include_directories(ihng_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(ihng_core PUBLIC cxx_std_23)
if(MSVC)
	target_compile_options(ihng_core PRIVATE /W4 /permissive- /utf-8)
endif()
