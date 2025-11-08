#include "db.h"
#include <iostream>
#include <cstdio>

using namespace std;

int main() {
    DBManager db;
    db.Initialize("test.db");
    std::string name, relationship;
    std::cin>>name;
    std::cin>>relationship;
    db.AddRecipient(name, relationship);
    auto allRecipients = db.LoadUsers();
    for (auto& r : allRecipients) {
        std::cout<<r.id<<" "<<r.name<<" "<<r.relationship<<std::endl;
    }    
    return 0;
}
