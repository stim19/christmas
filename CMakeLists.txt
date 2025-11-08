# Minimum required version for moden C++ standards and features
cmake_minimum_required(VERSION 3.10)

project(GiftPlanner)

# Set the C++ Standard (use C++17 for features like std::optional/std::string_view)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the source files for our core application

set(SOURCES
    main.cpp
    db.cpp
    sqlite3.c
	# Add other source files here later
)

add_executable(planner_app
	${SOURCES}
)

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

target_include_directories(planner_app PRIVATE $ {CMAKE_CURRENT_SOURCE_DIR})

# Link any system libraries here (e.g., standard C++ math, etc)
# target_link_libraries(planner_app PRIVATE stdc++ -lsqlite3)
# cmake -D CMAKE_C_COMPILER=C:\msys64\ucrt64\bin\gcc.exe -D CMAKE_CXX_COMPILER=C:\msys64\ucrt64\bin\g++.exe -G "MinGW Makefiles" ..
