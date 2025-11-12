#ifndef DB_H
#define DB_H
#include <string>
#include <vector>
#include <sqlite3.h>
#include <mutex>
#include <map>
#include <stdexcept>

struct Recipient {
    int id = 0;
    std::string name;
    std::string relationship;
    double budgetLimit = 0.0;
};

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
        void bind(int index, int value);
        void bind(int index, const std::string& value);
        bool step();
        void reset();
        void finalize();
    private:
        sqlite3_stmt* stmt;
};

#endif
