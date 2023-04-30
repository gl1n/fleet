project(fleet)

cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(
  "-Wall -g -O0"
)

# 编译静态库
aux_source_directory(src SRC_LIST)
set(SRC_LIST
  ${SRC_LIST}
  third_party/ByteBufferCpp/ByteBuffer.cpp
)
add_library(
  ${PROJECT_NAME}_static STATIC ${SRC_LIST}
)

include_directories(
  ${PROJECT_SOURCE_DIR}/src/include
)

include_directories(
  ${PROJECT_SOURCE_DIR}/third_party
)

add_subdirectory(
  ${PROJECT_SOURCE_DIR}/tests
)