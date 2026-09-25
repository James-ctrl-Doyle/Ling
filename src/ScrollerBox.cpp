#include "pch.h"
#include "../include/ScrollerBox.h"
#include "../include/WinBase.h"

namespace Ling {

	// 逻辑像素常量
	constexpr float sliderW{ 8.f }, sliderMinH{ 22.f };

	ScrollerBox::ScrollerBox(WinBase* win) : Node(win)
	{
		YGNodeStyleSetOverflow(node, YGOverflowScroll);   // 内容溢出走滚动，不参与父级 flex-basis
		YGNodeStyleSetMinHeight(node, 0.f);               // 关键：解除 flex 项目的 min-content 下限
		YGNodeStyleSetFlexShrink(node, 1.f);              // 允许被父级压缩到剩余空间

		colorVisibleScroller = win->compositor.CreateColorBrush(Color(0x88888822).getUIColor());
		colorHoverScroller = win->compositor.CreateColorBrush(Color(0x88888833).getUIColor());
		colorVisibleThumb = win->compositor.CreateColorBrush(Color(0x88888866).getUIColor());
		colorHoverThumb = win->compositor.CreateColorBrush(Color(0x88888888).getUIColor());
		colorTransparent = win->compositor.CreateColorBrush(Color(0x00000000).getUIColor());
		visual.Clip(win->compositor.CreateInsetClip());

		content = new Node(win);
		content->parent = this;
		visual.Children().InsertAtTop(content->visual);
		YGNodeInsertChild(this->node, content->node, YGNodeGetChildCount(this->node));
		auto safePtr = std::unique_ptr<Node>(content);
		children.push_back(std::move(safePtr));

		visualScroller = win->compositor.CreateSpriteVisual();
		visual.Children().InsertAtTop(visualScroller);
		visualScroller.IsVisible(false);

		visualThumb = win->compositor.CreateSpriteVisual();
		visualScroller.Children().InsertAtTop(visualThumb);


		auto weakThis = getWeakThis();
		wheelTok = win->onMouseWheel.add([this, weakThis](POINT pos, float space) { if (!weakThis.lock()) return; onWheel(pos, space); });
		moveTok  = win->onMouseMove .add([this, weakThis](POINT pos)               { if (!weakThis.lock()) return; onMove(pos); });
		upTok    = win->onMouseUp   .add([this, weakThis](POINT pos, bool isRight) { if (!weakThis.lock()) return; onUp(pos, isRight); });
		downTok  = win->onMouseDown .add([this, weakThis](POINT pos, bool isRight) { if (!weakThis.lock()) return; onDown(pos, isRight); });
	}

	ScrollerBox::~ScrollerBox()
	{
		win->onMouseWheel.remove(wheelTok);
		win->onMouseMove .remove(moveTok);
		win->onMouseUp   .remove(upTok);
		win->onMouseDown .remove(downTok);
	}

	void ScrollerBox::onWheel(POINT pos, float space)
	{
		if (!visualScroller.IsVisible()) return;
		if (!isPosIn(pos)) return;
		setScroll(scrollY - space);
	}

	void ScrollerBox::onDown(POINT pos, bool isRight)
	{
		if (!visual.IsVisible()) return;
		if (isRight) return;
		// 滚动条没显示时右侧没有可拖的东西，别让"贴着右边缘按下"凭空启动拖动
		if (!visualScroller.IsVisible()) return;
		auto sbW{ sliderW * win->dpi };
		// 只在点击滚动条条形区域内才启动拖动
		if (pos.y >= y && pos.y <= y + h && pos.x >= x + w - sbW && pos.x <= x + w) {
			SetCapture(win->hwnd);
			scrollerDragging = true;
			dragStartMouseY = (float)pos.y;
			dragStartScrollY = scrollY;
		}
	}

	void ScrollerBox::onUp(POINT pos, bool isRight)
	{
		if (scrollerDragging) {
			ReleaseCapture();
			scrollerDragging = false;
		}
	}

	void ScrollerBox::onMove(POINT pos)
	{
		if (!visual.IsVisible()) return;
		if (!scrollerDragging && !isPosIn(pos)) {
			visualScroller.Brush(colorTransparent);
			visualThumb.Brush(colorTransparent);
			return;
		}
		auto sbW{ sliderW * win->dpi };
		if (scrollerDragging) {
			float maxScroll = std::max(0.f, content->h - h);
			float minH = sliderMinH * win->dpi;
			float thumbH = std::max(minH, h * h / content->h);
			float trackFree = h - thumbH;
			if (trackFree <= 0) return;
			float ratio = (pos.y - dragStartMouseY) / trackFree;
			setScroll(dragStartScrollY + ratio * maxScroll);
		}
		else {
			if (pos.x < x + w - sbW) {
				visualScroller.Brush(colorVisibleScroller);
				visualThumb.Brush(colorVisibleThumb);
			}
			else {
				visualScroller.Brush(colorHoverScroller);
				visualThumb.Brush(colorHoverThumb);
			}
		}
	}

	void ScrollerBox::setScroll(float y)
	{
		float maxScroll = std::max(0.f, content->h - h);
		y = std::clamp(y, 0.f, maxScroll);
		// 关键：偏移 snap 到整像素。scrollY 带小数会让 content 及其所有子节点
		// 落到分数像素位置，ClearType 文本在滚动过程中会周期性发糊。
		// 命中测试用的也是这个整数 scrollY —— 保持"视觉/逻辑"一致。
		float snapped = std::round(y);
		float delta = snapped - scrollY;
		if (delta != 0.f) {
			scrollY = snapped;
			// 命中坐标跟着平移：content 子树的绝对 y 已含 -scrollY（scrollShiftY），
			// 子节点的 isPosIn 因此不再需要使用方手动 +getScrollY()
			content->shiftHitY(-delta);
		}
		// 不变量：scrollShiftY 恒等于 -scrollY。layout 会把它再累加进 content->y，
		// 两边必须同步，否则一次滚轮 + 一次重排就会把偏移算重。
		content->scrollShiftY = -scrollY;
		// Node::layout 会把 content->visual.Offset 重置回布局位（未滚动），
		// 所以这里无条件重放一次视觉偏移，保证任何调用路径下两边一致。
		content->visual.Offset({ 0.f, -scrollY, 0.f });
		if (content->h > h) {
			float minH = sliderMinH * win->dpi;
			float thumbH = std::max(minH, h * h / content->h);
			float top = maxScroll > 0 ? scrollY * (h - thumbH) / maxScroll : 0.f;
			// thumb 也 snap 一下，避免拖动时滑块自身发糊
			visualThumb.Offset({ 0.f, std::round(top), 0.f });
			visualThumb.Size({ sliderW * win->dpi, std::round(thumbH) });
		}
	}

	float ScrollerBox::getMaxScrollY() const
	{
		return std::max(0.f, content->h - h);
	}

	void ScrollerBox::scrollTo(float y)
	{
		if (scrollY == std::round(std::clamp(y, 0.f, getMaxScrollY()))) return;
		setScroll(y);
	}

	void ScrollerBox::scrollBy(float delta)
	{
		scrollTo(scrollY + delta);
	}

	void ScrollerBox::scrollIntoView(float top, float bottom)
	{
		// top/bottom 是内容坐标（未减 scrollY）。可视窗口是 [scrollY, scrollY + h]。
		if (bottom <= top) return;
		if (top < scrollY) scrollTo(top);
		else if (bottom > scrollY + h) {
			// 区间比视口还高时优先对齐顶部，否则底部贴边
			scrollTo(bottom - top > h ? top : bottom - h);
		}
	}

	float ScrollerBox::getScrollBarWidth() const
	{
		return visualScroller && visualScroller.IsVisible() ? sliderW * win->dpi : 0.f;
	}

	bool ScrollerBox::isPosInContent(POINT pos) const
	{
		return pos.x >= x && pos.x < x + w - getScrollBarWidth()
			&& pos.y >= y && pos.y < y + h;
	}

	void ScrollerBox::onDpiChanged()
	{
		// 滑块所有尺寸都是 dpi 派生量。这里不用做任何事：
		// applyDpiChange 结束后 WinBase 会 relayout，layout() 里会调 setScroll
		// 重算 visualThumb 的 offset/size；visualScroller 的 offset/size 也在 layout 里更新。
	}

	void ScrollerBox::setChild(Node* child)
	{
		child->parent = content;
		content->visual.Children().InsertAtTop(child->visual);
		YGNodeInsertChild(content->node, child->node, YGNodeGetChildCount(content->node));
	}

	void ScrollerBox::layout()
	{
		// 内容缩到不需要滚动了（窗口变大 / 内容变少）→ 先把滚动归零：
		// setScroll 内部会把 content 子树的命中坐标 shift 回未滚状态，
		// 并重置视觉偏移。否则残留旧值会让命中测试整体偏移。
		if (content->h <= h && scrollY != 0.f) {
			setScroll(0.f);
		}
		Node::layout();
		if (content->h > h) { //有滚动条
			auto sbW{ sliderW * win->dpi };
			visualScroller.Offset({ w - sbW, 0.f, 0.f });
			visualScroller.Size({ sbW, h });
			visualScroller.IsVisible(true);
			// delta=0 也会重放 content 的视觉偏移（Node::layout 刚把它重置回未滚动位），
			// 并保持 scrollShiftY 与 scrollY 一致，命中坐标因此始终对得上视觉。
			setScroll(scrollY);
		}
		else {
			visualScroller.IsVisible(false);
		}
	}

}
