# Minimum required version for moden C++ standards and features
cmake_minimum_required(VERSION 3.10)

project(GiftPlanner)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")

find_package(OpenGL REQUIRED)
include_directories( ${OPENGL_INCLUDE_DIRS} )

find_package(glfw3 CONFIG REQUIRED)

# Add ImGui as a library
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_demo.cpp
)

# Add ImGui backends
add_library(imgui_backends STATIC
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

# Include directories
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)
target_include_directories(imgui_backends PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/glfw/include
)

target_link_libraries(imgui_backends PUBLIC
    imgui
    glfw
    OpenGL::GL
)


include(FetchContent)
option(BUILD_MAIN  "Build main app executable" ON)


# Create static libraries
add_library(dbengine STATIC
   db.cpp
   sqlite3/sqlite3.c
)
target_include_directories(dbengine PUBLIC
   ${CMAKE_CURRENT_DIR}/sqlite3
)

set(APP_SOURCES
    app.cpp
)

set(UI_SOURCES
   ui/UIManager.cpp
   
)

target_include_directories(dbengine
PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#--- Main Executable-----

if(BUILD_MAIN)
    add_executable(GiftTracker ${APP_SOURCES} ${UI_SOURCES} main.cpp)
    target_include_directories(GiftTracker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    )
    target_link_libraries(GiftTracker PRIVATE dbengine
    imgui
    imgui_backends
    glfw
    OpenGL::GL
    )
endif()

#add_subdirectory(tests)



