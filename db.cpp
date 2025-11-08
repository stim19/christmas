#include "db.h"
#include <iostream>

bool DBManager::Initialize(const std::string& dbFile) {
    if (sqlite3_open(dbFile.c_str(), &db)){
        std::cerr << "Couldn't connect to database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
        return false;
    }
    std::string sql_recipients = R"(
        CREATE TABLE IF NOT EXISTS RECIPIENTS (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL,
            Relationship TEXT
            );
        )";
    execute(sql_recipients, "Created RECIPIENTS table");
    std::string sql_gifts = R"(
        CREATE TABLE IF NOT EXISTS GIFT_IDEAS (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            RecipientID INTEGER NOT NULL,
            Name TEXT NOT NULL,
            Price REAL,
            Category TEXT,
            Status INTEGER DEFAULT 0, -- 0=Idea, 1=Purchased 
            FOREIGN KEY(RecipientID) REFERENCES RECIPIENTS(ID)
            );
    )";
    execute(sql_gifts, "Created GIFTS Table");
    return true;
}

bool DBManager::execute(const std::string& sql, const std::string& msg) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL Error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    std::cout << "[SQLITE SUCCESS]: " << msg << std::endl;
    return true;
}

bool DBManager::AddRecipient(const std::string& name, const std::string& relationship) {
    std::string query =  "INSERT OR IGNORE INTO RECIPIENTS (Name, Relationship) VALUES ('"+name+"', '"+relationship+"');";
    //std::cout<<query;
    execute(query, "Added new recipient");
    return true;
}

std::vector<Recipient> DBManager::LoadUsers(){
    using namespace std;
    vector<Recipient>recipients;
    sqlite3_stmt *stmt;
    string sql = "SELECT ID, Name, Relationship FROM RECIPIENTS ORDER BY ID;";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            Recipient r;
            
            r.id = sqlite3_column_int(stmt, 0);
            r.name = (sqlite3_column_text(stmt, 1))? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) : "";
            r.relationship = (sqlite3_column_text(stmt, 2))? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) : "";
            recipients.push_back(r);
        }
    }

    return recipients;
}

void DBManager::Close() {
    if (db) { 
        sqlite3_close(db);
        db = nullptr;
    }
}



