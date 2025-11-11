# Minimum required version for moden C++ standards and features
cmake_minimum_required(VERSION 3.10)

project(GiftPlanner)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
option(BUILD_MAIN  "Build main app executable" ON)

# Create static libraries 
add_library(dbengine STATIC
   db.cpp
   sqlite3.c
)
target_include_directories(dbengine 
PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#--- Main Executable-----

if(BUILD_MAIN)
    add_executable(main_app main.cpp)
    target_link_libraries(main_app PRIVATE dbengine)
endif()

#add_subdirectory(tests)



