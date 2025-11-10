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



















