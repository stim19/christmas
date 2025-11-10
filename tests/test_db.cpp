#include <iostream>
#include <gtest/gtest.h>
#include "../db.h"

TEST(DBConstructor, Logs) {
    DBEngine db("test.db", true);
    // If constructor crashes, test fails automatically
    SUCCEED();
}

TEST(DBLoad, Sample) {
    DBEngine db("test.db");
    // TODO: ADD load test here
    EXPECT_TRUE(true);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
