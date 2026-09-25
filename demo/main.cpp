#include "pch.h"
#include "include/Ling.h"
#include "WindowScroller.h"
#include "WindowImage.h"
#include "WindowCanvas.h"
#include "WindowBorderRadius.h"
#include "WindowSlider.h"
#include "WindowTextBox.h"
#include <shellapi.h>
#include <memory>

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
    Ling::init();
    Ling::App::get()->refuseSecondInstance();
    Ling::D2D::get()->addFonts({ L"icon.ttf" });

    // 命令行选要看的窗口：demo.exe scroller|image|canvas|border|slider|textbox
    // 不带参数 = textbox。（原来靠注释切换，忘了换就"怎么跑了没变化"。）
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring which = argc > 1 ? argv[1] : L"textbox";
    LocalFree(argv);

    std::unique_ptr<Ling::WinBase> win;
    if (which == L"scroller")    win = std::make_unique<WindowScroller>();
    else if (which == L"image")  win = std::make_unique<WindowImage>();
    else if (which == L"canvas") win = std::make_unique<WindowCanvas>();
    else if (which == L"border") win = std::make_unique<WindowBorderRadius>();
    else if (which == L"slider") win = std::make_unique<WindowSlider>();
    else                         win = std::make_unique<WindowTextBox>();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Ling::dispose();
    return 0;
}
