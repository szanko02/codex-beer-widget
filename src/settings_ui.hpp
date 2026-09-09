#pragma once
#include "settings.hpp"
#include <functional>
#include <vector>

namespace beer {
struct SettingsActions {
    std::function<void(bool)> changed;
    std::function<bool(unsigned,unsigned)> hotkey;
    std::function<void()> refresh,restore,toggle_visibility,closed;
    std::function<void(bool)> demonstration;
    std::function<bool()> is_demo;
};
class SettingsWindow {
public:
    SettingsWindow(Settings& settings,SettingsActions actions);
    ~SettingsWindow();
    void open();
    HWND window()const{return window_;}
    void status(const std::wstring& text,const std::vector<std::pair<std::string,std::wstring>>& groups);
private:
    Settings& settings_;SettingsActions actions_;HWND window_{},tabs_{};HFONT font_{};UINT dpi_=96;
    struct Control{HWND window;int page,x,y,w,h;};std::vector<Control> controls_;
    std::vector<std::pair<std::string,std::wstring>> groups_;
    std::vector<std::filesystem::path> preset_files_;
    static LRESULT CALLBACK procedure(HWND,UINT,WPARAM,LPARAM);
    LRESULT message(UINT,WPARAM,LPARAM);
    HWND add(const wchar_t* type,const wchar_t* text,DWORD style,int id,int page,int x,int y,int w,int h);
    void slider(int id,int page,const wchar_t* label,int x,int y,int low,int high);
    void create_controls();void sync();void layout();void page();void change(bool persist=true);
    void presets();void save_preset();void load_preset();void choose_color(bool liquid);
};
}
