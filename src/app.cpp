#include "renderer.hpp"
#include <windowsx.h>
#include <algorithm>
#include <memory>
#include <string>

namespace {
constexpr wchar_t window_class[]=L"CodexBeerWidget.Window";
enum Command { ToggleTop=101,ToggleLock,ToggleClicks,Quit };
struct App {
    HWND window{},overlay{};
    std::unique_ptr<beer::Renderer> renderer;
    beer::Theme theme;
    bool top=true,locked=false,clicks=false;
    int height=240;
    void region() {
        RECT rc{}; GetClientRect(window,&rc);
        const double sx=rc.right/240.,sy=rc.bottom/300.;
        auto round=[&](int l,int t,int r,int b,int radius) {return CreateRoundRectRgn(int(l*sx),int(t*sy),int(r*sx)+1,int(b*sy)+1,int(radius*sx),int(radius*sy));};
        HRGN result=round(37,32,178,234,42), handle=round(158,77,214,184,40),hole=round(172,91,201,170,26);
        CombineRgn(handle,handle,hole,RGN_DIFF); CombineRgn(result,result,handle,RGN_OR);
        HRGN label=round(26,240,214,300,25); CombineRgn(result,result,label,RGN_OR);
        DeleteObject(handle); DeleteObject(hole); DeleteObject(label);
        if(!SetWindowRgn(window,result,TRUE)) DeleteObject(result);
    }
    void paint() {
        if(renderer) renderer->draw(theme,50,67,L"Демо · 50%",L"Тест окна",0,clicks?overlay:nullptr);
    }
    void size(int value) {
        height=std::clamp(value,80,400);
        const UINT dpi=GetDpiForWindow(window);
        const int h=MulDiv(height,dpi,96),w=MulDiv(h,240,300);
        SetWindowPos(window,top?HWND_TOPMOST:HWND_NOTOPMOST,0,0,w,h,SWP_NOMOVE|SWP_NOACTIVATE);
        RECT rc{}; GetWindowRect(window,&rc); SetWindowPos(overlay,top?HWND_TOPMOST:HWND_NOTOPMOST,rc.left,rc.top,w,h,SWP_NOACTIVATE);
        region(); paint();
    }
    void click_mode() {
        clicks=!clicks;
        if(clicks) { RECT rc{};GetWindowRect(window,&rc);SetWindowPos(overlay,top?HWND_TOPMOST:HWND_NOTOPMOST,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOACTIVATE);paint();ShowWindow(overlay,SW_SHOWNOACTIVATE);ShowWindow(window,SW_HIDE); }
        else {ShowWindow(overlay,SW_HIDE);ShowWindow(window,SW_SHOWNOACTIVATE);paint();}
    }
    void menu() {
        HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING|(top?MF_CHECKED:0),ToggleTop,L"Поверх остальных окон");
        AppendMenuW(menu,MF_STRING|(locked?MF_CHECKED:0),ToggleLock,L"Зафиксировать положение");
        AppendMenuW(menu,MF_STRING|(clicks?MF_CHECKED:0),ToggleClicks,L"Пропускать клики (Ctrl+Alt+B — вернуть)");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,Quit,L"Выход");
        POINT pt{};GetCursorPos(&pt);SetForegroundWindow(window);
        const int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);
        if(command==ToggleTop){top=!top;size(height);} if(command==ToggleLock)locked=!locked;
        if(command==ToggleClicks)click_mode();if(command==Quit)DestroyWindow(window);
        PostMessageW(window,WM_NULL,0,0);
    }
};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));app->window=window;}
    if(!app)return DefWindowProcW(window,message,w,l);
    try {
        switch(message) {
        case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(window,&pt);RECT rc{};GetClientRect(window,&rc);
            return !app->locked && pt.y < rc.bottom*240/300 ? HTCAPTION:HTCLIENT;
        }
        case WM_MOUSEWHEEL:if(GET_KEYSTATE_WPARAM(w)&MK_CONTROL){app->size(app->height+GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*10);return 0;}break;
        case WM_DPICHANGED:{auto* rc=reinterpret_cast<RECT*>(l);SetWindowPos(window,nullptr,rc->left,rc->top,rc->right-rc->left,rc->bottom-rc->top,SWP_NOZORDER|SWP_NOACTIVATE);app->size(app->height);return 0;}
        case WM_SIZE:if(app->renderer){app->renderer->resize(LOWORD(l),HIWORD(l));app->region();app->paint();}return 0;
        case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(window,&ps);EndPaint(window,&ps);app->paint();return 0;}
        case WM_ERASEBKGND:return 1;
        case WM_CONTEXTMENU:app->menu();return 0;
        case WM_HOTKEY:if(app->clicks)app->click_mode();return 0;
        case WM_CLOSE:DestroyWindow(window);return 0;
        case WM_DESTROY:UnregisterHotKey(window,1);PostQuitMessage(0);return 0;
        }
    }catch(const std::exception&){MessageBoxW(window,L"Не удалось создать или обновить графику Direct2D.",L"Codex Beer Widget",MB_ICONERROR);DestroyWindow(window);return 0;}
    return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR arguments,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    App app;
    WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=instance;wc.lpszClassName=window_class;wc.lpfnWndProc=procedure;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    RegisterClassExW(&wc);
    const bool inspect=std::wstring(arguments).find(L"--inspect")!=std::wstring::npos;
    HWND window=CreateWindowExW((inspect?WS_EX_APPWINDOW:(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))|WS_EX_NOREDIRECTIONBITMAP,window_class,L"Codex Beer Widget",WS_POPUP,120,120,192,240,nullptr,nullptr,instance,&app);
    if(!window){CoUninitialize();return 1;}
    app.overlay=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TRANSPARENT,L"STATIC",L"Codex Beer Widget — click through",WS_POPUP,120,120,192,240,nullptr,nullptr,instance,nullptr);
    try {
        app.renderer=std::make_unique<beer::Renderer>(window);
        if(!RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'B'))MessageBoxW(window,L"Ctrl+Alt+B занята. Режим пропуска кликов пока не используйте.",L"Codex Beer Widget",MB_ICONWARNING);
        app.size(240);ShowWindow(window,SW_SHOWNOACTIVATE);app.paint();
        MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    }catch(const std::exception&){MessageBoxW(window,L"Не удалось запустить Direct2D/DirectComposition.",L"Codex Beer Widget",MB_ICONERROR);}
    DestroyWindow(app.overlay);app.renderer.reset();if(IsWindow(window))DestroyWindow(window);CoUninitialize();return 0;
}
