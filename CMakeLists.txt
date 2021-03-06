cmake_minimum_required(VERSION 3.0.0)
project (inclusive VERSION "0.0.1")

if(MSVC)
  set(CMAKE_CXX_STANDARD 14)
  # avoid the boost warning, it will disable boost from using IOCP and newer APIs.
  add_definitions(-D_WIN32_WINNT=0x0601)
else()
  add_compile_options(-std=c++14)
endif()

if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

# Almost-Static build on ubuntu (ease deployment)
if (LINUX) 
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    SET(BUILD_SHARED_LIBRARIES OFF)
    link_libraries("-static-libgcc -static-libstdc++")
  endif()
endif() 

# Threads
find_package(Threads REQUIRED)


# Boost
add_definitions(-DBOOST_ASIO_NO_DEPRECATED)
find_package(Boost 1.73.0 COMPONENTS system filesystem process)
include_directories(AFTER ${Boost_INCLUDE_DIRS})


# Shippedwith
add_subdirectory(deps/mstch)
include_directories(${mstch_INCLUDE_DIR})
include_directories(AFTER ${CMAKE_CURRENT_LIST_DIR}/deps/nlohmann-json/src)
include_directories(BEFORE ${CMAKE_CURRENT_LIST_DIR}/src)
add_executable(inglued src/inglued.cpp)
target_link_libraries(inglued ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} mstch)
