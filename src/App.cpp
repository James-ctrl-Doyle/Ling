#include "pch.h"
#include <Windows.h>
#include <winuser.h>
#include "../include/Util.h"
#include "../include/D2D.h"
#include "../include/App.h"
#include "../yoga/YGConfig.h"

namespace Ling {

    /// <summary>
    /// 生成"本 exe 专属"的 appID。
    ///
    /// ⚠ 绝对不能用 COMPILE_TIME_RAND_STR：那个宏的种子是 __TIME__ / __COUNTER__ /
    ///   __LINE__，全部来自**编译这个 .cpp 的时刻**。而 App() 构造函数是编进
    ///   预编译静态库（Ling.lib）的 —— 一旦打库，取值就固化了，跟链接它的程序是谁
    ///   **完全无关**。任何两个链接同一份 Ling.lib 的程序拿到的 appID 一模一样，
    ///   于是共用 FindWindow(L"STATIC", appID) 那一个槽位，后启动者必被误判成
    ///   "第二实例"直接 ExitProcess。ZPin 与 ZDock 互相冲突就是这个原因。
    ///
    /// 换成按 **exe 自身的完整路径**做哈希：每个程序天然不同，
    /// 同一个程序重编、换目录也稳定。
    /// </summary>
    static std::wstring makeAppID()
    {
        wchar_t buf[MAX_PATH * 2]{};
        GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
        std::wstring path{ buf };
        // 路径大小写不敏感，统一小写再哈希，避免同目录不同写法算成两个 ID
        for (auto& c : path) c = static_cast<wchar_t>(::towlower(c));

        // FNV-1a 64 位
        uint64_t h = 1469598103934665603ULL;
        for (wchar_t c : path) {
            h ^= static_cast<uint64_t>(c);
            h *= 1099511628211ULL;
        }

        // 再把"同一个 exe 的多个副本放在不同目录"也区分开 —— 上面的路径哈希已经做到了，
        // 这里只负责格式化成一个窗口标题友好的短串
        return std::format(L"Ling_{:012X}", h & 0xFFFFFFFFFFFFULL);
    }

    static std::unique_ptr<App> app;

    App::App() :dq{ winrt::Windows::System::DispatcherQueue::GetForCurrentThread() }, appID{ makeAppID()}
    {
        SetCurrentProcessExplicitAppUserModelID(appID.data());
    }

    App::~App()
    {
        disposeTray();
    }

    App* App::get()
    {
        return app.get();
    }

    void App::dispose()
    {
        app.reset();
    }

    void App::quit(int code)
    {
        onBeforeQuit();
        PostQuitMessage(code);
    }

    void App::exit(int code)
    {
        ExitProcess(code);
    }

    bool App::refuseSecondInstance()
    {
        auto hwnd = FindWindow(L"STATIC", appID.data());
        if (hwnd) {
            PostMessage(hwnd, WM_APP + 1, 0, 0);
            App::exit(0);
            return true;
        }
        initMsgWin();
        return false;
    }

    bool App::regHotKey(const std::wstring& keyStr, const UINT msgId)
    {
        std::wstring lowerName = keyStr;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
        auto arr = Util::splitStr(lowerName, L'+');
        UINT modifiers = 0;
        UINT keyCode{ 0 };
        for (auto& key : arr)
        {
            if (key == L"ctrl") {
                modifiers |= MOD_CONTROL;
            }
            else if (key == L"alt") {
                modifiers |= MOD_ALT;
            }
            else if (key == L"shift") {
                modifiers |= MOD_SHIFT;
            }
            else if (key == L"win" || key == L"lwin" || key == L"rwin") {
                modifiers |= MOD_WIN;
            }
            else {
                keyCode = Util::strToKey(key);
            }
        }
        //键名认不出来（配置文件被手工改坏了之类）：注册不了，交给调用方处理
        if (keyCode == 0) return false;
        initMsgWin();
        // 失败的常见原因是 ERROR_HOTKEY_ALREADY_REGISTERED：这个组合已经被别的程序占了
        return RegisterHotKey(msgHwnd, WM_APP + msgId, modifiers, keyCode) != FALSE;
    }

    void App::unRegHotKey(const UINT msgId)
    {
        UnregisterHotKey(msgHwnd, WM_APP + msgId);
    }

    void App::initTray(const UINT msgId, const std::wstring& tip)
    {
        if (tray.get()) return;
        initMsgWin();
        tray = std::make_unique<NOTIFYICONDATA>();
        ZeroMemory(tray.get(), sizeof(NOTIFYICONDATA));
        tray->cbSize = sizeof(NOTIFYICONDATA);
        tray->hWnd = msgHwnd; 
        tray->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        trayMsgId = WM_APP + msgId;
        tray->uCallbackMessage = trayMsgId;
        tray->hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(1));
        if (!tip.empty()) {
            wcscpy_s(tray->szTip, tip.data());
        }        
        Shell_NotifyIcon(NIM_ADD, tray.get());
    }

    void App::setTrayIcon(const int iconResourceId)
    {
        if (!tray) return;
        auto hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(iconResourceId));
        if (!hIcon) return;   // 资源不存在：保持原图标，别把托盘弄成空白
        tray->hIcon = hIcon;
        tray->uFlags = NIF_ICON;
        Shell_NotifyIcon(NIM_MODIFY, tray.get());
    }

    void App::disposeTray()
    {
        if (tray.get()) {
            Shell_NotifyIcon(NIM_DELETE, tray.get());
            tray.reset();
        }
    }

    void App::initArgs()
    {
        LPWSTR* argv;
        int argc;
        LPWSTR cmdLine = GetCommandLine();
        argv = CommandLineToArgvW(cmdLine, &argc);
        for (int i = 1; i < argc; ++i) {
            std::wstring arg{ argv[i] };
            auto index = arg.find(L"=");
            if (index != std::wstring::npos) {
                args[arg.substr(0, index)] = arg.substr(index + 1);
            }
            else {
                args.insert({ arg,L"true" });
            }
        }
        LocalFree(argv);
    }

    UINT App::popupMenu(HMENU menu)
    {
        POINT pt;
        GetCursorPos(&pt);
        // 参考 MS KB 135788：托盘弹菜单前必须把 owner 拉到前台, 菜单才能收到"外部点击"
        // 和"失焦"事件从而自动关闭；否则会一直挂着。菜单结束后再补一个 WM_NULL, 兜住
        // 部分 shell 版本的兼容问题。
        SetForegroundWindow(msgHwnd);
        UINT selectedCmd = TrackPopupMenuEx(menu,TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,pt.x,pt.y,msgHwnd,nullptr);
        PostMessage(msgHwnd, WM_NULL, 0, 0);
        DestroyMenu(menu);
        return selectedCmd;
    }

    void App::init()
    {
        App::initDispatcherQueueCtrl();
        auto ptr = new App();
        app.reset(ptr);
    }

    void App::initMsgWin()
    {
        if (msgHwnd) return;
        msgHwnd = CreateWindow(L"STATIC", appID.data(), 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetInstanceModule(NULL), NULL);
        SetWindowLongPtr(msgHwnd, GWLP_WNDPROC, (LONG_PTR)App::winProc);
        SetWindowLongPtr(msgHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    LRESULT App::winProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto self = reinterpret_cast<App*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (!self) {
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
        else if (msg == WM_APP + 1)
        {
            self->onSecondInstance();
            return 0;
        }
        else if (msg == WM_HOTKEY) {
            self->onHotKey((UINT)wParam - WM_APP);
            return 0;
        }
        else if (msg == self->trayMsgId)
        {
            if (lParam == WM_LBUTTONDOWN) {
                self->onTrayMouseEvent(true,false);
            }
            else if (lParam == WM_RBUTTONDOWN)
            {
                self->onTrayMouseEvent(true, true);
            }
            else if (lParam == WM_LBUTTONUP)
            {
                self->onTrayMouseEvent(false, false);
            }
            else if (lParam == WM_RBUTTONUP)
            {
                self->onTrayMouseEvent(false, true);
            }
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void App::initDispatcherQueueCtrl()
    {
        DispatcherQueueOptions options{ sizeof(DispatcherQueueOptions), DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE };
        static winrt::Windows::System::DispatcherQueueController controller{ nullptr };
        auto hr = CreateDispatcherQueueController(options, reinterpret_cast<ABI::Windows::System::IDispatcherQueueController**>(winrt::put_abi(controller)));
        if (FAILED(hr))
        {
            MessageBox(NULL, L"无法创建DispatcherQueueController", L"系统提示", MB_OK);
            ExitProcess(-1);
        }
    }
}
