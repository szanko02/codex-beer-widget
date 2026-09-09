#include "renderer.hpp"
#include "settings.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <memory>
#include <string>

namespace {
constexpr wchar_t window_class[]=L"CodexBeerWidget.Window";
enum Command { ToggleTop=101,ToggleLock,ToggleClicks,Quit,ToggleShow,OpenSettings,Restore,Refresh,
    SizeSlider=201,HotkeyControl,ApplySettings,AutorunControl,SnapControl };
constexpr UINT TrayMessage=WM_APP+1,RestoreMessage=WM_APP+7;
LRESULT CALLBACK settings_proc(HWND,UINT,WPARAM,LPARAM);
std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);std::string r(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),r.data(),n,nullptr,nullptr);return r;}
struct App {
    HWND window{},overlay{};
    HWND settings_window{};
    std::unique_ptr<beer::Renderer> renderer;
    beer::Settings settings;
    beer::Theme& theme=settings.theme;
    bool& top=settings.top;bool& locked=settings.locked;bool& clicks=settings.click_through;
    int& height=settings.height;
    int hotkey_id=1;
    bool hotkey_ok=false,quitting=false;
    UINT taskbar_created=RegisterWindowMessageW(L"TaskbarCreated");
    void save(){try{RECT rc{};GetWindowRect(window,&rc);settings.x=rc.left;settings.y=rc.top;
        MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        settings.monitor=utf8(monitor.szDevice);beer::save_settings(settings);
    }catch(const std::exception&){MessageBoxW(settings_window?settings_window:window,L"Не удалось сохранить настройки.",L"Codex Beer Widget",MB_ICONWARNING);}}
    void tray(bool remove=false){
        NOTIFYICONDATAW icon{};icon.cbSize=sizeof(icon);icon.hWnd=window;icon.uID=1;
        icon.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;icon.uCallbackMessage=TrayMessage;icon.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
        wcscpy_s(icon.szTip,L"Codex Beer Widget — показать / скрыть");
        Shell_NotifyIconW(remove?NIM_DELETE:NIM_ADD,&icon);
        if(!remove){icon.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&icon);}
    }
    void show(bool visible){settings.visible=visible;ShowWindow(window,visible&&!clicks?SW_SHOWNOACTIVATE:SW_HIDE);
        if(visible&&clicks)paint();ShowWindow(overlay,visible&&clicks?SW_SHOWNOACTIVATE:SW_HIDE);if(visible)paint();save();}
    void recover(bool primary=false){
        RECT rc{};GetWindowRect(window,&rc);HMONITOR monitor=primary?MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY):MonitorFromRect(&rc,MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};GetMonitorInfoW(monitor,&info);
        int x=std::clamp(rc.left,info.rcWork.left,(std::max)(info.rcWork.left,info.rcWork.right-(rc.right-rc.left)));
        int y=std::clamp(rc.top,info.rcWork.top,(std::max)(info.rcWork.top,info.rcWork.bottom-(rc.bottom-rc.top)));
        if(primary){x=info.rcWork.right-(rc.right-rc.left)-24;y=info.rcWork.bottom-(rc.bottom-rc.top)-24;}
        SetWindowPos(window,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);size(height);
        if(primary){clicks=false;show(true);}save();
    }
    bool register_hotkey(unsigned modifiers,unsigned key){
        if(hotkey_ok&&modifiers==settings.hotkey_modifiers&&key==settings.hotkey)return true;
        const int candidate=hotkey_id==1?2:1;
        if(!(modifiers&(MOD_CONTROL|MOD_ALT))||!key||!RegisterHotKey(window,candidate,modifiers|MOD_NOREPEAT,key))return false;
        if(hotkey_ok)UnregisterHotKey(window,hotkey_id);
        hotkey_id=candidate;hotkey_ok=true;settings.hotkey_modifiers=modifiers;settings.hotkey=key;return true;
    }
    void open_settings(){
        if(settings_window){ShowWindow(settings_window,SW_SHOWNORMAL);SetForegroundWindow(settings_window);return;}
        settings_window=CreateWindowExW(0,L"CodexBeerWidget.Settings",L"Настройки Codex Beer Widget",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
            CW_USEDEFAULT,CW_USEDEFAULT,480,320,nullptr,nullptr,GetModuleHandleW(nullptr),this);
        if(!settings_window)throw std::runtime_error("Cannot create settings window");
        ShowWindow(settings_window,SW_SHOWNORMAL);
        SetWindowPos(settings_window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW);
        SetForegroundWindow(settings_window);
    }
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
        if(renderer&&settings.visible) renderer->draw(theme,50,67,L"Демо · 50%",L"Тест окна",0,clicks?overlay:nullptr);
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
        clicks=!clicks;RECT rc{};GetWindowRect(window,&rc);SetWindowPos(overlay,top?HWND_TOPMOST:HWND_NOTOPMOST,rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top,SWP_NOACTIVATE);show(settings.visible);
    }
    void menu() {
        HMENU menu=CreatePopupMenu();
        AppendMenuW(menu,MF_STRING,ToggleShow,settings.visible?L"Скрыть":L"Показать");
        AppendMenuW(menu,MF_STRING,OpenSettings,L"Настройки…");
        AppendMenuW(menu,MF_STRING|(top?MF_CHECKED:0),ToggleTop,L"Поверх остальных окон");
        AppendMenuW(menu,MF_STRING|(locked?MF_CHECKED:0),ToggleLock,L"Зафиксировать положение");
        AppendMenuW(menu,MF_STRING|(clicks?MF_CHECKED:0),ToggleClicks,L"Пропускать клики");
        AppendMenuW(menu,MF_STRING,Restore,L"Вернуть на основной экран");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,Quit,L"Выход");
        POINT pt{};GetCursorPos(&pt);SetForegroundWindow(window);
        const int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,window,nullptr);DestroyMenu(menu);
        if(command==ToggleTop){top=!top;size(height);} if(command==ToggleLock)locked=!locked;
        if(command==ToggleShow)show(!settings.visible);if(command==OpenSettings)open_settings();if(command==Restore)recover(true);
        if(command==ToggleClicks)click_mode();if(command==Quit)DestroyWindow(window);
        if(command!=Quit)save();
        PostMessageW(window,WM_NULL,0,0);
    }
};
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE) {app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));app->window=window;}
    if(!app)return DefWindowProcW(window,message,w,l);
    if(message==app->taskbar_created){app->tray();return 0;}
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
        case WM_HOTKEY:app->show(!app->settings.visible);return 0;
        case TrayMessage:if(LOWORD(l)==WM_CONTEXTMENU)app->menu();else if(LOWORD(l)==NIN_SELECT||LOWORD(l)==NIN_KEYSELECT)app->show(!app->settings.visible);return 0;
        case RestoreMessage:app->recover(true);return 0;
        case WM_EXITSIZEMOVE:app->recover();return 0;
        case WM_MOVING:if(app->settings.snap){auto* rc=reinterpret_cast<RECT*>(l);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromRect(rc,MONITOR_DEFAULTTONEAREST),&info);
            const int distance=MulDiv(12,GetDpiForWindow(window),96),width=rc->right-rc->left,height=rc->bottom-rc->top;
            if(abs(rc->left-info.rcWork.left)<distance){rc->left=info.rcWork.left;rc->right=rc->left+width;}
            if(abs(rc->right-info.rcWork.right)<distance){rc->right=info.rcWork.right;rc->left=rc->right-width;}
            if(abs(rc->top-info.rcWork.top)<distance){rc->top=info.rcWork.top;rc->bottom=rc->top+height;}
            if(abs(rc->bottom-info.rcWork.bottom)<distance){rc->bottom=info.rcWork.bottom;rc->top=rc->bottom-height;}}
            return TRUE;
        case WM_DISPLAYCHANGE:app->recover();return 0;
        case WM_CLOSE:DestroyWindow(window);return 0;
        case WM_DESTROY:app->quitting=true;app->save();app->tray(true);if(app->settings_window)DestroyWindow(app->settings_window);UnregisterHotKey(window,app->hotkey_id);PostQuitMessage(0);return 0;
        }
    }catch(const std::exception&){MessageBoxW(window,L"Не удалось создать или обновить графику Direct2D.",L"Codex Beer Widget",MB_ICONERROR);DestroyWindow(window);return 0;}
    return DefWindowProcW(window,message,w,l);
}
LRESULT CALLBACK settings_proc(HWND window,UINT message,WPARAM w,LPARAM l){
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));}
    if(!app)return DefWindowProcW(window,message,w,l);
    switch(message){
    case WM_CREATE:{
        auto control=[&](const wchar_t* kind,const wchar_t* label,int style,int id,int x,int y,int width,int height){
            HWND child=CreateWindowExW(0,kind,label,WS_CHILD|WS_VISIBLE|style,x,y,width,height,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;};
        control(L"STATIC",L"Высота виджета: 80–400 DIP",0,0,20,20,400,24);
        HWND slider=control(TRACKBAR_CLASSW,L"Размер",TBS_AUTOTICKS|WS_TABSTOP,SizeSlider,20,48,420,36);
        SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELPARAM(80,400));SendMessageW(slider,TBM_SETPOS,TRUE,app->height);
        control(L"STATIC",L"Горячая клавиша показа / скрытия",0,0,20,98,400,24);
        HWND hotkey=control(HOTKEY_CLASSW,L"Горячая клавиша",WS_TABSTOP,HotkeyControl,20,125,240,28);
        unsigned modifiers=((app->settings.hotkey_modifiers&MOD_CONTROL)?HOTKEYF_CONTROL:0)|((app->settings.hotkey_modifiers&MOD_ALT)?HOTKEYF_ALT:0)|((app->settings.hotkey_modifiers&MOD_SHIFT)?HOTKEYF_SHIFT:0);
        SendMessageW(hotkey,HKM_SETHOTKEY,MAKEWORD(app->settings.hotkey,modifiers),0);
        control(L"BUTTON",L"Применить",BS_PUSHBUTTON|WS_TABSTOP,ApplySettings,285,125,155,28);
        HWND autorun=control(L"BUTTON",L"Запускать при входе в Windows",BS_AUTOCHECKBOX|WS_TABSTOP,AutorunControl,20,175,400,28);
        SendMessageW(autorun,BM_SETCHECK,app->settings.autorun?BST_CHECKED:BST_UNCHECKED,0);
        HWND snap=control(L"BUTTON",L"Привязывать к краям экрана",BS_AUTOCHECKBOX|WS_TABSTOP,SnapControl,20,215,400,28);
        SendMessageW(snap,BM_SETCHECK,app->settings.snap?BST_CHECKED:BST_UNCHECKED,0);return 0;}
    case WM_HSCROLL:if(GetDlgCtrlID(reinterpret_cast<HWND>(l))==SizeSlider){app->size(static_cast<int>(SendMessageW(reinterpret_cast<HWND>(l),TBM_GETPOS,0,0)));if(LOWORD(w)==TB_ENDTRACK)app->save();}return 0;
    case WM_COMMAND:
        if(LOWORD(w)==ApplySettings){const auto hot=SendDlgItemMessageW(window,HotkeyControl,HKM_GETHOTKEY,0,0);const auto flags=HIBYTE(hot);
            unsigned modifiers=((flags&HOTKEYF_CONTROL)?MOD_CONTROL:0)|((flags&HOTKEYF_ALT)?MOD_ALT:0)|((flags&HOTKEYF_SHIFT)?MOD_SHIFT:0);
            if(!app->register_hotkey(modifiers,LOBYTE(hot)))MessageBoxW(window,L"Комбинация занята или недопустима. Используйте Ctrl или Alt с буквой/цифрой.",L"Горячая клавиша",MB_ICONWARNING);else app->save();}
        if(LOWORD(w)==SnapControl){app->settings.snap=IsDlgButtonChecked(window,SnapControl)==BST_CHECKED;app->save();}
        if(LOWORD(w)==AutorunControl){const bool enabled=IsDlgButtonChecked(window,AutorunControl)==BST_CHECKED;try{beer::set_autorun(enabled);app->settings.autorun=enabled;app->save();}catch(...){CheckDlgButton(window,AutorunControl,app->settings.autorun?BST_CHECKED:BST_UNCHECKED);MessageBoxW(window,L"Не удалось изменить автозапуск.",L"Настройки",MB_ICONWARNING);}}return 0;
    case WM_CLOSE:DestroyWindow(window);return 0;
    case WM_DESTROY:app->settings_window=nullptr;return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR arguments,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\CodexBeerWidget.Instance");
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(HWND existing=FindWindowW(window_class,nullptr))PostMessageW(existing,RestoreMessage,0,0);if(singleton)CloseHandle(singleton);CoUninitialize();return 0;}
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES|ICC_HOTKEY_CLASS};InitCommonControlsEx(&controls);
    App app;
    std::string settings_error;app.settings=beer::load_settings(settings_error);
    WNDCLASSEXW wc{sizeof(wc)};wc.hInstance=instance;wc.lpszClassName=window_class;wc.lpfnWndProc=procedure;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    RegisterClassExW(&wc);
    WNDCLASSEXW sc=wc;sc.lpszClassName=L"CodexBeerWidget.Settings";sc.lpfnWndProc=settings_proc;sc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassExW(&sc);
    const bool inspect=std::wstring(arguments).find(L"--inspect")!=std::wstring::npos;
    HWND window=CreateWindowExW((inspect?WS_EX_APPWINDOW:(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE))|WS_EX_NOREDIRECTIONBITMAP,window_class,L"Codex Beer Widget",WS_POPUP,app.settings.x,app.settings.y,192,240,nullptr,nullptr,instance,&app);
    if(!window){CoUninitialize();return 1;}
    app.overlay=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED|WS_EX_TRANSPARENT,L"STATIC",L"Codex Beer Widget — click through",WS_POPUP,120,120,192,240,nullptr,nullptr,instance,nullptr);
    try {
        app.renderer=std::make_unique<beer::Renderer>(window);
        app.tray();if(!app.register_hotkey(app.settings.hotkey_modifiers,app.settings.hotkey))MessageBoxW(window,L"Сохранённая горячая клавиша занята. Выберите другую в настройках через трей.",L"Codex Beer Widget",MB_ICONWARNING);
        app.size(app.height);app.recover();app.show(app.settings.visible);
        if(std::wstring(arguments).find(L"--settings")!=std::wstring::npos)app.open_settings();
        if(!settings_error.empty())MessageBoxW(window,L"Не удалось прочитать настройки. Использованы значения по умолчанию.",L"Codex Beer Widget",MB_ICONWARNING);
        MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!app.settings_window||!IsDialogMessageW(app.settings_window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    }catch(const std::exception&){MessageBoxW(window,L"Не удалось запустить Direct2D/DirectComposition.",L"Codex Beer Widget",MB_ICONERROR);}
    DestroyWindow(app.overlay);app.renderer.reset();if(IsWindow(window))DestroyWindow(window);if(singleton)CloseHandle(singleton);CoUninitialize();return 0;
}
