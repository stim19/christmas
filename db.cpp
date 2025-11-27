#include "db.hpp"
#include "logger.hpp"
#include <iostream>

using namespace Engine;

/*
 * Class: LRUCache
 * LRU cache for sqlite3_stmt* statements
 */
LRUCache::LRUCache(size_t capacity) : capacity(capacity), head(nullptr), tail(nullptr) {
    if(capacity>1000){
        throw CacheLimitError("Max cache capacity is 1000, got "+ std::to_string(capacity)+"instead.", ENGINE_ERROR);
    }
}

// removes a node from the list. 
// doesn't finalize or delete from map.
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
}
//helper to move cache to MRU
void LRUCache::pushFront(Node* n) {
    n->prev = nullptr;
    n->next = head;
    if(head)
        head->prev = n;
    head=n;
    if(!tail)
        tail=n;
} 
// Unlink a node from the list, then move it to MRU
void LRUCache::moveToFront(Node* n) {
    removeNode(n);
    pushFront(n);
}
int LRUCache::get(const std::string& key, sqlite3_stmt* &stmt) {
    // stmt is OUT param
    // not found
    if(map.find(key) == map.end()) {
        Logger::info("[Cache]: Cache not found");
        return CACHE_NOT_FOUND;
    }

    auto it = map.find(key);

    //if found but is in use inform caller that cache is  busy 
    Node* n = it->second;
    if(n->inUse){
        Logger::info("[Cache]: Cache busy");
        return CACHE_BUSY;
    }
    //set cache state to in use, move to MRU, set stmt, return ok
    n->inUse = true;
    moveToFront(n);
    stmt = n->value;
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    activeMap[stmt] = n;
    Logger::info("[Cache]: Retrieved from cache");
    return CACHE_OK;
}
//finalize and delete a cache entry
int LRUCache::evict() {
    while(map.size() >= capacity){
        Node* n = tail;
        while(n && n->inUse) {
            n = n->prev;
        }
        if(!n)
            return CACHE_FULL;
        // free memory
        map.erase(n->key);
        // unlink
        removeNode(n);
        //finalize
        sqlite3_finalize(n->value);
        // remove from map
        delete(n);
    }
    Logger::info("[Cache]: Evicted cache entry");
    return CACHE_OK;
}

int LRUCache::put(const std::string& key, sqlite3_stmt* value) {
    auto it = map.find(key);
    if(it != map.end()) {
        Logger::warn("[Cache]: Attempted duplicate cache entry, key already exists.");
        return CACHE_DUPLICATE;
    }
    if(map.size() >= capacity) {
        int rc = evict();
        if(rc == CACHE_FULL) {
            Logger::info("[Cache]: Cache at max limit, all entries are in use.");
            return rc;
        }
    } 
    Node* n = new Node{key, value, nullptr, nullptr, false};
    pushFront(n);
    map[key] = n;
    Logger::info("[Cache]: Added to cache");
    return CACHE_OK;
}

int LRUCache::release(sqlite3_stmt* key) {
        if(activeMap.find(key)==activeMap.end()){
            return CACHE_NOT_FOUND;
        }
        Node* n = activeMap[key];
        if(!n->inUse){
            return CACHE_INVALID_STATE;
        }
        n->inUse = false;
        moveToFront(n);
        Logger::info("[Cache]: Releasing cache");
        return CACHE_OK;
}

int LRUCache::clearAll() {
    Node* cache = head;
    while(cache) {
        Node* next = cache->next;
        if(cache->inUse){
            Logger::warn("[Cache]: Failed to clear statement cache. Cache still in use by another operation.");
            return CACHE_BUSY;
        }
        if(cache->value)
            sqlite3_finalize(cache->value);
        delete cache;
        cache = next;
    }
    head = tail = nullptr;
    map.clear();
    activeMap.clear();
    Logger::info("[Cache]: Cleared statement cache");
    return CACHE_OK;
}
// end of Class: LRUCache



/*
 * Class: DBEngine
 */
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
        stmtCache->clearAll();
        stmtCache = nullptr;
        Logger::info("[DB]: Cleared statement cache");
    }
}



/**
 * Executes a SQL statement
 * @param sql The sql query string
 * @return ok if execution succeeds, error otherwise.
 */
//execute
int DBEngine::execute(const std::string& sql, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        Logger::error(std::string("[DB]: Failed to execute query: ") + msg + ": " + errMsg);
        sqlite3_free(errMsg);
        return ENGINE_ERROR;
    }

    Logger::info("[DB] OK: "+ msg);
    return ENGINE_OK;
}


/*
 * Prepares a sqlite3_stmt
 * @params: &sql, &stmtOut
 * returns int
 */
int DBEngine::prepare(const std::string &sql, sqlite3_stmt* &stmt) {
    sqlite3_stmt* _stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &_stmt, nullptr);
    if(rc != SQLITE_OK) {
        _stmt = nullptr;
        if(rc == SQLITE_ERROR) {
            std::string msg = getLastErrorMsg();
            Logger::info("[DB]: SQL syntax error: "+msg);
            return ENGINE_SYNTAX_ERROR;
        }
        else {
            Logger::info("[DB]: Failed to prepare statement");
            return ENGINE_ERROR;
        }
    } 
    Logger::info("[DB]: Prepare statement success");
    stmt = _stmt;    // gibst den stmt zuruck
    return ENGINE_OK;   // success
}

// get from cache
int DBEngine::getCached(const std::string& sql, sqlite3_stmt*& stmt) {
    int rc = stmtCache->get(sql, stmt);
    return rc;
}
int DBEngine::addToCache(const std::string& sql, sqlite3_stmt* stmt) {
    return stmtCache->put(sql, stmt);
}
int DBEngine::releaseCached(sqlite3_stmt* stmt){
    return stmtCache->release(stmt);
}

// Initiate a transaction
// The engine allows only one transaction at a time per connection
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
// end of Class: DBEngine





/*
 * Class: Transaction
 *
 */
Transaction::Transaction(DBEngine* dbEngine) : db(dbEngine) {
    db->begin();           // db initiates a transaction 
}
Transaction::~Transaction() {
    if(!committed) {
        db->rollback();    // db rollbacks the transaction
    }
}
int Transaction::getTransactionState() {
    return state;
};
void Transaction::commit() {
    db->commit();           // db commits changes
    committed = true;       
}
// end of Class: Transaction





/* 
 * Class: PreparedStatement
 *
 * Note: A stmt borrowed from cache should never be finalized by wrapper
 * 
 */
PreparedStatement::PreparedStatement(DBEngine* db, const std::string& sql):db_(db), _sql(sql) {
    
    Logger::info("Preparing statement");
    stmt = nullptr;
    sqlite3_stmt* _stmt = nullptr;
    int rc;
    rc = db->getCached(sql, _stmt);
    if(rc == CACHE_OK){
        isCached = true;
    }
    else{
        int tmp = rc;
        isCached = false;
        rc = db->prepare(sql, _stmt); 
        if(rc != ENGINE_OK) {
            stmt = nullptr;
            if(rc == ENGINE_SYNTAX_ERROR)
            {
                std::string msg = db_->getLastErrorMsg();
                throw SyntaxError("SQL Error during execution: "+ msg, rc);
            }
            else {
                std::string msg = db->getLastErrorMsg();
                throw std::runtime_error("Unexpected exception occurred: "+msg);
            }
        }
        else{
            if(tmp==CACHE_NOT_FOUND){
                rc = db->addToCache(sql, _stmt);
                // add to cache, should not finalize
                if(rc == CACHE_OK){
                    isCached = true;
                }
            }
        }
    }
    stmt = _stmt;
    _stmt = nullptr;
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
        throw ConstraintError("Database constraint violated: ", rc); 
    }
    if(rc == SQLITE_MISUSE){
        reset();
        Logger::info("SQL: "+_sql);
        throw std::runtime_error("SQLite Misuse: "+ std::string(db_->getLastErrorMsg()));
    }
    if(rc==SQLITE_ERROR) {
        reset();
        throw SyntaxError("SQL Error during execution: ", rc);
    }
    if(rc==SQLITE_MISMATCH) {
        reset(); 
        throw DatatypeMismatchError("Datatype mismatch on column binding ", rc);
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
    if(isCached) {
        db_->releaseCached(stmt);
        stmt = nullptr;
        Logger::info("Statement released");
        return;
    }
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

// end of Class: PreparedStatement

