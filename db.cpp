#include "db.hpp"
#include "logger.hpp"
#include <iostream>

//TODO: rewrite using PIMPL
//struct DB::Impl {
//  sqlite3* dbHandle;
//  bool debugMode;
//};

// This engine does not support multi-threaded operations yet
DBEngine::DBEngine(const std::string& dbPath, bool debug) {
    
    // enable logging
    Logger::enabled = debug;
    
    if (sqlite3_open(dbPath.c_str(), &db)){
        Logger::error("[DB]: Failed to open DB");
        //std::cerr << "[DB] Couldn't connect to database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;        
        throw std::runtime_error("[DB] Couldn't connect to database");
    }
    Logger::info("[DB]: Opened DB successfully");
}
DBEngine::~DBEngine(){
    if(db){ 
        sqlite3_close(db);
        db=nullptr;
        Logger::info("[DB]: Closed DB successfully");
    }
}

/**
 * Executes a SQL statement
 * 
 * @param sql The sql query string
 * @return true if execution succeeds, false otherwise.
 *
 */
//execute
bool DBEngine::execute(const std::string& sql, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::error(std::string("[DB]: Failed to execute query: ") + msg + ": " + errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    Logger::info("[DB] OK: "+ msg);
    return true;
}
//'begin' a transaction
// the engine allows only one transaction at a time per connection
void DBEngine::begin() {
    std::lock_guard<std::mutex> lock(mtx);
    if(active)
        throw std::runtime_error("Transaction already active");
    char* errMsg = nullptr;
    if(sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error(msg);
    }
    active = true;    
    Logger::info("[TRANSACTION]: Starting transaction");
}
//commit
void DBEngine::commit() {
    std::lock_guard<std::mutex> lock(mtx);
    if(!active)
        throw std::runtime_error("No active transaction");
    char* errMsg = nullptr;
    if(sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "Commit failed" ;
        sqlite3_free(errMsg);
        active = false;
        throw std::runtime_error(msg);
    }
    active = false; 
    Logger::info("[TRANSACTION]: Commit success");
}
//rollback
void DBEngine::rollback() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!active) return; // nothing to rollback
    char* errMsg = nullptr;
    if(sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        active=false;
        std::string msg = errMsg ? errMsg : "Rollback failed" ;
        Logger::error("[TRANSACTION]: "+msg);
    }
    active = false; 
    Logger::info("[TRANSACTION]: Rollback success");
}
//returns a db handle
sqlite3* DBEngine::get() {
    return db;
}


/**
 * Initiate a transaction
 * automatically rollback when it goes out of scope
 *
 * Transaction t;   //initiates transaction
 * Execute statements;
 * t.commit()       // commit changes, automatically rollback otherwise
 * 
 */
Transaction::Transaction(DBEngine* dbEngine) : db(dbEngine) {
    db->begin();            // DBEngine handles the actual BEGIN
}
Transaction::~Transaction() {
    if(!committed) {
        db->rollback();     //DBEngine handles the actual ROLLBACK
    }
}
void Transaction::commit() {
    db->commit();           // DBEngine handles the actual COMMIT
    committed = true;       // mark as committed so destructor won't rollback
}
//TODO: Implement none, active, committing, rollingback states for Transactions


/* 
 * Class: PreparedStatement
 *
 */
PreparedStatement::PreparedStatement(sqlite3* db, const std::string& sql) {
    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    prepared=true;
    std::cout << "Prepared statement" << std::endl;
}
PreparedStatement::~PreparedStatement() {
    if(stmt) {
        finalize();
        finalized=true;
    }
}
//step a stmt
int PreparedStatement::step() {
    std::cout << "inserted" << std::endl;
    return sqlite3_step(stmt);
}
//reset a stmt
void PreparedStatement::reset() {
    sqlite3_reset(stmt);
    std::cout <<"Statement reset" <<std::endl;
}
//finalize a prepared stmt
void PreparedStatement::finalize() {
    if(sqlite3_finalize(stmt)!=SQLITE_OK)
        throw std::runtime_error("Finalize failed");
    stmt = nullptr;
    std::cout << "Finalized" << std::endl;
}
//bind text  
void PreparedStatement::bind(int index, const std::string& value) {
    if(sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT)!=SQLITE_OK)
        throw std::runtime_error("Bind failed at index " + std::to_string(index) + "(text)");
}
//bind int
void PreparedStatement::bind(int index, int value) {
    if(sqlite3_bind_int(stmt, index, value)!=SQLITE_OK)
        throw std::runtime_error("Bind failed at index " + std::to_string(index) + "(int)");
}
//bind double
void PreparedStatement::bind(int index, double value) {
    if(sqlite3_bind_double(stmt, index, value)!=SQLITE_OK)
        throw std::runtime_error("Bind failed at index " + std::to_string(index) + "(double)");
}
//bind null
void PreparedStatement::bind(int index) {
    if(sqlite3_bind_null(stmt, index)!=SQLITE_OK) 
        throw std::runtime_error("Bind failed at index " + std::to_string(index) + "(null)");
}
//bind bool
void PreparedStatement::bind(int index, bool value) {
    if(sqlite3_bind_int(stmt, index, value ? 1:0)!=SQLITE_OK)
        throw std::runtime_error("Bind failed at index " + std::to_string(index) + "(bool)");
}
// get column count
int PreparedStatement::columnCount() {
    return sqlite3_column_count(stmt);
}
//get column name at index
const char* PreparedStatement::columnName(int index) {
    return sqlite3_column_name(stmt, index);
}
//get column type at index
int PreparedStatement::columnType(int index) {
    return sqlite3_column_type(stmt, index);
}
//returns a sqlite3_stmt* handle
sqlite3_stmt* PreparedStatement::get() {
    return stmt;
}


//TODO: implement LRU cache for prepared statements, customizable max 16
/*
 * The LRU statement cache will hold mappings from sql text to sqlite3_stmt. When you prepare(), check if statement exists in cache:
 *      - if yes, it must be reset() and reused
 *      - if no, create a new one and insert into cache
 *  When capacity is exceeded, evict least recently used and finalize the stmt before eviction.
 *  Tracking can be done using a linked list or an ordered map keyed by recency.
 */

//TODO: implement returning rows with automatic type handling using row template

//TODO: implement query planner
// Rows can be streamed rather than buffered for efficiency

// Don't mix caching with user owned stmt on the same SQL string. Statements are intended to be owned by the LRU cache or a RAII wrapper
// For statement-level profiling:
//
//     flags to be implemented per statement: 
//     - "executed_in_tx"
//     - "is_cached"
//










