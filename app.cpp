#include <iostream> 
#include <app.hpp>

namespace App {

    GiftPlanner::GiftPlanner(const std::string& filename) {
        db=new DBEngine(filename, false);
    }
    GiftPlanner::~GiftPlanner() {
        if(db){
            db->~DBEngine();
            db=nullptr;
        }
    }
    void GiftPlanner::initialize_tables(){
        
        // create Recipients table
        Transaction tx(db);
        std::string recipients_table = R"(
        CREATE TABLE IF NOT EXISTS RECIPIENTS (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL,
            Relationship TEXT,
            Budget TEXT 
            );
        )";

        std::string gifts_table = R"(
        CREATE TABLE IF NOT EXISTS GIFTS (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            RecipientID INTEGER NOT NULL,
            EventID INTEGER NOT NULL,
            Name TEXT NOT NULL,
            Link TEXT,
            Price TEXT,
            Status INTEGER DEFAULT 0,
            Date TEXT,
            FOREIGN KEY(RecipientID) REFERENCES RECIPIENTS(ID) ON DELETE CASCADE,
            FOREIGN KEY(EventID) REFERENCES EVENTS(ID) ON DELETE CASCADE
            );
        )";

        std::string event_table = R"(
        CREATE TABLE IF NOT EXISTS EVENTS (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL UNIQUE,
            Date TEXT NOT NULL
            );
        )";

        std::string user_data = R"(
        CREATE TABLE IF NOT EXISTS USER (
            ID INTEGER PRIMARY KEY AUTOINCREMENT, 
            Name TEXT,
            Budget INTEGER,
            MoneySpent TEXT,
            LeftToBuy INTEGER,
            GiftsBought INTEGER
            );
        )";

        db->execute(event_table, "Create Event table");
        db->execute(recipients_table, "Create Recipients table");
        db->execute(gifts_table, "Create Gifts table");
        db->execute(user_data, "Create User data table");
        tx.commit();

    }

    void GiftPlanner::addRecipient(Recipient recipient) {
        Transaction tx(db);
        PreparedStatement stmt(db, "INSERT INTO RECIPIENTS VALUES(?, ?, ?);");
        stmt.bind(1, recipient.name);
        stmt.bind(2, recipient.relationship);
        stmt.bind(3, recipient.budgetLimit);
        stmt.step();
        tx.commit();
    }
    void GiftPlanner::addGift(Gift gift) {
        Transaction tx(db);
        PreparedStatement stmt(db, "INSERT INTO GIFTS VALUES(?, ?, ?, ?, ?);");
        stmt.bind(1, gift.recipientId);
        stmt.bind(2, gift.name);
        stmt.bind(3, gift.link);
        stmt.bind(4, gift.price);
        stmt.bind(5, static_cast<int>(gift.status));
        stmt.step();
        tx.commit();
    }

    void GiftPlanner::markGiftAsPurchased(int giftId) {
        Transaction tx(db);
        PreparedStatement stmt(db, "UPDATE GIFTS SET Status = ? WHERE ID = ?;");
        stmt.bind(1, giftId);
        stmt.bind(2, static_cast<int>(GiftStatus::PURCHASED));
        stmt.step();
        tx.commit();
    }
   
    std::vector<RecipientGifts> GiftPlanner::fetchUsersAndGifts(int limit, int offset, const std::string& order){
        int columnCount=0;
        std::vector<RecipientGifts> rows;
        
        std::string query = "\
            SELECT \
                recipients.id,\
                recipients.name,\
                recipients.relationship,\
                recipients.budget,\
                events.id\
                gifts.id\
                gifts.name,\
                gifts.link,\
                gifts.price,\
                gifts.status\
            FROM recipients\
            INNER JOIN\
                gifts ON recipients.id = gifts.recipientid";
        
        if(limit!=-1 && offset!= -1) {
            query+= " LIMIT ? OFFSET ?";
        }

        if(!order.empty()) {
            query += " ORDER BY ?";
        }

        query += ";";

        PreparedStatement stmt(db, query);
        while(stmt.step()==ENGINE_ROW) {
            Row r(stmt.get());
            RecipientGifts row;
            row.recipientId = r.get<int>(1);
            row.recipientName = r.get<std::string>(2);
            row.recipientRelationship = r.get<std::string>(3).empty() ? "" : r.get<std::string>(3);
            row.recipientBudget =  r.get<std::string>(4).empty() ? "" : r.get<std::string>(4);
            row.giftId = r.get<int>(5);
            row.giftName = r.get<std::string>(6);
            row.giftLink =  r.get<std::string>(7).empty() ? "" : r.get<std::string>(7);
            row.giftPrice = r.get<std::string>(8).empty() ? "" : r.get<std::string>(8);
            row.giftStatus = r.get<int>(9);
            rows.push_back(row); 
        }
        return rows;
    }

    int GiftPlanner::getEventCount() {
        std::string query = "SELECT COUNT(*) FROM EVENTS;";
        PreparedStatement stmt(db, query);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(1);
    }
    int GiftPlanner::getRecipientCount() {
        std::string query = "SELECT COUNT(*) FROM RECIPIENTS;";
        PreparedStatement stmt(db, query);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(1);
    }
    int GiftPlanner::getGiftCount() {
        std::string query = "SELECT COUNT(*) FROM GIFTS;";
        PreparedStatement stmt(db, query);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(1);
    }
    int GiftPlanner::totalGiftsPurchased() {
        int status = static_cast<int>(GiftStatus::PURCHASED);
        std::string query = "SELECT COUNT(*) FROM GIFTS WHERE STATUS = ?;";
        PreparedStatement stmt(db, query);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(1);
    }
    

}
