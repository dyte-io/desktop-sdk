cmake_minimum_required(VERSION 3.4...3.18)
set(MODULE_NAME dyte_sdk)
project("${MODULE_NAME}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/pybind11)

pybind11_add_module("${MODULE_NAME}" src/c++/dyte_sdk.cc)

target_include_directories(
  "${MODULE_NAME}" PRIVATE "${CMAKE_SOURCE_DIR}/third_party/libmobilecore")

target_link_directories(
  "${MODULE_NAME}" PRIVATE "${CMAKE_SOURCE_DIR}/third_party/libcwebrtc" PRIVATE
  "${CMAKE_SOURCE_DIR}/third_party/libmobilecore")

target_link_libraries("${MODULE_NAME}" PRIVATE cwebrtc mobilecore)

set_target_properties("${MODULE_NAME}" PROPERTIES LINK_FLAGS
                                                  "-Wl,-rpath,\$ORIGIN")
