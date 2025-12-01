#include <iostream> 
#include <app.hpp>

using namespace Engine;
namespace App {

    //convert string to double
    double strToDouble(const std::string& str) {
        return std::stod(str);
    }
    std::string doubleToStr(const double num) {
        return std::to_string(num);
    }

    void GiftPlanner::init(const std::string& filename) {
        db=new DBEngine(filename, false);
    }
    GiftPlanner::~GiftPlanner() {
        if(db){
            delete(db);
            db = nullptr;
        }
    }
    void GiftPlanner::initialize_tables(){
        
        // create Recipients table
        Transaction tx(db);
        std::string recipients_table = R"(
        CREATE TABLE IF NOT EXISTS RECIPIENTS (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            Name TEXT NOT NULL,
            Relationship TEXT
            );
        )";

        std::string gifts_table = R"(
        CREATE TABLE IF NOT EXISTS GIFTS (
            ID INTEGER PRIMARY KEY AUTOINCREMENT,
            RecipientID INTEGER NOT NULL,
            EventID INTEGER NOT NULL,
            Name TEXT NOT NULL,
            Link TEXT,
            Budget TEXT,
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
        PreparedStatement stmt(db, "INSERT INTO RECIPIENTS(name, relationship) VALUES(?, ?);");
        stmt.bind(1, recipient.name);
        stmt.bind(2, recipient.relationship); 
        stmt.step();
        tx.commit();
    }
    void GiftPlanner::addGift(Gift gift) {
        Transaction tx(db);
        PreparedStatement stmt(db, "INSERT INTO GIFTS(recipientId, name, link, price, status, eventId, budget) VALUES(?, ?, ?, ?, ?, ?, ?);");
        stmt.bind(1, gift.recipientId);
        stmt.bind(2, gift.name);
        stmt.bind(3, gift.link);
        stmt.bind(4, doubleToStr(gift.price));
        stmt.bind(5, static_cast<int>(gift.status));
        stmt.bind(6, gift.eventId);
        stmt.bind(7, doubleToStr(gift.budgetLimit));
        stmt.step();
        tx.commit();
    }
    void GiftPlanner::addEvent(Event event) {
        Transaction tx(db);
        PreparedStatement stmt(db, "INSERT INTO EVENTS(name, date) VALUES(?, ?);");
        stmt.bind(1, event.eventName);
        stmt.bind(2, event.eventDate);
        stmt.step();
        tx.commit();
    }

    void GiftPlanner::markGiftAsPurchased(int giftId) {
        Transaction tx(db);
        PreparedStatement stmt(db, "UPDATE GIFTS SET Status = ? WHERE ID = ?;");
        stmt.bind(1, static_cast<int>(GiftStatus::PURCHASED));
        stmt.bind(2, giftId);        stmt.step();
        tx.commit();
    }
   
    std::vector<RecipientGifts> GiftPlanner::fetchRecipientsAndGifts(int eventId, int limit, int offset){
        int columnCount=0;
        std::vector<RecipientGifts> rows;
        bool paged=false;
        
        std::string query = "SELECT recipients.id, recipients.name, recipients.relationship, "
                            "gifts.id AS giftId, gifts.name AS giftName, gifts.link, gifts.budget, gifts.price, gifts.status, "
                            "events.name, events.date "
                            "FROM gifts "
                            "JOIN recipients ON recipients.id = gifts.recipientid "
                            "JOIN events ON events.id = gifts.eventId "
                            "WHERE events.id = ?";
        
        if(limit>-1 && offset> -1) {
            query+= " LIMIT ? OFFSET ?";
        }

        query += ";";

        PreparedStatement stmt(db, query);
        
        stmt.bind(1,eventId);
        if(paged){
            stmt.bind(2, limit);
            stmt.bind(3, offset);
        }
        while(stmt.step()==ENGINE_ROW) {
            Row r(stmt.get());
            RecipientGifts row;
            row.recipientId = r.get<int>(0);
            row.recipientName = r.get<std::string>(1);
            row.recipientRelationship = r.get<std::string>(2).empty() ? "" : r.get<std::string>(2);
            row.giftId = r.get<int>(3);
            row.giftName = r.get<std::string>(4);
            row.giftLink =  r.get<std::string>(5).empty() ? "" : r.get<std::string>(5);
            row.giftBudget =  r.get<std::string>(6).empty() ? 0.0 : strToDouble(r.get<std::string>(6));
            row.giftPrice = r.get<std::string>(7).empty() ? 0.0 : strToDouble(r.get<std::string>(7));
            row.giftStatus = static_cast<GiftStatus>(r.get<int>(8));
            row.eventName = r.get<std::string>(9).empty() ? "" : r.get<std::string>(9);
            row.eventDate = r.get<std::string>(10).empty() ? "" : r.get<std::string>(10);
            rows.push_back(row); 
        }
        return rows;
    }

    int GiftPlanner::getEventCount() {
        std::string query = "SELECT COUNT(*) FROM EVENTS;";
        PreparedStatement stmt(db, query);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(0);
    }
    int GiftPlanner::getRecipientCount() {
        std::string query = "SELECT COUNT(*) FROM RECIPIENTS;";
        PreparedStatement stmt(db, query);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(0);
    }
    int GiftPlanner::getGiftCount(int eventId) {
        std::string query = "SELECT COUNT(*) FROM GIFTS WHERE eventId = ?;";
        PreparedStatement stmt(db, query);
        stmt.bind(1, eventId);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(0);
    }
    
    int GiftPlanner::totalGiftsPurchased() {
        int status = static_cast<int>(GiftStatus::PURCHASED);
        std::string query = "SELECT COUNT(*) FROM GIFTS WHERE STATUS = ?;";
        PreparedStatement stmt(db, query);
        stmt.bind(1, status);
        stmt.step();
        Row r(stmt.get());
        return r.get<int>(0);
    }
    
    bool GiftPlanner::setupComplete() {
        std::string query = "SELECT * from USER";
        PreparedStatement stmt(db, query);
        stmt.step();
        Row r(stmt.get());
        if(r.get<int>(0)==0)
            return false;
        else 
            return true;
    }
    
    void GiftPlanner::setup(User user) {
        Transaction tx(db);
        std::string query = "INSERT INTO user (Name) VALUES(?)";
        PreparedStatement stmt(db, query);
        stmt.bind(1, user.name);
        stmt.step();
        tx.commit();
    }
    User GiftPlanner::getUserData() {
        std::string query = "SELECT * FROM USER LIMIT 1";
        PreparedStatement stmt(db, query);
        stmt.step();
        User user;
        Row r(stmt.get());
        user.id = r.get<int>(0);
        user.name = r.get<std::string>(1);
        return user;
    }
    
    std::vector<Event> GiftPlanner::getEvents() {
        std::vector<Event> events;
        std::string query = "SELECT * FROM EVENTS";
        PreparedStatement stmt(db, query);
        while(stmt.step() == ENGINE_ROW) {
            Row r(stmt.get());
            Event event;
            event.eventId = r.get<int>(0);
            event.eventName = r.get<std::string>(1);
            event.eventDate = r.get<std::string>(2);
            events.push_back(event);
        }
        
        return events;
    }
    std::vector<Recipient> GiftPlanner::getRecipients() {
        std::vector<Recipient> recipients;
        std::string query = "SELECT * from Recipients";
        PreparedStatement stmt(db, query);
        while(stmt.step() == ENGINE_ROW) {
            Row r(stmt.get());
            Recipient recipient;
            recipient.id = r.get<int>(0);
            recipient.name = r.get<std::string>(1);
            recipient.relationship = r.get<std::string>(2);
            recipients.push_back(recipient);
        }

        return recipients;
    }

}
