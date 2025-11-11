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

class Transaction;                  // forward declaration
class PreparedStatement;            // forward declaration

class DBEngine {
    public:
        DBEngine(const std::string& dbPath, bool debug=false);   // constructor opens a db file
        ~DBEngine();                                             // destructor closes the db file
        
        bool execute(const std::string& sql, const std::string& msg); 
        sqlite3* get();
        
        DBEngine(const DBEngine&) = delete;
        DBEngine& operator=(const DBEngine&) = delete;
        DBEngine(DBEngine&&) = default;
        DBEngine& operator=(DBEngine&&) = default;
    private:
        //TODO: Use Pointer to IMPLementation to hide implementation details like sqlite3* from DB interface
        //struct Impl;                                            // forward declaration
        //std::unique_ptr<Impl> pImpl; 
        
        sqlite3* db = nullptr;
        bool debugMode;
        void log(const std::string& msg);
        std::mutex mtx;
};

class Transaction {
    public:
        Transaction(sqlite3* db);
        ~Transaction();
        void begin();
        void commit();
        void rollback();
    private:
        sqlite3* db;
        bool active;
};

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
