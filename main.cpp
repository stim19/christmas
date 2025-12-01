#include "db.hpp"
#include <iostream>
#include <cstdio>
#include <memory>
#include <variant>
#include <vector>
#include <app.hpp>
#include <cstdio>
#include <conio.h>
#include <csignal>
#include <limits>
#include <print>
#include <iomanip>
#include <ctime>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <string>           

// Ignore the imports for now



using namespace Engine;
using namespace App;



class Manager {
    int eventId;
    std::string eventName;
    std::string eventDate;
    User user;
    GiftPlanner* app=nullptr;
    public:
    void initApp(const std::string& db){
        app=new GiftPlanner();
        app->init(db);
    }
    ~Manager(){
        delete app;
        app = nullptr;
    }
    GiftPlanner& getApp(){
        if(!app)
            throw std::runtime_error("App not created");
        return *app;
    }
    void setUser(User u) { user = u; }
    void setEvent(int id, const std::string& name, const std::string& date){
        eventId = id;
        eventName = name;
        eventDate = date;
    }
    int getEventId() { return eventId; };
    std::string getEventName() { return eventName; }
    std::string getEventDate() { return eventDate; }
    std::string getUserName() { return user.name; }
    
};

static Manager appManager;

std::string getDateStr(const int day, const int month, const int year){
    std::tm date_info = {};
    date_info.tm_mday = day;
    date_info.tm_mon = month -1; // month is 0 indexed
    date_info.tm_year = year - 1900; // Year is years since 1900
    std::ostringstream oss;
    oss << std::put_time(&date_info, "%d-%m-%Y");
    std::string date_str = oss.str();
    return date_str;
}

struct Date{
    int day;
    int month;
    int year;
    std::time_t time;
};

Date parseDate(const std::string& date_str){
    Date date;
    std::tm t ={};
    std::istringstream ss(date_str);
    ss>>std::get_time(&t, "%d-%m-%Y");
    if(ss.fail())
        throw std::runtime_error("Couldn't parse date string.");
    else {
        date.day = t.tm_mday;
        date.month = t.tm_mon + 1; // month starts at 0
        date.year = t.tm_year + 1900;
        date.time = std::mktime(&t);
    }
    return date;
}

static bool isDateValid(int day, int month , int year){
    //check leap
    bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if(year <2024) return false;
    if (month >0 && month <=12){
        if(day < 1 || day >31) return false;
        else if(month == 2){
            if(isLeap){
                if(day > 29) return false;
            }
            else{
                if(day > 28) return false;
            }
        }
        else {
            if(month % 2 == 1 && day > 30){
                return false;
            }
        }
        
        return true;  
    }
    return false;    
}

//===========================================================
//                      ImGui
//===========================================================

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Screens                                      
static bool ShowSetupScreen     = true;        // 01
static bool ShowMainMenu        = false;         // 02
//static int current_screen       = 1;            // setup screen  
//TODO:

static void ChangeMenus(const int screen){
    if(ShowSetupScreen)
        ShowSetupScreen = false;
    if(ShowMainMenu)
        ShowMainMenu = false;
    if(screen == 1)
        ShowSetupScreen = true;
    if (screen == 2)
        ShowMainMenu = true;
}

//TODO: Select items from dropdown 
static void DemoWindowWidgetsTextFilter()
{
    if (ImGui::TreeNode("Text Filter"))
    {
        // Helper class to easy setup a text filter.
        // You may want to implement a more feature-full filtering scheme in your own application.
        static ImGuiTextFilter filter;
        ImGui::Text("Filter usage:\n"
            "  \"\"         display all lines\n"
            "  \"xxx\"      display lines containing \"xxx\"\n"
            "  \"xxx,yyy\"  display lines containing \"xxx\" or \"yyy\"\n"
            "  \"-xxx\"     hide lines containing \"xxx\"");
        filter.Draw();
        const char* lines[] = { "aaa1.c", "bbb1.c", "ccc1.c", "aaa2.cpp", "bbb2.cpp", "ccc2.cpp", "abc.h", "hello, world" };
        for (int i = 0; i < IM_ARRAYSIZE(lines); i++)
            if (filter.PassFilter(lines[i]))
                ImGui::BulletText("%s", lines[i]);
        ImGui::TreePop();
    }
}

// Screen: Setup Screen
static void SetupScreen() {
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

static void DisplayGiftsTable() {
    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    ImVec2 outer_size = ImVec2(0.0f, TEXT_BASE_HEIGHT * 15);    // table height

    GiftPlanner& MyApp = appManager.getApp();
    int GiftCount = MyApp.getGiftCount();

    if(ImGui::BeginTable("Gifts", 8, flags, outer_size)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Gift", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Relationship", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Budget", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Link", ImGuiTableColumnFlags_None);
        ImGui::TableHeadersRow();
        
        ImGuiListClipper clipper;
        clipper.Begin(GiftCount);
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                ImGui::TableNextRow();
                for (int column = 0; column < 3; column++)
                {
                    ImGui::TableSetColumnIndex(column);
                    ImGui::Text("Hello %d,%d", column, row);
                }
            }
        }
        
        ImGui::EndTable(); 
    } // table
}

static void EventsTab(){
    
    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    ImVec2 outer_size = ImVec2(0.0f, TEXT_BASE_HEIGHT * 12);    // table height
    
    GiftPlanner& MyApp = appManager.getApp();
    int EventCount = MyApp.getEventCount();
    static char chbuf1[100];
    static std::string name ="";
    static std::string date = "";
    static int day, month, year;
    static bool valid = true;   // by default true for displaying error correctly
    static std::vector<Event> events = MyApp.getEvents();
    
    ImGui::SeparatorText("Create Event");
    ImGui::InputText("Event Name", chbuf1, IM_ARRAYSIZE(chbuf1));
    name = chbuf1;
    ImGui::InputInt("Day", &day);
    ImGui::InputInt("Month", &month);
    ImGui::InputInt("Year", &year);
    if(ImGui::Button("Add")) {
       valid = isDateValid(day, month, year);
       if(valid)
       {
           Event e;
           e.eventName = name;
           e.eventDate = getDateStr(day, month, year);
           MyApp.addEvent(e);
           ImGui::Text("Added new event");
           events = MyApp.getEvents();
           EventCount = MyApp.getEventCount();
       }
    }
    
    if(!valid) ImGui::Text("Invalid Date");
    ImGui::SeparatorText("Events");
    if(ImGui::BeginTable("Events", 3, flags, outer_size)){
        ImGui::TableSetupScrollFreeze(0,1);
        ImGui::TableSetupColumn("No.", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_None);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(EventCount);
        while(clipper.Step())
        {
            for (int row = clipper.DisplayStart; row<clipper.DisplayEnd; row++){
                ImGui::TableNextRow();
                for(int i=0; i<events.size(); i++){
                    ImGui::TableSetColumnIndex(i);
                    ImGui::Text("%d", events[i].eventId);
                    ImGui::TableNextColumn();
                    ImGui::TableSetColumnIndex(i+1);
                    ImGui::Text("%s", events[i].eventName.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TableSetColumnIndex(i+2);
                    ImGui::Text("%s", events[i].eventDate.c_str());
                }
            }
        }
        ImGui::EndTable();
    }
}

static void MenuTabs() {
    
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("Gifts"))
        {
            ImGui::Text("This is the Avocado tab!\nblah blah blah blah blah");
            DisplayGiftsTable();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Events"))
        {
            ImGui::Text("This is the Broccoli tab!\nblah blah blah blah blah");
            EventsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("People"))
        {
            ImGui::Text("This is the Cucumber tab!\nblah blah blah blah blah");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::Separator();
}

int main() {
   
//=========================================================
//              Setup ImGui and Graphics 
//=========================================================

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(720 * main_scale), "GiftTracker v0.1.0-pre", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    
    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale); 
    style.FontScaleDpi = main_scale;        
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
    io.ConfigDpiScaleFonts = true;          
    io.ConfigDpiScaleViewports = true;      
#endif

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

//========================================================
//                    Setup app
//========================================================
    
    appManager.initApp("test_app2.db");
    GiftPlanner& MyApp = appManager.getApp();
    MyApp.initialize_tables();
    static const char* username;
    //TODO: Remove comment 
    if(MyApp.setupComplete()){
        ShowSetupScreen = false;
        ShowMainMenu = true;
    
        User user = MyApp.getUserData();
        appManager.setUser(user);
        username = appManager.getUserName().c_str();
    }

    bool show_demo_window = true;
    static ImGuiWindowFlags window_flags =  ImGuiWindowFlags_NoCollapse;
    //window_flags |=  ImGuiWindowFlags_NoMove;
    window_flags |=  ImGuiWindowFlags_NoResize;
    //
   
    
   
//=========================================================
//                    Main Loop
//=========================================================
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();
        if(ShowSetupScreen) {
            SetupScreen();
            User user = MyApp.getUserData();
            appManager.setUser(user);
            username = appManager.getUserName().c_str();
        }

        if(ShowMainMenu){
            ImVec2 mySize = ImVec2(900, 500); // width 900, height 700
            ImGui::SetNextWindowSize(mySize, ImGuiCond_Always);
            ImGui::Begin("Main Menu",NULL, window_flags);       

            ImGui::Text("Welcome...%s", username);
            ImGui::SeparatorText("");
            MenuTabs();
                        
            ImGui::End();
        } //main menu

        if(show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);


        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);

 
    } // main loop
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

}
