#ifndef DB_H
#define DB_H
#include <string>
#include <vector>
#include <sqlite3.h>
#include <mutex>
#include <map>
#include <stdexcept>
#include <variant>

struct Recipient {
    int id = 0;
    std::string name;
    std::string relationship;
    double budgetLimit = 0.0;
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
// Completed now users can perform easy type conversion using Row or Row templates


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
        
        DBEngine(const std::string& dbPath, bool debug=false);   // constructor opens a db file
        ~DBEngine();                                             // destructor closes the db file

        // Begins a new transaction. Throws if a transaction is already active
        void begin();

        // Commits the current transaction
        void commit();

        // Roll back the current transaction. Safe to call if no active transaction.
        void rollback();

        // Returns true if transaction active
        bool isActive() const { return active; }
        
        // Execute an arbitary SQL statement
        bool execute(const std::string& sql, const std::string& msg); 

        sqlite3* get();
        //TODO: implement query method with variants, use maps. use struct for rows and columns.

        
        DBEngine(const DBEngine&) = delete;
        DBEngine& operator=(const DBEngine&) = delete;
        DBEngine(DBEngine&&) = default;
        DBEngine& operator=(DBEngine&&) = default;

    private:
        //TODO: Use Pointer to IMPLementation to hide implementation details like sqlite3* from DB interface
        //struct Impl;                                            // forward declaration
        //std::unique_ptr<Impl> pImpl; 
        
        sqlite3* db = nullptr;
        bool active = false;                            // track active transactions
        std::mutex mtx;
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
class Transaction {
    public:
        explicit Transaction(DBEngine* dbEngine);
        ~Transaction(); 
        void commit();
    private:
        DBEngine* db;
        bool committed = false;
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
        PreparedStatement(sqlite3* db, const std::string& sql);
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
        void bind(int index, const std::string& value);
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
        sqlite3_stmt* stmt;
        bool finalized = false;
        bool prepared;
};

#endif
