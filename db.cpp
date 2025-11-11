#include "db.h"
#include <iostream>

//TODO: rewrite using PIMPL
//struct DB::Impl {
//  sqlite3* dbHandle;
//  bool debugMode;
//};

DBEngine::DBEngine(const std::string& dbPath, bool debug):debugMode(debug){
    if (sqlite3_open(dbPath.c_str(), &db)){
        log("Failed to open DB");
        std::cerr << "Couldn't connect to database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;        
    }
    log("Opened DB successfully");
}
DBEngine::~DBEngine(){
    if(db){ 
        sqlite3_close(db);
        db=nullptr;
        log("Closed DB successfully");
    }
}

void DBEngine::log(const std::string& msg) {
    if(debugMode)
        std::cout<< "[DB] " << msg << std::endl;
}

/**
 * Executes a SQL statement
 * 
 * @param sql The sql query string
 * @return true if execution succeeds, false otherwise.
 *
 */

bool DBEngine::execute(const std::string& sql, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        
        log(std::string("Failed to execute query: ") + msg + ": " + errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    log("OK: "+ msg);
    return true;
}

sqlite3* DBEngine::get() {
    return db;
}

Transaction::Transaction(sqlite3* db) : db(db), active(true) {
    sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    std::cout << "[BEGIN TRANSACTION]" << std::endl;
}
Transaction::~Transaction() {
    if(active) {
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        std::cout << "[TRANSACTION ROLLBACK]" << std::endl;
    }
}

PreparedStatement::PreparedStatement(sqlite3* db, const std::string& sql) {
    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    std::cout << "Prepared statement" << std::endl;
}

bool PreparedStatement::step() {
    std::cout << "inserted" << std::endl;
    return sqlite3_step(stmt) == SQLITE_ROW;
}

void PreparedStatement::reset() {
        sqlite3_reset(stmt);
        std::cout <<"Statement reset" <<std::endl;
    }

void PreparedStatement::finalize() {
        sqlite3_finalize(stmt);
        stmt = nullptr;
        std::cout << "Finalized" << std::endl;
    }
    
void PreparedStatement::bind(int index, const std::string& value) {
        sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

void PreparedStatement::bind(int index, int value) {
        sqlite3_bind_int(stmt, index, value);
    }

PreparedStatement::~PreparedStatement() {
    if(stmt) 
        sqlite3_finalize(stmt);
}















