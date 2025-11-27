#ifndef DB_H
#define DB_H
#include <string>
#include <vector>
#include <sqlite3.h>
#include <mutex>
#include <map>
#include <stdexcept>
#include <variant>
#include <common.hpp>
#include <exception.hpp>
#include <memory>
#include <unordered_map>
#include <list>

namespace Engine {

class DBEngine;
class Transactions;
class PreparedStatement;

class LRUCache;


enum ENGINE_CODES {
    ENGINE_OK,
    ENGINE_ERROR,
    ENGINE_CONNECTION_ERROR,
    ENGINE_ROLLBACK_FAILURE,
    ENGINE_COMMIT_FAILURE,
    ENGINE_SYNTAX_ERROR,
    ENGINE_STEP_ERROR,
    ENGINE_BIND_ERROR,
    ENGINE_ROW,
    ENGINE_FINALIZE_ERROR,
    ENGINE_BUSY,
    ENGINE_CACHE_OK,
    ENGINE_CACHE_BUSY,
    ENGINE_CACHE_NOT_FOUND
};

enum CACHE_RESULT {
    CACHE_OK,   // cache added or cache retrieved success
    CACHE_BUSY, // cache present but in use
    CACHE_FULL, // cache is at max capacity and every entry is in use
    CACHE_NOT_FOUND, // cache not found
    CACHE_DUPLICATE, // Duplicate entry
    CACHE_INVALID_STATE 
};
class LRUCache {
    private:
        struct Node {
            std::string key;
            sqlite3_stmt* value;
            Node* prev;
            Node* next;
            bool inUse;
        };
        size_t capacity;
        std::unordered_map<std::string, Node*> map;
        std::unordered_map<sqlite3_stmt*, Node*> activeMap;
        Node* head;
        Node* tail;
        void removeNode(Node* n);
        void pushFront(Node* n);
        void moveToFront(Node* n);
        int evict();
    public:
        LRUCache(size_t capacity);
        int put(const std::string& key, sqlite3_stmt* stmt);
        int get(const std::string& key, sqlite3_stmt* &stmt);
        int release(sqlite3_stmt* key);
        int clearAll();
};


class Row {
    public:
        explicit Row(sqlite3_stmt* stmt):
            stmt_(stmt){}
        
        template <typename T> T get(int col) const;
        
        //int
        int getInt(int col) const {
            return sqlite3_column_int(stmt_, col);
        }
        //long
        long long getInt64(int col) const {
            return sqlite3_column_int64(stmt_, col);
        }
        //double
        double getDouble(int col) const {
            return sqlite3_column_double(stmt_, col);
        }
        //raw text
        const unsigned char* getTextRaw(int col) const {
            return sqlite3_column_text(stmt_, col);
        }
        //string
        std::string getText(int col) const {
            const unsigned char* text = sqlite3_column_text(stmt_, col);
            if(!text)
                return "";
            return std::string(reinterpret_cast<const char*>(text));
        }
        //blob
        std::string getBlob(int col) const {
            const void* blob = sqlite3_column_blob(stmt_, col);
            int size = sqlite3_column_bytes(stmt_, col);
            if(!blob)
                return "";
            return std::string(static_cast<const char*>(blob), size);
        }
        //null
        bool isNull(int col) const {
            return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
        }

    private:
        sqlite3_stmt* stmt_;
};
// Templates specifications for Row
// int
template<>
inline int Row::get<int>(int col) const {
    return getInt(col);
}
//long
template<>
inline long long Row::get<long long>(int col) const {
    return getInt64(col);
}
//double
template<>
inline double Row::get<double>(int col) const {
    return getDouble(col);
}
//text
template<>
inline std::string Row::get<std::string>(int col) const {
    return getText(col);    
}
//null
template<>
inline bool Row::get<bool>(int col) const {
    return getInt(col)!=0;
}
// Completed. Users can now perform easy type conversion using Row templates


/* 
 * Class: DBEngine
 * 
 * Purpose: Manages a SQLite database connection. Provides manual transaction and SQL execution.
 *
 * Transaction in DBEngine can be managed in two ways:
 *  1. RAII-style using 'Transaction' (recommended)
 *  2. Manual Transaction control using 'DBEngine' directly
 *
 * Notes:
 *  - Tracks active transactions internally
 *  - Thread-safe SQL execution and transactions
 *  - 'DBEngine' tracks active transactions internally and will throw if operations are invalid 
 *  (e.g., commit without begin)
 *
 *  Example:
 *  DBEngine db("app.db")
 *  db.begin();
 *  db.execute("INSERT INTO users VALUES (1, 'foo');", "Insert into users"); // specify message
 *  db.commit();
 */
class DBEngine {
    public:
        
        DBEngine(const std::string& dbPath, bool debug=false, size_t cacheSize=16);   // constructor opens a db file
        ~DBEngine();                                             // destructor closes the db file

        // Begins a new transaction. Throws if a transaction is already active
        int begin();

        // Commits the current transaction
        int commit();

        // Roll back the current transaction. Safe to call if no active transaction.
        int rollback();

        // Returns true if transaction active
        bool isActive() const { return active; }
        
        // Execute an arbitary SQL statement
        int execute(const std::string& sql, const std::string& msg); 

        const char* getLastErrorMsg();
        
        int prepare(const std::string& sql, sqlite3_stmt* &stmt);

        int getCached(const std::string& sql, sqlite3_stmt*& stmt);
        int addToCache(const std::string& sql, sqlite3_stmt* stmt);
        int releaseCached(sqlite3_stmt* stmt);

        sqlite3* get();
        
        
        DBEngine(const DBEngine&) = delete;
        DBEngine& operator=(const DBEngine&) = delete;
        DBEngine(DBEngine&&) = default;
        DBEngine& operator=(DBEngine&&) = default;

    private:
        sqlite3* db = nullptr;
        bool active = false;                            // track active transactions
        std::mutex mtx;
        size_t cacheSize;
        LRUCache* stmtCache;
};


/* 
 * Class: Transaction
 * 
 * Create a Transaction object with a reference pointer to 'DBEngine'
 * 
 * Notes:
 *  - All operations within the scope of the 'Transaction' are part of the transaction
 *  - If the 'Transaction' is destroyed without 'commit()', changes are automatically rolled back
 *  
 *  The recommended way is to use 'Transaction'
 *  Manual control is supported for advanced use cases, but the caller is responsible for proper commit/rollback
 * 
 * Example:
 * {
 *   Transaction t(&db);
 *   db.execute("INSERT INTO test VALUES(1, 'foo');");
 *   t.commit();
 * }
 *
 */
enum class TransactionState {
    NONE,
    ACTIVE,
    COMMITTED,
    ROLLED_BACK,
    ERROR
};

class Transaction {
    public:
        explicit Transaction(DBEngine* dbEngine);
        ~Transaction();
        int getTransactionState();
        void commit();
    private:
        DBEngine* db;
        bool committed = false;
        int state;
};



/*
 * Class : PreparedStatement
 *
 * Example:
 * {
 *   PreparedStatement stmt(db, "SELECT Id, Name FROM users WHERE(?, ?);");
 *   stmt.bind(1, 1);
 *   stmt.bind(2, "foo");
 *   stmt.step();
 *  }
 */
class PreparedStatement {
    public:
        PreparedStatement(DBEngine* db, const std::string& sql);
        ~PreparedStatement();
        //bind int
        void bind(int index, int value);
        //bind int64
        void bind(int index, long long value);
        //bind double
        void bind(int index, double value);
        //bind null
        void bind(int index);
        //bind text
        void bind(int index, const char* value);
        //bind text string
        void bind(int index, std::string value);
        //bind blob
        void bind(int index, const void*data, int size);
        //bind bool
        void bind(int index, bool value);
        int step();
        void reset();
        void finalize();
        
        bool isFinalized();
        bool isPrepared();

        int columnCount();
        const char* columnName(int index);
        int columnType(int index);
        bool hasParameter(const std::string& name);
        int getParameterCount();
        std::string getParameterName(int index);
        int getParameterIndex(const std::string& name);
        
        sqlite3_stmt* get();

    private:
        DBEngine* db_;
        sqlite3_stmt* stmt;
        bool finalized = false;
        bool prepared;
        bool isCached;
        //TODO: implement proper state enums
        bool isReset=true;
        std::string _sql;
};


} // namespace Engine

#endif
