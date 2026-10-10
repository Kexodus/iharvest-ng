# iHarvestNG.dll - the SKSE plugin (SE 1.5.97 + AE 1.6.x + VR in one DLL via CommonLibSSE-NG).
# Only this layer may touch RE::/SKSE::.

set(IHNG_COMMONLIB_DIR "${PROJECT_SOURCE_DIR}/extern/CommonLibSSE-NG")
if(NOT EXISTS "${IHNG_COMMONLIB_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "CommonLibSSE-NG submodule missing. Run: git submodule update --init --recursive")
endif()

# All three runtimes ON: the multi-runtime layouts are what make one DLL work on SE/AE/VR.
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build CommonLibSSE-NG's own unit tests" FORCE)

add_subdirectory("${IHNG_COMMONLIB_DIR}" "${CMAKE_BINARY_DIR}/extern/CommonLibSSE-NG" SYSTEM EXCLUDE_FROM_ALL)

get_target_property(_ihng_commonlib_imported CommonLibSSE IMPORTED)
if(MSVC AND NOT _ihng_commonlib_imported)
	target_compile_options(CommonLibSSE PRIVATE /MP)
endif()

if(NOT COMMAND add_commonlibsse_plugin)
	include("${IHNG_COMMONLIB_DIR}/cmake/CommonLibSSE.cmake")
endif()

find_package(spdlog CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIRS "SimpleIni.h" REQUIRED)

set(IHNG_PLUGIN_SOURCES
	PCH.h
	main.cpp
	Log.h
	Log.cpp
	Settings.h
	Settings.cpp
	Forms.h
	Forms.cpp
	Hooks.h
	Hooks.cpp
	Events.h
	Events.cpp
	Harvestable.h
	Harvestable.cpp
	Wisp.h
	Wisp.cpp
	Manager.h
	Manager.cpp
	Serialization.h
	Serialization.cpp
	UI/Menu.h
	UI/Menu.cpp
)
list(TRANSFORM IHNG_PLUGIN_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# Generates SKSEPluginInfo exporting both SKSEPlugin_Version (AE) and SKSEPlugin_Query (SE/VR).
add_commonlibsse_plugin(iHarvestNG
	NAME "iHarvestNG"
	AUTHOR "iHarvest NG contributors"
	VERSION "${PROJECT_VERSION}"
	USE_ADDRESS_LIBRARY
	SOURCES ${IHNG_PLUGIN_SOURCES}
)

target_compile_features(iHarvestNG PRIVATE cxx_std_23)
target_include_directories(iHarvestNG
	PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_include_directories(iHarvestNG SYSTEM
	PRIVATE
		"${PROJECT_SOURCE_DIR}/extern/SKSEMenuFramework"
		"${SIMPLEINI_INCLUDE_DIRS}"
)
target_link_libraries(iHarvestNG PRIVATE spdlog::spdlog ihng::core)
target_precompile_headers(iHarvestNG PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/PCH.h")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Version.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/iHarvestNG/Version.h"
	@ONLY
)

if(MSVC)
	target_compile_options(iHarvestNG PRIVATE
		/W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor /external:W0 /MP
		/Zi
		"$<$<NOT:$<CONFIG:Debug>>:/Zc:inline;/Gy;/Gw>"
	)
	target_link_options(iHarvestNG PRIVATE
		/DEBUG:FULL
		"$<$<NOT:$<CONFIG:Debug>>:/OPT:REF;/OPT:ICF;/INCREMENTAL:NO>"
	)
	target_compile_definitions(iHarvestNG PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${IHNG_PLUGIN_SOURCES})

# ---- Deploy into a mod folder (optional) -------------------------------------------------------
# Set IHNG_DEPLOY_DIR (cache variable or environment variable) to e.g. your MO2 "mods/iHarvest NG"
# folder to have every build copied there. Empty = no deploy.
set(IHNG_DEPLOY_DIR "$ENV{IHNG_DEPLOY_DIR}"
	CACHE PATH "Mod root to deploy into (the folder that maps onto Skyrim's Data/); empty disables deploy")
if(IHNG_DEPLOY_DIR)
	set(_ihng_deploy_default ON)
else()
	set(_ihng_deploy_default OFF)
endif()
option(IHNG_DEPLOY "Copy the DLL/PDB and dist/Data into IHNG_DEPLOY_DIR after build" ${_ihng_deploy_default})

set(_ihng_deploy_cmd
	"${CMAKE_COMMAND}"
	"-DDLL=$<TARGET_FILE:iHarvestNG>"
	"-DPDB=$<TARGET_PDB_FILE:iHarvestNG>"
	"-DDIST_DIR=${PROJECT_SOURCE_DIR}/dist/Data"
	"-DDEPLOY_DIR=${IHNG_DEPLOY_DIR}"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Deploy.cmake"
)

if(IHNG_DEPLOY)
	add_custom_command(TARGET iHarvestNG POST_BUILD
		COMMAND ${_ihng_deploy_cmd}
		COMMENT "Deploying iHarvestNG to ${IHNG_DEPLOY_DIR}"
		VERBATIM
	)
endif()

add_custom_target(iHarvestNG_deploy
	COMMAND ${_ihng_deploy_cmd}
	DEPENDS iHarvestNG
	COMMENT "Deploying iHarvestNG to ${IHNG_DEPLOY_DIR}"
	VERBATIM
)
