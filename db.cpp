#include "db.h"
#include "logger.h"
#include <iostream>

//TODO: rewrite using PIMPL
//struct DB::Impl {
//  sqlite3* dbHandle;
//  bool debugMode;
//};



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
bool DBEngine::execute(const std::string& sql, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::error(std::string("[DB]: Failed to execute query: ") + msg + ": " + errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    Logger::info("OK: "+ msg);
    return true;
}


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

void DBEngine::rollback() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!active) return; // nothing to rollback
    char* errMsg = nullptr;
    if(sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "Rollback failed" ;
        Logger::error("[TRANSACTION]: "+msg);
    }
    active = false; 
    Logger::info("[TRANSACTION]: Rollback success");
}

/**
 * Implement query method
 * @params sql, limit, offset
 * @returns a struct:
 *      vector<string> col // column names;
 *      vector<vector<string, variant<int, string, null>>>;
 * prepare stmt*
 * count columns: col_count(stmt)
 * fetch col names :row_name(stmt) -> store in columns[]
 * step(stmt) == row
 * for i : col_count
 * get_type(stmt, i)
 * switch type:
 * case 1:
 *  row push int
 * case 2:
 *  row push = reinterpert const char* column_text
 * case 3:
 *  row push null
 * default:
 *  invalid
 * where row is variant <int, string, null>
 *
 * app layer:
 * struct user;
 * users=query(sql)
 * for u : users
 * vector users
 * user({result[i][0], result[i][1], ...)
 * or for column[i]
 * value = row [i]
 */

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


/*
 *
 */

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















