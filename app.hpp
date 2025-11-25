#ifndef APP_H
#define APP_H
#include "db.hpp"
#include <vector>
#include <string>
#include <optional>
#include <common.hpp>
#include <memory>

namespace App {

    enum class GiftStatus{
        IDEA,
        ORDERED,
        PURCHASED,
        CANCELLED
    };

    struct User {
        int id = 0;
        std::string name;
        double budget = 0.0;
        double total_spent =0.0;
    };
   
    struct Recipient {
        int id = 0;
        std::string name;
        std::string relationship;
        double budgetLimit = 0.0;
    };
    
    struct Gift {
        int id = 0;
        int recipientId = 0;
        int eventId=0;
        std::string name;
        std::string link;
        double price;
        GiftStatus status = GiftStatus::IDEA;
    };

    struct RecipientGifts {
        int recipientId;
        int giftId;
        std::string recipientName;
        std::string recipientRelationship;
        std::string giftName;
        std::string giftLink;
        double recipientBudget;
        double giftPrice;
        GiftStatus giftStatus;
        std::string eventName;
        std::string eventDate;
    };

    struct Event {
        int eventId=0;
        std::string eventName;
        std::string eventDate;
    };

    class GiftPlanner {
        public:
            GiftPlanner(const std::string& filename);
            ~GiftPlanner();
            
            void initialize_tables();

            void addRecipient(Recipient recipient);
            void addGift(Gift gift);
            void addEvent(Event event);
            void markGiftAsPurchased(int giftId);
            std::vector<RecipientGifts> fetchRecipientsAndGifts(int limit=-1, int offset=-1);
            int getEventCount();
            int getRecipientCount();
            int getGiftCount();
            int totalGiftsPurchased();
            int totalMoneySpent();
            bool setupComplete();
            void setup(User user);
            User getUserData();
            std::vector<Event>getEvents();
            std::vector<Recipient> getRecipients();
            
        private:
            Engine::DBEngine* db;
    };

}

#endif
