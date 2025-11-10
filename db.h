#ifndef DB_H
#define DB_H
#include <string>
#include <vector>
#include <sqlite3.h>
#include <memory>

struct Recipient {
    int id = 0;
    std::string name;
    std::string relationship;
    double budgetLimit = 0.0;
};


class DBEngine {
    public:
        DBEngine(const std::string& dbPath, bool debug=false);   // constructor opens a db file
        ~DBEngine();                                            // destructor closes the db file
    private:
        //TODO: Use Pointer to IMPLementation to hide implementation details like sqlite3* from DB interface
        //struct Impl;                                            // forward declaration
        //std::unique_ptr<Impl> pImpl; 
        sqlite3* db = nullptr;
        bool debugMode;
        void log(const std::string& msg);
};


#endif
