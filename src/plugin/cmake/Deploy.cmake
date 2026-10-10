# Post-build deploy, run as: cmake -DDLL=... -DPDB=... -DDIST_DIR=... -DDEPLOY_DIR=... -P Deploy.cmake
#
# Layout produced under DEPLOY_DIR (a mod folder that maps onto Skyrim's Data/):
#   SKSE/Plugins/IHarvestNG.dll, .pdb
#   everything in dist/Data (plugin, meshes, textures, sounds, default INI) - except that an existing
#   SKSE/Plugins/IHarvestNG.ini is never overwritten (user settings).

foreach(_var DLL DEPLOY_DIR DIST_DIR)
	if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
		message(FATAL_ERROR "Deploy.cmake: ${_var} not set")
	endif()
endforeach()

set(_plugins "${DEPLOY_DIR}/SKSE/Plugins")
file(MAKE_DIRECTORY "${_plugins}")

set(_ini "${_plugins}/IHarvestNG.ini")
set(_keep_ini FALSE)
if(EXISTS "${_ini}")
	file(READ "${_ini}" _user_ini)
	set(_keep_ini TRUE)
endif()

file(GLOB _dist_entries "${DIST_DIR}/*")
if(_dist_entries)
	file(COPY ${_dist_entries} DESTINATION "${DEPLOY_DIR}")
endif()

if(_keep_ini)
	file(WRITE "${_ini}" "${_user_ini}")
	message(STATUS "Kept existing ${_ini}")
endif()

file(COPY "${DLL}" DESTINATION "${_plugins}")
if(DEFINED PDB AND EXISTS "${PDB}")
	file(COPY "${PDB}" DESTINATION "${_plugins}")
endif()
message(STATUS "Deployed to ${DEPLOY_DIR}")
