#pragma once
#ifndef APP_H
#define APP_H
#include "db.hpp"
#include <vector>
#include <string>
#include <optional>
#include <common.hpp>
#include <memory>

using namespace Engine;
namespace App {

    enum class GiftStatus{
        IDEA,
        ORDERED,
        PURCHASED,
        CANCELLED
    };

    struct User {
        std::string budget;
        std::string total_spent;
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
        int eventID=0;
        std::string name;
        std::string link;
        double price = 0.0;
        GiftStatus status = GiftStatus::IDEA;
    };

    struct RecipientGifts {
        int recipientId;
        int giftId;
        std::string recipientName;
        std::string recipientRelationship;
        std::string giftName;
        std::string giftLink;
        std::string recipientBudget;
        std::string giftPrice;
        std::string giftStatus;
    };

    struct Event {
        int eventId;
        std::string event_name;
        std::string event_date;
    };

    class GiftPlanner {
        public:
            GiftPlanner(const std::string& filename);
            ~GiftPlanner();
            
            void initialize_tables();

            void addRecipient(Recipient recipient);
            void addGift(Gift gift);
            void markGiftAsPurchased(int giftId);
            std::vector<RecipientGifts> fetchUsersAndGifts(int limit=-1, int offset=-1, const std::string& order="");
            int getEventCount();
            int getRecipientCount();
            int getGiftCount();
            int totalGiftsPurchased();
            int totalMoneySpent();
            
        private:
            DBEngine* db;
    };

}

#endif
