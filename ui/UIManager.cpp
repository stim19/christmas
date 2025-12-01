#include "imgui.h"
#include "UIManager.hpp"
//#include "PeopleList.hpp"
//#include "GiftsList.hpp"
//#include "OccasionsPage.hpp"

static UIScreen currentScreen = UIScreen::People;

void UIChangeScreen(UIScreen s) {
    currentScreen = s;
}
/*
void UI_Draw() {
    switch (currentScreen) {
        case UIScreen::People:
            DrawPeopleList();
            break;
        case UIScreen::Gifts:
            DrawGiftsList();
            break;
        case UIScreen::Occasions:
            DrawOccasionsPage();
            break;
    }
}
*/
