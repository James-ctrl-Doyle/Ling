#pragma once
#include "include/Ling.h"
#include "TitleBar.h"
class WindowScroller : public Ling::WinBase
{
public:
	WindowScroller();
	~WindowScroller();
private:
	void onCreated() override;
	LRESULT onHitTest(const POINT pos) override;
	void initScrollerBox();
private:
	Ling::ScrollerBox* scrollerBox{ nullptr };
	// 回归哨兵：列表末尾放一个可点按钮（100 个 Label 什么都测不出来 —— 它们不做命中测试）。
	// 手工验收：滚到底点它，标题里的次数应该 +1。若是"视觉滚了、命中没滚"，
	// 点同样的位置会落到某个 Label 上，次数纹丝不动。
	int clickCount{ 0 };
	bool sentinelHover{ false };   // 哨兵当前是否 hover（探针读标题验证）
	std::unique_ptr<TitleBar> titleBar;
};

