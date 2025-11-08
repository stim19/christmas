#ifndef DB_H
#define DB_H
#include <string>
#include <vector>
#include <sqlite3.h>

struct Recipient {
    int id = 0;
    std::string name;
    std::string relationship;
    double budgetLimit = 0.0;
};


class DBManager {
    public:
        bool Initialize(const std::string& dbPath);
        void Close();

        bool AddRecipient(const std::string& name, const std::string& relationship);
        std::vector<Recipient> LoadUsers();

    private:
        sqlite3* db = nullptr;
        bool execute(const std::string& sql, const std::string& msg);
};

#endif
