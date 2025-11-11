#include "db.h"
#include <iostream>
#include <cstdio>

using namespace std;

int main() {
    int i=0;
    DBEngine db("test.db", true);
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS test (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL,
            Relationship TEXT
            );
    )";
    db.execute(sql, "Create TEST table");

    PreparedStatement stmt(db.get(), "INSERT INTO users(Name, Relationship) VALUES(?, ?);");
    stmt.bind(1, "Jack");
    stmt.bind(2, "Family");
    stmt.step();
    stmt.reset();
    stmt.bind(1, "Mark");
    stmt.bind(2, "Family");
    stmt.step();
    return 0; 
}
