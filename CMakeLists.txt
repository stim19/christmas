# Minimum required version for moden C++ standards and features
cmake_minimum_required(VERSION 3.10)

project(GiftPlanner)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create static libraries 
add_library(dbengine STATIC
   db.cpp
   sqlite3.c
)
target_include_directories(dbengine 
PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_app tests/test_db.cpp)
enable_testing()
target_link_libraries(test_app PRIVATE dbengine)
add_test(NAME DBTest COMMAND test_db)

#option(BUILD_APP "Build main application" ON)
#option(BUILD_TESTS "Build tests" ON)

# Optional main app executable
#if(BUILD_APP)
#    add_executable(planner_app main.cpp)
#    target_link_libraries(planner_app PRIVATE dbengine)
#endif()



#=========================================#
#		Tests			  #
#=========================================#

# Optional test executable
#if(BUILD_TESTS)
#    add_executable(test_db tests/test_db.cpp)
#    target_link_libraries(test_db PRIVATE dbengine)
#    enable_testing()
#    # Register each test function with CTest
#    # Always name your tests explicitly
#    add_test(NAME DBtest COMMAND test_db)
#endif()




# Optional: use system-wide SQLite library and link it
# find_package(SQLIITE3 REQUIRED)
# target_link_libraries(planner_app PRIVATE SQLite::SQLite3)
# Use the target_link_libraries below only if OS-specific linking is required

#------WINDOWS COSOLE FIX -------------- 
# Force the linker to use standard main as entry point needed on (windows console setups when using MinGW)
#if(WIN32)
#	target_link_options(planner_app PRIVATE "-Wl,--subsystem,console")
#endif()
#---------------------------------------

#target_include_directories(planner_app PRIVATE $ {CMAKE_CURRENT_SOURCE_DIR})

# Link any system libraries here (e.g., standard C++ math, etc)
# target_link_libraries(planner_app PRIVATE stdc++ -lsqlite3)
# cmake -D CMAKE_C_COMPILER=C:\msys64\ucrt64\bin\gcc.exe -D CMAKE_CXX_COMPILER=C:\msys64\ucrt64\bin\g++.exe -G "MinGW Makefiles" ..





