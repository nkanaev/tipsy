cmake_minimum_required(VERSION 3.25..3.27)
project(tipsy)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
  tigr
  GIT_REPOSITORY https://github.com/erkkah/tigr
  GIT_TAG v3.1.0
)
FetchContent_GetProperties(tigr)

if(NOT tigr_POPULATED)
  FetchContent_Populate(tigr)
  add_library(tigr STATIC ${tigr_SOURCE_DIR}/tigr.c)
endif()

add_executable(${PROJECT_NAME} src/tipsy.c)
target_include_directories(${PROJECT_NAME} PRIVATE ${tigr_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE tigr)
