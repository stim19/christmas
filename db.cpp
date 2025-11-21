#include "db.hpp"
#include "logger.hpp"
#include <iostream>

using namespace Engine;

void LRUCache::removeNode(Node* n) {
    if(!n) return;
    if(n->prev)
        n->prev->next = n->next;
    else
        head = n->next;
    
    if(n->next)
        n->next->prev = n->prev;
    else
        tail = n->prev;

    n->prev = nullptr;
    n->next = nullptr;
}
void LRUCache::pushFront(Node* n) {
    n->prev = nullptr;
    n->next = head;
    if(head)
        head->prev = n;
    head=n;
    if(!tail)
        tail=n;
}
void LRUCache::moveToFront(Node* n) {
    if(n == head) return;
    removeNode(n);
    pushFront(n);
}
PreparedStatement* LRUCache::get(const std::string& key) {
    auto it = map.find(key);
    if(it == map.end())
        return nullptr;

    Node* n = it->second;
    moveToFront(n);
    return n->value;
}
void LRUCache::put(const std::string& key, PreparedStatement* value) {
    auto it = map.find(key);
    if(it != map.end()) {
        Node* n = it->second;
        n->value = value;
        moveToFront(n);
        return;
    }

    Node* n = new Node{key, value, nullptr, nullptr};
    pushFront(n);
    map[key] = n;

    if (map.size() > capacity) {
        Node* old = tail;
        removeNode(old);
        map.erase(old->key);
        delete old;
    }
}


// This engine does not support multithreading
DBEngine::DBEngine(const std::string& dbPath, bool debug, size_t cacheSize) {
    
    // enable logging
    Logger::enabled = debug;
    
    if (sqlite3_open(dbPath.c_str(), &db)){
        Logger::error("[DB]: Failed to open DB");
        //std::cerr << "[DB] Couldn't connect to database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;        
        throw ConnectionError("[DB] Couldn't connect to database", ENGINE_CONNECTION_ERROR);
    }
    Logger::info("[DB]: Opened DB successfully");
    stmtCache = new LRUCache(cacheSize);
    Logger::info("[DB]: Initialized statement cache");
}
DBEngine::~DBEngine(){
    if(db){ 
        sqlite3_close(db);
        db=nullptr;
        Logger::info("[DB]: Closed DB successfully");
    }
    if(stmtCache) {
        stmtCache = nullptr;
        Logger::info("[DB]: Cleared statement cache");
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
int DBEngine::begin() {
    std::lock_guard<std::mutex> lock(mtx);
    if(active)
        throw TransactionError("Transaction already active", ENGINE_ERROR);
    char* errMsg = nullptr;
    if(sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg;
        sqlite3_free(errMsg);
        throw TransactionError("Failed to start transaction"+msg, ENGINE_ERROR);
    }
    active = true;    
    Logger::info("[TRANSACTION]: Starting transaction");
    return ENGINE_OK;
}
//commit
int DBEngine::commit() {
    std::lock_guard<std::mutex> lock(mtx);
    if(!active)
        throw std::runtime_error("No active transaction");
    char* errMsg = nullptr;
    if(sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "Commit failed" ;
        sqlite3_free(errMsg);
        active = false;
        throw TransactionError("Failed to commit transaction "+msg, ENGINE_COMMIT_FAILURE);
    }
    active = false; 
    Logger::info("[TRANSACTION]: Commit success");
    return ENGINE_OK;
}
//rollback
int DBEngine::rollback() {
    std::lock_guard<std::mutex> lock(mtx);
    if (!active) return ENGINE_OK; // nothing to rollback
    char* errMsg = nullptr;
    if(sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        active=false;
        std::string msg = errMsg ? errMsg : "Rollback failed" ;
        Logger::error("[TRANSACTION]: "+msg);
    }
    active = false; 
    Logger::info("[TRANSACTION]: Rollback success");
    return ENGINE_OK;
}

const char* DBEngine::getLastErrorMsg() {
    return sqlite3_errmsg(db);
}

//returns a db handle
sqlite3* DBEngine::get() {
    return db;
}

void DBEngine::addToCache(const std::string& key, PreparedStatement* value) {
    stmtCache->put(key, value);
}
PreparedStatement* DBEngine::getFromCached(const std::string& key) {
    return stmtCache->get(key);
}

/**
 * Initiate a transaction
 * automatically rollback when it goes out of scope
 * 
 * Example:
 * Transaction t;   
 * Execute statements;
 * t.commit()                // commit changes, automatically rollback otherwise
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
int Transaction::getTransactionState() {
    return state;
};
void Transaction::commit() {
    db->commit();           // DBEngine handles the actual COMMIT
    committed = true;       // mark as committed so destructor won't rollback
}
//TODO: Implement none, active, committing, rollingback states for Transactions


/* 
 * Class: PreparedStatement
 *
 */
PreparedStatement::PreparedStatement(DBEngine* db, const std::string& sql):db_(db) {
    
    if(!db->getFromCached(sql)){
        Logger::info("Preparing statement");
        int rc=sqlite3_prepare_v2(db->get(), sql.c_str(), -1, &stmt, nullptr); 
        if(rc != SQLITE_OK) {
            stmt = nullptr;
            if(rc == SQLITE_ERROR)
            {
                std::string msg = db_->getLastErrorMsg();
                throw SyntaxError("SQL Error during execution: "+ msg, rc);
            }
            else {
                std::string msg = db->getLastErrorMsg();
                throw std::runtime_error("Unexpected exception occurred: "+msg);
            }
        }
        Logger::info("Adding statement to cache");
        db->addToCache(sql, this);
    }
    else {
        Logger::info("Getting statement cache");
        stmt = db->getFromCached(sql)->get();
    }

    prepared=true;
    Logger::info("Statement prepared");    
}
PreparedStatement::~PreparedStatement() {
    if(stmt) {
        finalize();
        finalized=true;
    }
    db_=nullptr;
}
//is prepared
bool PreparedStatement::isPrepared() {
    return prepared;
}
// is finalized
bool PreparedStatement::isFinalized() {
    return finalized;
}
//step a stmt
int PreparedStatement::step() {
    if (!stmt)
        // after statement is finalized, step operation is not permitted
        throw StatementStateError("Cannot call step() on a finalized or uninitialized statement.", 1);
    
    // after a step(), statement must be reset for binding or else should throw a state error
    isReset=false;         
       
    int rc=sqlite3_step(stmt);
    if(rc==SQLITE_CONSTRAINT) {
        reset(); // statement is reset after exception because calling finalize after an invalid step() throws
        throw ConstraintError("Database constraint violated: ", rc); //TODO: should add database error message?
    }
    if(rc==SQLITE_ERROR) {
        reset();
        throw SyntaxError("SQL Error during execution: ", rc);
    }
    if(rc==SQLITE_MISMATCH) {
        reset(); //TODO: fix err message
        throw std::runtime_error("Datatype mismatch on column binding ");
    }
    if(rc == SQLITE_ROW) {
        return ENGINE_ROW;
    }
    // handle other errors: BUSY etc
    //TODO: replace with proper logging message
    Logger::info("Statement executed");

    return rc;
}
//reset a stmt
void PreparedStatement::reset() {
    if(finalized)
        throw StatementStateError("Cannot call bind() on a finalized or uninitialized statement.", 1); // after statement is finalized, operation not permitted
    sqlite3_reset(stmt);
    isReset=true;
    Logger::info("Statement reset");
}
//finalize a prepared stmt
void PreparedStatement::finalize() {
    if(!stmt) { return; }
    if(sqlite3_finalize(stmt)!=SQLITE_OK)
        throw std::runtime_error("Finalize failed "+ std::string(db_->getLastErrorMsg()));
    finalized=true;
    stmt = nullptr;
    Logger::info("Statement Finalized");
}
//bindings:
/*
* TODO: imlpement exceptions for
* Error codes:
* SQLITE_OK: 0 Success // Return or proceed
* SQLITE_RANGE: 25 Parameter index is out of range // BindRangeError
* SQLITE_NOMEM: 7 Out of memory // Resource exception
*/
//bind text  
void PreparedStatement::bind(int index, const char* value) {
    if(!stmt)
        throw StatementStateError("Cannot call bind() on a finalized or uninitialized statement.", 1); 
    if(!isReset)
        throw StatementStateError("Statement must be reset() before binding", 1);
    int rc=sqlite3_bind_text(stmt, index, value, -1, SQLITE_TRANSIENT);
    if(rc != SQLITE_OK) {
        if(rc == SQLITE_RANGE)
            throw BindRangeException("Parameter index is out of range " + std::to_string(index) + "(text)", rc);
        if(rc == SQLITE_NOMEM)
            throw ResourceException("Out of memory", rc);
    }
}
//bind text string
void PreparedStatement::bind(int index, std::string value) {
    if(!stmt)
        throw StatementStateError("Cannot call bind() on a finalized or uninitialized statement.", 1);
    if(!isReset)
        throw StatementStateError("Statement must be reset() before binding", 1);
    int rc=sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    if(rc != SQLITE_OK) {
        if(rc == SQLITE_RANGE)
            throw BindRangeException("Parameter index is out of range " + std::to_string(index) + "(text)", rc);
        if(rc == SQLITE_NOMEM)
            throw ResourceException("Out of memory", rc);
    }
}
//bind int
void PreparedStatement::bind(int index, int value) {
    if(!isReset)
        throw StatementStateError("Statement must be reset() before binding", 1);
    int rc=sqlite3_bind_int(stmt, index, value);
    if(rc != SQLITE_OK) {
        if(rc == SQLITE_RANGE)
            throw BindRangeException("Parameter index is out of range " + std::to_string(index) + "(text)", rc);
        if(rc == SQLITE_NOMEM)
            throw ResourceException("Out of memory", rc);
    }
}
//bind double
void PreparedStatement::bind(int index, double value) {
    if(!isReset)
        throw StatementStateError("Statement must be reset() before binding", 1);

    int rc=sqlite3_bind_double(stmt, index, value);
    if(rc != SQLITE_OK) {
        if(rc == SQLITE_RANGE)
            throw BindRangeException("Parameter index is out of range " + std::to_string(index) + "(text)", rc);
        if(rc == SQLITE_NOMEM)
            throw ResourceException("Out of memory", rc);
    }
}
//bind null
void PreparedStatement::bind(int index) {
    if(!isReset)
        throw StatementStateError("Statement must be reset() before binding", 1);

    int rc=sqlite3_bind_null(stmt, index);
    if(rc != SQLITE_OK) {
        if(rc == SQLITE_RANGE)
            throw BindRangeException("Parameter index is out of range " + std::to_string(index) + "(text)", rc);
        if(rc == SQLITE_NOMEM)
            throw ResourceException("Out of memory", rc);
    }
}
//bind bool
void PreparedStatement::bind(int index, bool value) {
    if(!isReset)
        throw StatementStateError("Statement must be reset() before binding", 1);

    int rc=sqlite3_bind_int(stmt, index, value ? 1:0);
    if(rc != SQLITE_OK) {
        if(rc == SQLITE_RANGE)
            throw BindRangeException("Parameter index is out of range " + std::to_string(index) + "(text)", rc);
        if(rc == SQLITE_NOMEM)
            throw ResourceException("Out of memory", rc);
    }
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

// For statement-level profiling:
//
//     flags to be implemented per statement: 
//     - "executed_in_tx"
//     - "is_cached"
//




