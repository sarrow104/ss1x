cmake_minimum_required(VERSION 2.8) 	# must surply this !
add_definitions(-std=c++11)
add_definitions(-W -fexceptions -Wunused-variable -Wfatal-errors)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	# http://stackoverflow.com/questions/22140520/how-to-enable-assert-in-cmake-release-mode
	# add_definitions(-DNODEBUG) # assert()
	add_definitions(-O2 -s)
	set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> <LINK_FLAGS> rus <TARGET> <OBJECTS>")
	set(target_name "ss1x")
else()
	add_definitions(-DDEBUG -D_DEBUG_ -O0 -g -ggdb)
	set(target_name "ss1xD")
endif()

set(CMAKE_CXX_ARCHIVE_CREATE ${CMAKE_C_ARCHIVE_CREATE})

message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
message(STATUS "target_name=${target_name}")

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/../../bin/")
set(LIBRARY_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/../../lib/")

message(STATUS ${LIBRARY_OUTPUT_NAME})

project(${target_name})                 # define cmake project name

#add_definitions(-std=c++11)    		# add compiler parameter
file(GLOB_RECURSE SOURCE "./*.cpp")  # glob all source file
# NOTE cmake might missing some cpp source file, if I just only use this:
# file(GLOB SOURCE "**/*.cpp")
#. sad
# for example, my sss::CMLParser.cpp, sss::Terminal.cpp ... why 
#file(GLOB SOURCE "./*.cpp"    		# do the same thing, but ...
#"bit_operation/*.cpp"
#"container/*.cpp"
#...
#"xml4/*.cpp")

add_library(${target_name} STATIC ${SOURCE})       # what's our taget and it's type

# CMake的ar/ranlib动作，有一个问题，那就是，参数AR_FLAGS没法方便地起作用；
# CMake系统，默认使用cr参数，如果你额外定义了AR_FLAGS，那么这个值只会附加在后面，而不是替换
# http://stackoverflow.com/questions/5659225/how-to-set-the-options-for-cmake-ar
# https://cmake.org/pipermail/cmake/2010-July/038286.html

#message(${CMAKE_CXX_ARCHIVE_CREATE})
#message(${CMAKE_C_ARCHIVE_CREATE})
