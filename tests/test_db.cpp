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

class DBEngineTest :
    public ::testing::Test {
        protected:
            static DBEngine* db;

            static void SetUpTestSuite() {
                db = new DBEngine(":memory:", true);
                db->execute("CREATE TABLE test (id INT, name TEXT);", "create test table");
            }
            static void TearDownTestSuite() {
                delete db;
                db=nullptr;
            }
            void SetUp() override {
                db->execute("DELETE FROM test;", "delete from test");
            }
    };

DBEngine* DBEngineTest::db = nullptr;

TEST(DBConstructor, Connection) {
    DBEngine db(":memory:", true);
    // If constructor crashes, test fails automatically
    SUCCEED();
    sqlite3* conn = db.get();
    ASSERT_NE(conn, nullptr);
    int rc = sqlite3_exec(conn, "CREATE TABLE test(id INT);", 0, nullptr, nullptr);
    ASSERT_TRUE(rc == SQLITE_OK);
    sqlite3_exec(conn, "DROP TABLE test;", 0, nullptr, nullptr);
}

/*TEST(DBLoad, Sample) {
    DBEngine db("test.db");
    // TODO: ADD load test here
    EXPECT_TRUE(true);
}
*/

TEST(DBEngineExecute, Execute) {
    DBEngine db(":memory:", true);
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
    EXPECT_FALSE(db.execute(BadSql, "Select all from test table"));
    db.execute("DROP TABLE test;", "Drop test table");
}

TEST_F(DBEngineTest, BeginShouldStartTransaction) {
    db->begin();
    ASSERT_TRUE(db->isActive());
    db->rollback();
}
TEST_F(DBEngineTest, ShouldCommitTranasction) {
    db->begin();
    db->commit();
    ASSERT_FALSE(db->isActive());
}
TEST_F(DBEngineTest, ShouldRollbackTransaction) {
    db->begin();
    db->rollback();
    ASSERT_FALSE(db->isActive());
}

/*
 * Transaction tests
 */
TEST_F(DBEngineTest, ConstructorShouldStartTransaction) {
    {
        Transaction t(db);
        ASSERT_TRUE(db->isActive());
    }
    ASSERT_FALSE(db->isActive());
}
TEST_F(DBEngineTest, ShouldAutoRollback) {
    {
        Transaction t(db);
        db->execute("INSERT INTO test VALUES(1, 'hello');", "insert into test table");
    }

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db->get(), "SELECT COUNT(*) FROM test;", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    ASSERT_EQ(count, 0);
}
TEST_F(DBEngineTest, ShouldCommitTransaction) {
    {
        Transaction t(db);
        db->execute("INSERT INTO test VALUES(2, 'hii');", "insert into test table");
        t.commit();
    }
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db->get(), "SELECT COUNT(*) FROM test;", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    ASSERT_EQ(count, 1);
}

/*
 * Prepared Statement tests
 */
TEST_F(DBEngineTest, PreparedStatementBasicFunctionality) {
    PreparedStatement insert(db, "INSERT INTO test VALUES(?, ?);");
    REQUIRE(insert.bind(1, 1));
    REQUIRE(insert.bind(2, "bob"));
    REQUIRE(insert.step()==SQLITE_DONE); //TODO: step should return OK, ROW, BUSY
    REQUIRE_NOTHROW(insert.reset());
}
/*
TEST_F(DBEngineTest, PreparedStatementShouldThrowOnViolations) {
    PreparedStatement
}
*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
