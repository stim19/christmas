#pragma once

enum class UIScreen {
    Setup,
    People,
    Gifts,
    Occasions
};

void UI_Draw();
void UI_ChangeScreen(UIScreen screen);
