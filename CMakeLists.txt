cmake_minimum_required(VERSION 3.15)
project(vdds LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VDDS_MAJOR_VERSION 1)
set(VDDS_MINOR_VERSION 1)
set(VDDS_PATCH_VERSION 0)
set(VDDS_VERSION "${VDDS_MAJOR_VERSION}.${VDDS_MINOR_VERSION}.${VDDS_PATCH_VERSION}")

# Set default pref to /usr (unless user provided an override)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "..." FORCE)
endif()

option(WITH_TESTS "enable vDDS tests" ON)

find_package(Boost 1.58.0 REQUIRED)
find_package(HOGL REQUIRED)

enable_testing()
add_subdirectory(src)

if (WITH_TESTS)
	add_subdirectory(tests)
endif()

configure_file(vdds-config.cmake.in
	"${PROJECT_BINARY_DIR}/vdds-config.cmake" @ONLY)
configure_file(vdds-config-version.cmake.in
	"${PROJECT_BINARY_DIR}/vdds-config-version.cmake" @ONLY)

install(FILES
	"${PROJECT_BINARY_DIR}/vdds-config.cmake"
	"${PROJECT_BINARY_DIR}/vdds-config-version.cmake"
	DESTINATION lib/cmake/vdds COMPONENT dev)

# DEB packaging
set(CPACK_PACKAGE_VENDOR  "Max Krasnyansky")
set(CPACK_PACKAGE_CONTACT "quic_maxk@quicinc.com")
set(CPACK_PACKAGE_VERSION_MAJOR "${VDDS_MAJOR_VERSION}")
set(CPACK_PACKAGE_VERSION_MINOR "${VDDS_MINOR_VERSION}")
set(CPACK_PACKAGE_VERSION_PATCH "${VDDS_PATCH_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${VDDS_MAJOR_VERSION}.${VDDS_MINOR_VERSION}.${VDDS_PATCH_VERSION}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SECTION "util")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

set(CPACK_DEBIAN_DEV_PACKAGE_NAME "vdds-dev")
set(CPACK_DEBIAN_DEV_PACKAGE_DESCRIPTION "vDDS library and headers")
set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "libboost-dev(>= 1.65.1) libhogl-dev(>= 3.0)")

include(CPack)
