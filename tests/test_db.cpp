#include <iostream>
#include <gtest/gtest.h>
#include "../db.h"

TEST(DBConstructor, Logs) {
    DBEngine db("test.db", true);
    // If constructor crashes, test fails automatically
    SUCCEED();
}

/*TEST(DBLoad, Sample) {
    DBEngine db("test.db");
    // TODO: ADD load test here
    EXPECT_TRUE(true);
}
*/

TEST(DBExec, Logs) {
    DBEngine db("test.db", true);
    std::string GoodSql = R"(
        CREATE TABLE IF NOT EXISTS test (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL,
            Relationship TEXT
            );
    )";
    EXPECT_TRUE(db.execute(GoodSql, "Create TEST table"));
    
    std::string BadSql = R"(
        SELECT * FROM test where 
    )";
    EXPECT_FALSE(db.execute(BadSql, "Select all from TEST table"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
