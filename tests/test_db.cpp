#include <iostream>
#include <gtest/gtest.h>
#include <sqlite3.h>
#include "../db.hpp"
#include "../logger.hpp"
#include <sstream>

using namespace Engine;

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
                db->execute("CREATE TABLE test (id INT, name TEXT NOT NULL);", "create test table");
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

//TODO: Create test suite for cache

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


TEST(DBEngineExecute, Execute) {
    DBEngine db(":memory:", true);
    std::string GoodSql = R"(
        CREATE TABLE IF NOT EXISTS test (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL,
            Relationship TEXT
            );
    )";
    ASSERT_EQ(ENGINE_OK, db.execute(GoodSql, "Create TEST table"));
    
    std::string BadSql = R"(
        SELECT * FROM test where 
    )";
    ASSERT_EQ(ENGINE_ERROR, db.execute(BadSql, "Select all from test table"));
    db.execute("DROP TABLE test;", "Drop test table");
}

TEST_F(DBEngineTest, Sqlite3PreparedStatementTest) {
    std::string query = "INSERT INTO test (id, Name) VALUES(?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = db->prepare(query, stmt);
    ASSERT_EQ(rc, ENGINE_OK);
    sqlite3_finalize(stmt);
    stmt = nullptr;
    query = "INSRT INTO test VALUES(?, ?, ?);";
    rc = db->prepare(query, stmt);
    ASSERT_EQ(rc, ENGINE_SYNTAX_ERROR);
    sqlite3_finalize(stmt);
    stmt = nullptr;
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
 * Cache Tests
 *
 */
TEST_F(DBEngineTest, InitializeCacheWithValidCapacity){
    size_t cap = 5;
    ASSERT_NO_THROW(LRUCache cache = LRUCache(cap));
    cap = 5000;
    ASSERT_THROW(LRUCache cache = LRUCache(cap), CacheLimitError);
}
TEST_F(DBEngineTest, ShouldAddToCache) {
    int rc;
    size_t cap = 5;
    LRUCache cache = LRUCache(cap);
    sqlite3_stmt* stmt1 = nullptr;
    sqlite3_stmt* stmt2 = nullptr;
    sqlite3_stmt* stmt3 = nullptr;
    std::string query1 = "SELECT id FROM test;";
    std::string query2 = "SELECT name FROM test;";
    std::string query3 = "INSERT INTO test (Name) VALUES(?);";
    db->prepare(query1, stmt1);
    db->prepare(query2, stmt2);
    db->prepare(query3, stmt3);
    rc = cache.put(query1, stmt1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query2, stmt2);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query3, stmt3);
    ASSERT_EQ(rc, CACHE_OK);
    sqlite3_finalize(stmt1);
    sqlite3_finalize(stmt2);
    sqlite3_finalize(stmt3);
}
TEST_F(DBEngineTest, TestDuplicateEntry) {
    int rc;
    size_t cap = 5;
    LRUCache cache = LRUCache(cap);
    sqlite3_stmt* stmt1 = nullptr;
    sqlite3_stmt* stmt2 = nullptr;
    std::string query1 = "SELECT id FROM test;";
    db->prepare(query1, stmt1);
    db->prepare(query1, stmt2);
    cache.put(query1, stmt1);
    rc = cache.put(query1, stmt2);
    ASSERT_EQ(rc, CACHE_DUPLICATE);
    sqlite3_finalize(stmt1);
    sqlite3_finalize(stmt2);

}

TEST_F(DBEngineTest, ReleaseShouldUpdateStatus) {
    int rc;
    size_t cap = 5;
    LRUCache cache = LRUCache(cap);
    sqlite3_stmt* stmt1 = nullptr; 
    sqlite3_stmt* cache1 = nullptr;
    sqlite3_stmt* cache2 = nullptr;
    std::string query1 = "INSERT INTO test (id, name) VALUES(?, ?);";

    std::string query2 ="SELECT * FROM test;";
    sqlite3_stmt* invalidKey = nullptr;
    db->prepare(query1, stmt1);
    db->prepare(query2, invalidKey);
    cache.put(query1, stmt1);
    stmt1 = nullptr;
    rc = cache.get(query1, cache1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.get(query1, cache2);
    ASSERT_EQ(rc, CACHE_BUSY);
    
    
    rc = cache.release(cache1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(invalidKey);
    ASSERT_EQ(rc, CACHE_NOT_FOUND);
    

    rc = cache.get(query1, cache2);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache2);
    ASSERT_EQ(rc, CACHE_OK);
    cache2 = nullptr;
    rc = cache.clearAll();
    ASSERT_EQ(rc, CACHE_OK);
}

TEST_F(DBEngineTest, ShouldGetFromCache) {
    int rc;
    size_t cap = 5;
    LRUCache cache = LRUCache(cap);
    sqlite3_stmt* stmt1 = nullptr; 
    sqlite3_stmt* stmt2 = nullptr; 
    sqlite3_stmt* stmt3 = nullptr;
    std::string query1 = "INSERT INTO test (id, name) VALUES(?, ?);";
    std::string query2 = "SELECT name from test;";
    std::string query3 = "SELECT * from test;";
    db->prepare(query1, stmt1);
    db->prepare(query2, stmt2);
    db->prepare(query3, stmt3);
    rc=cache.put(query1, stmt1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query2, stmt2);
    ASSERT_EQ(rc, CACHE_OK);   
    rc = cache.put(query3, stmt3);
    ASSERT_EQ(rc, CACHE_OK);
    std::string newQuery = "SELECT id FROM test;";
    sqlite3_stmt* cache1 = nullptr;
    sqlite3_stmt* cache2 = nullptr;
    sqlite3_stmt* cache3 = nullptr;
    sqlite3_stmt* cache4 = nullptr;
    rc = cache.get(query1, cache1);
    ASSERT_EQ(rc, CACHE_OK);
    // test when a cache is retrieved, status is set to busy and cannot be used
    rc = cache.get(query1, cache2);
    ASSERT_EQ(rc, CACHE_BUSY);
    ASSERT_EQ(cache2, nullptr);
    rc = cache.get(newQuery, cache2);
    ASSERT_EQ(rc, CACHE_NOT_FOUND);
    ASSERT_EQ(cache2, nullptr);
    rc = cache.get(query2, cache2);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.get(query3, cache3);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache3);
    ASSERT_EQ(rc, CACHE_OK);
    cache3 = nullptr;
    rc = cache.get(query3, cache4);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache2);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache4);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache1);
    ASSERT_EQ(rc, CACHE_OK);
    cache1 = cache2 = cache3 = cache4 = nullptr;
    sqlite3_finalize(stmt1);
    sqlite3_finalize(stmt2);
    sqlite3_finalize(stmt3);
}

TEST_F(DBEngineTest, ShouldResetAndClearAllBindings){
    int rc;
    size_t cap = 5;
    LRUCache cache = LRUCache(cap);
    sqlite3_stmt* stmt1 = nullptr; 
    sqlite3_stmt* cache1 = nullptr;
    sqlite3_stmt* cache2 = nullptr;
    std::string query1 = "INSERT INTO test (id, name) VALUES(?, ?);";
    db->prepare(query1, stmt1);
    rc = cache.put(query1, stmt1);
    rc = cache.get(query1, cache1);
    ASSERT_EQ(rc, CACHE_OK);
    sqlite3_bind_int(cache1, 1, 1);
    sqlite3_bind_text(cache1, 2, "bob", -1, SQLITE_TRANSIENT); 
    rc=sqlite3_step(cache1);
    ASSERT_EQ(rc, SQLITE_DONE);
    rc = cache.release(cache1);
    ASSERT_EQ(rc, CACHE_OK);
    cache1 = nullptr;
    rc = cache.get(query1, cache2);
    ASSERT_EQ(rc, CACHE_OK);
    rc = sqlite3_step(cache2);
    // should return code 19 when inserting NULL (empty binds) into field Name with NOT NULL constraint
    ASSERT_EQ(rc, SQLITE_CONSTRAINT);
    //reset after stepping, bind values
    sqlite3_reset(cache2);
    sqlite3_bind_int(cache2, 1, 2);
    sqlite3_bind_text(cache2, 2, "bob2", -1, SQLITE_TRANSIENT); 
    rc = sqlite3_step(cache2);
    ASSERT_EQ(rc, SQLITE_DONE);
    // manual reset with empty binds should return code 19 SQLITE_CONSTRAINT
    sqlite3_reset(cache2);
    sqlite3_clear_bindings(cache2);
    rc = sqlite3_step(cache2);
    ASSERT_EQ(rc, SQLITE_CONSTRAINT);
    sqlite3_reset(cache2);
    rc = cache.release(cache2);
    ASSERT_EQ(rc, CACHE_OK);
    cache2 = nullptr;
    sqlite3_finalize(stmt1);
}

TEST_F(DBEngineTest, TestBehavoirWhenAtMaxCapacity) {
    int rc;
    size_t cap = 3;
    LRUCache cache = LRUCache(cap);
    sqlite3_stmt* stmt1 = nullptr; 
    sqlite3_stmt* stmt2 = nullptr;
    sqlite3_stmt* stmt3 = nullptr;
    sqlite3_stmt* stmt4 = nullptr;
    sqlite3_stmt* cache1 = nullptr;
    sqlite3_stmt* cache2 = nullptr;
    sqlite3_stmt* cache3 = nullptr;
    std::string query1 = "INSERT INTO test (id, name) VALUES(?, ?);";
    std::string query2 = "SELECT * FROM test;";
    std::string query3 = "SELECT id FROM test;";
    std::string query4 = "SELECT name FROM test;";
    
    db->prepare(query1, stmt1);
    db->prepare(query2, stmt2);
    db->prepare(query3, stmt3);
    db->prepare(query4, stmt4);

    cache.put(query1, stmt1);
    cache.put(query2, stmt2);
    cache.put(query3, stmt3);
    
    // test when cache is at max capacity and all entries are free
    // should remove query1 (LRU)
    rc = cache.put(query4, stmt4);
    ASSERT_EQ(rc, CACHE_OK);
    // confirm query1 is evicted
    rc = cache.get(query1, cache1);
    ASSERT_EQ(rc, CACHE_NOT_FOUND);
    ASSERT_EQ(cache1, nullptr);
    rc = cache.get(query4, cache1);
    ASSERT_EQ(rc, CACHE_OK);

    // since stmt1 is now finalized since it was evicted, prepare stmt1 again
    stmt1 = nullptr;
    db->prepare(query1, stmt1);
    // test when some entries are busy
    // query4, query3 are in use, should remove query2
    rc = cache.get(query3, cache2);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query1, stmt1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.get(query1, cache3);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache3);
    ASSERT_EQ(rc, CACHE_OK);
    cache3 = nullptr;
    rc = cache.get(query2, cache3);
    ASSERT_EQ(rc, CACHE_NOT_FOUND);
    ASSERT_EQ(cache3, nullptr);
    
    // since stmt2 is now finalized since it was evicted, prepare stmt2 again
    stmt2 = nullptr;
    db->prepare(query2, stmt2);
    
    // test when all entries are busy and cache is at max capacity
    rc = cache.get(query1, cache3);
    ASSERT_EQ(rc, CACHE_OK);
    //try adding query2 again
    rc = cache.put(query2, stmt2);
    ASSERT_EQ(rc, CACHE_FULL);
    rc = cache.release(cache3);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.release(cache2);
    ASSERT_EQ(rc, CACHE_OK);
    cache1 = nullptr;
    cache2 = nullptr;
    cache3 = nullptr;
    sqlite3_finalize(stmt1);
    sqlite3_finalize(stmt2);
    sqlite3_finalize(stmt3);
    sqlite3_finalize(stmt4);
    
}

TEST_F(DBEngineTest, ShouldFinalizeAndClearAll) {
    int rc;
    size_t cap = 5;
    LRUCache cache = LRUCache(cap);
    sqlite3_stmt* stmt1 = nullptr; 
    sqlite3_stmt* stmt2 = nullptr;
    sqlite3_stmt* stmt3 = nullptr;
    sqlite3_stmt* stmt4 = nullptr;
    sqlite3_stmt* stmt5 = nullptr;
    sqlite3_stmt* cache1 = nullptr;
    std::string query1 = "INSERT INTO test (id, name) VALUES(?, ?);";
    std::string query2 = "INSERT INTO test (name) VALUES(?);";
    std::string query3 = "SELECT * FROM test;";
    std::string query4 = "SELECT name FROM test WHERE id = ?;";
    std::string query5 = "SELECT * FROM test WHERE name = ?;";

    db->prepare(query1, stmt1);
    db->prepare(query2, stmt2);
    db->prepare(query3, stmt3);
    db->prepare(query4, stmt4);
    db->prepare(query5, stmt5);

    rc = cache.put(query1, stmt1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query2, stmt2);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query3, stmt3);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query4, stmt4);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.put(query5, stmt5);
    ASSERT_EQ(rc, CACHE_OK);

    rc = cache.get(query5, cache1);
    ASSERT_EQ(rc, CACHE_OK);
    
    rc = cache.clearAll();
    ASSERT_EQ(rc, CACHE_BUSY);
    
    rc = cache.release(cache1);
    ASSERT_EQ(rc, CACHE_OK);

    rc = cache.clearAll();
    ASSERT_EQ(rc, CACHE_OK);

    //should allow insert of query1
    stmt1 = nullptr;
    db->prepare(query1, stmt1);
    rc = cache.put(query1, stmt1);
    ASSERT_EQ(rc, CACHE_OK);
    rc = cache.clearAll();
    ASSERT_EQ(rc, CACHE_OK);
}

/*
 * Prepared Statement tests
 *
TEST_F(DBEngineTest, PreparedStatementBasicFunctionality) {
    PreparedStatement insert(db->get(), "INSERT INTO test VALUES(?, ?);");
    ASSERT_NO_THROW(insert.bind(1, 1));
    ASSERT_NO_THROW(insert.bind(2, "bob"));
   
    ASSERT_EQ(insert.step(), SQLITE_DONE); //TODO: step should return OK, ROW, BUSY
    ASSERT_NO_THROW(insert.reset());
}
TEST_F(DBEngineTest, PreparedStatementLifecycleTest) {
    PreparedStatement insert(db->get(), "INSERT INTO test VALUES(?, ?);");
    ASSERT_TRUE(insert.isPrepared());
    insert.finalize();
    ASSERT_EQ(insert.get(), nullptr);
    ASSERT_TRUE(insert.isFinalized());
    ASSERT_NO_THROW(insert.finalize()); 
}

// Constraint violation tests
TEST_F(DBEngineTest, StatementShouldThrowOnConstraintViolations) {
    PreparedStatement insert(db->get(), "INSERT INTO test(id) VALUES(?);");
    insert.bind(1, 1);
    ASSERT_THROW(insert.step(), ConstraintError);
}
//TODO: next write tests for binding parameter out of range
TEST_F(DBEngineTest, ShouldThrowWhenBindingOutOfRange) {
    PreparedStatement insert(db->get(), "INSERT INTO test VALUES(?, ?);");
    insert.bind(1,1);
    insert.bind(2,"bob");
    ASSERT_THROW(insert.bind(3, "99999999"), BindRangeException); 
}
// Should throw when SQL syntax error
TEST_F(DBEngineTest, ShouldThrowSQLSyntaxError) {
    ASSERT_THROW(PreparedStatement insert(db->get(), "INSERT INTO test ID VALUES(?, ?);"), SyntaxError);
    try {
            PreparedStatement insert(db->get(), "INSERT INTO test ID VALUES(?, ?);");
    }
    catch(SyntaxError& e) {
        std::cout << e.what() << std::endl;
    }
}
//Should throw when binding without reset
TEST_F(DBEngineTest, ShouldThrowWhenBindWithoutReset) {
    PreparedStatement insert(db->get(), "INSERT INTO test VALUES(?, ?);");
    insert.bind(1, 1);
    insert.bind(2, "bob");
    insert.step();
    ASSERT_THROW(insert.bind(1, 2), StatementStateError);
    insert.reset();
    ASSERT_NO_THROW(insert.bind(1, 2));
    ASSERT_NO_THROW(insert.bind(2, "bob2"));
    ASSERT_NO_THROW(insert.step());
}
*/
/* Statement cache tests
 *
 */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
