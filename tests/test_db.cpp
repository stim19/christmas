#include <iostream>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include "../db.h"
#include "../logger.h"
#include <sstream>

/*
 * Logger tests
 */
TEST(LoggerTest, DisabledShouldNotPrint) {
    Logger::enabled = false;

    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    Logger::info("This should not print");
    std::cout.rdbuf(old);
    EXPECT_TRUE(buffer.str().empty());
}

TEST(LoggerTest, EnabledPrintsToConsole) {
    Logger::enabled = true;

    std::ostringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    Logger::info("test message");
    std::cout.rdbuf(old);
    std::string out = buffer.str();
    EXPECT_NE(out.find("test message"), std::string::npos);
}

TEST(DBConstructor, Connection) {
    DBEngine db(":memory:", true);
    // If constructor crashes, test fails automatically
    SUCCEED();
    sqlite3* conn = db.get();
    ASSERT_NE(conn, nullptr);
    int rc = sqlite3_exec(conn, "CREATE TABLE test(id INT);", 0, nullptr, nullptr);
    ASSERT_TRUE(rc == SQLITE_OK);
}

/*TEST(DBLoad, Sample) {
    DBEngine db("test.db");
    // TODO: ADD load test here
    EXPECT_TRUE(true);
}
*/

TEST(DBEngineTest, Execute) {
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
