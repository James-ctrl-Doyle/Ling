#include "pch.h"
#include "WindowScroller.h"
#include "TitleBar.h"

WindowScroller::WindowScroller():Ling::WinBase()
{
    setTitle(L"滚动容器演示");
    setSize(800, 600);
    setCenter();
    onDestroy.add([this] { Ling::App::get()->quit(); });
    createNativeWindow();
}

WindowScroller::~WindowScroller()
{
}

void WindowScroller::onCreated()
{
    enableShadow();
    body->setBg(0xFFFFFFFF);
    body->setFlexDirection(Ling::FlexDirection::Column);
    titleBar = std::make_unique<TitleBar>(this);
    initScrollerBox();
    show();
}

LRESULT WindowScroller::onHitTest(const POINT pos)
{
    POINT pt = pos;
    ScreenToClient(hwnd, &pt); 
    if (!isMaximized) {
        auto result = borderHitTest(pt);
        if (result != HTCLIENT) return result;
    }
    return titleBar->hitCaption(pt);
}

void WindowScroller::initScrollerBox()
{
    // ⚠ 这里刻意保留 `scrollerBox->makeChild<...>()` 这种写法：
    //   ScrollerBox 会把子节点实际挂到内部 content 层，makeChild 必须把所有权
    //   一并记到 content 上（见 Node::makeChild 的注释）。写法错了就会出现
    //   "视觉滚了、命中坐标没滚"—— 末尾那个哨兵按钮就是用来当场抓住它的。
    scrollerBox = body->makeChild<Ling::ScrollerBox>();
    scrollerBox->setWidthPercent(100.f);
    scrollerBox->setFlexGrow(1.f);
    for (size_t i = 0; i < 100; i++)
    {
        auto text = scrollerBox->makeChild<Ling::Label>();
        text->setText(std::format(L"这是一行文本，看看能渲染多少条{}", i));        
        text->setMargin(12.f, 8.f, 12.f, 8.f);
    }

    // —— 回归哨兵（2026-10-08 加）——
    // 固定高度、可点、可高亮；滚到底点它，标题变成"点击 N 次"。
    // 判据在 tools/probe_scroller_hit.py：滚到底点一下 → 标题计数必须 +1。
    auto sentinel = scrollerBox->makeChild<Ling::Button>();
    sentinel->setText(L"哨兵按钮：滚到底点我");
    sentinel->setHeight(120.f);            // 够高，滚动一格后仍有可见带（hover 回归要在它身上量像素）
    sentinel->setWidthPercent(100.f);
    sentinel->setMargin(12.f, 12.f, 12.f, 24.f);
    sentinel->setBg(0xE8F0FEFF);
    sentinel->setHoverBg(0xD2E3FCFF);
    // 回调不需要 weakThis：按钮与窗口同生共死（按钮是窗口节点树的子孙）。
    // hover 状态也写进标题 —— 探针靠它验证"滚动后 hover 有没有自己重算"（见 tools/probe_scroller_hit.py）。
    auto refreshTitle = [this] {
        setTitle(std::format(L"滚动容器演示——哨兵 hover:{} 点击 {} 次",
                             sentinelHover ? L"in" : L"out", clickCount));
    };
    sentinel->onClick.add([this, refreshTitle](Ling::Button*) {
        clickCount++;
        refreshTitle();
    });
    sentinel->onEnter.add([this, refreshTitle](Ling::Button*) { sentinelHover = true;  refreshTitle(); });
    sentinel->onLeave.add([this, refreshTitle](Ling::Button*) { sentinelHover = false; refreshTitle(); });}