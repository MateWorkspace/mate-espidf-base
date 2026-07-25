cmake_minimum_required(VERSION 3.22)

set(PROJECT_VERSION "v1.0.0-dev.1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(mate-espidf-base)
