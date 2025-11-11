
option(BUILD_TESTS "Build test executable" ON)

# Fetch Google Test
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

#--- Test Executable-----

add_executable(test_app test_db.cpp)
target_link_libraries(test_app PRIVATE dbengine gtest_main)

enable_testing()
#include(GoogleTest)

#gtest_discover_tests(test_app)

#=========================================#
#		Tests			  #
#=========================================#

add_test(NAME DBExec COMMAND test_app)








