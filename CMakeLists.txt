cmake_minimum_required(VERSION 3.14)
project(ros_param_sync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 声明 Header-only 接口库 (支持 FetchContent 直接接入与 target_link_libraries)
add_library(ros_param_sync INTERFACE)
add_library(ros_param_sync::ros_param_sync ALIAS ros_param_sync)

target_include_directories(ros_param_sync INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# 2. 查找依赖项 (roscpp, yaml-cpp)
find_package(yaml-cpp REQUIRED)

if(DEFINED CATKIN_DEVEL_PREFIX OR DEFINED catkin_DIR OR COMMAND catkin_package)
  find_package(catkin REQUIRED COMPONENTS roscpp)
  catkin_package(
    INCLUDE_DIRS include
    CATKIN_DEPENDS roscpp
  )
  target_link_libraries(ros_param_sync INTERFACE
    ${catkin_LIBRARIES}
    yaml-cpp
  )
  target_include_directories(ros_param_sync INTERFACE
    ${catkin_INCLUDE_DIRS}
  )
else()
  find_package(roscpp QUIET)
  target_link_libraries(ros_param_sync INTERFACE yaml-cpp)
endif()
