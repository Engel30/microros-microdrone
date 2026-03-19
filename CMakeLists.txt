# Top-level CMakeLists.txt for microros-microdrone
# Equivalente del file progetto di CubeIDE

cmake_minimum_required(VERSION 3.16)

# Dice a ESP-IDF dove trovare i nostri componenti custom
set(EXTRA_COMPONENT_DIRS "components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(microros-microdrone)
