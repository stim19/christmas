#include "imgui.h"
#include "app.hpp"

// Screen: Setup Screen
void SetupScreen() {
    static char chbuf[100];
    static int count=0;
    static std::string name="";
    User user;
    GiftPlanner& MyApp = appManager.getApp();
    ImGui::Begin("Welcome Screen");
    ImGui::Text("Welcome to GiftTracker");
    ImGui::Text("Let's get you set up so gift-giving is EASY and FUN!");
            
    if(count==0){
        if(ImGui::Button("Ok")) {
            count++;
        }
    }
    if(count==1) {
        ImGui::InputText("What's your name?", chbuf, IM_ARRAYSIZE(chbuf));
        name = chbuf;
        if(ImGui::Button("Next"))
            count++;
    }

    if(count>=2) {
        ImGui::Text("You are all set,");
        ImGui::SameLine();
        ImGui::Text(name.c_str()); ImGui::SameLine();
        ImGui::Text("!");
        if(ImGui::Button("Back"))
            count--;
        ImGui::SameLine();
        if(ImGui::Button("Continue")){
            user.name = name;
            MyApp.setup(user);
            ChangeMenus(2);
        }
    }
    ImGui::End();
}

