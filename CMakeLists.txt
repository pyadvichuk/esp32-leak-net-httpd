cmake_minimum_required(VERSION 3.5)

# For building of static resources
option(BUILD_STATIC_CONTENT "Build the static content" OFF)

set(COMPONENTS "main; esptool_py")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(wb-net-test2)
