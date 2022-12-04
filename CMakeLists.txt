project(fleet)

cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(
  "-Wall -g -O0"
)

# 编译静态库
aux_source_directory(src SRC_LIST)
add_library(
  ${PROJECT_NAME}_static STATIC ${SRC_LIST}
)

include_directories(
  ${PROJECT_SOURCE_DIR}/include
)

add_subdirectory(
  ${PROJECT_SOURCE_DIR}/tests
)