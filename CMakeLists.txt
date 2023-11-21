cmake_minimum_required(VERSION 3.26)
project(zulc)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 20)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_subdirectory(./srcs)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(zulc ${llvm_libs})