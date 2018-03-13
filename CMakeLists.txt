cmake_minimum_required(VERSION 2.6.3)
find_package(PkgConfig)

PROJECT(rockchip_hdmiset_process_platform C CXX)
INCLUDE(GNUInstallDirs)

pkg_search_module(PTHREAD pthread)

ADD_DEFINITIONS(-Wmissing-declarations -Wall -Wextra -Wshadow -ffast-math)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -O0 -g3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os")

# putting those into config.h would be better

# ----------------------------------------------------------------------------
#  Start module definition
# ----------------------------------------------------------------------------
# project overall include file

# ----------------------------------------------------------------------------
#  Media Process Platform library
# ----------------------------------------------------------------------------
# Media Process Platform include
add_subdirectory(drm)

# ----------------------------------------------------------------------------
#  test / demo
# ----------------------------------------------------------------------------


# End generate the build configure file
