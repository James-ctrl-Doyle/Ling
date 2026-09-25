#include "pch.h"
#include "../include/Button.h"
#include "../include/WinBase.h"
#include "../include/Text.h"

namespace Ling {

	Button::Button(WinBase* win) :Node(win)
	{
		setJustifyContent(Ling::Justify::Center);
		setAlignItems(Ling::Align::Center);
		text = makeChild<Text>();
		auto weakThis = getWeakThis();
		moveTok = win->onMouseMove.add([this, weakThis](POINT pos) {
			if (!weakThis.lock()) return;
			onMove(pos);
		});
		downTok = win->onMouseDown.add([this, weakThis](POINT pos, bool isRight) {
			if (!weakThis.lock()) return;
			onDown(pos, isRight);
		});
		upTok = win->onMouseUp.add([this, weakThis](POINT pos, bool isRight) {
			if (!weakThis.lock()) return;
			onUp(pos, isRight);
		});
	}

	Button::~Button()
	{
		// 按着的时候按钮被销毁（如下拉列表项点完即拆）：把捕获还回去，
		// 否则窗口会一直攥着鼠标捕获，别的控件收不到 mouse move
		if (pressed && GetCapture() == win->hwnd) ReleaseCapture();
		win->onMouseMove.remove(moveTok);
		win->onMouseDown.remove(downTok);
		win->onMouseUp.remove(upTok);
	}
	void Button::setText(const std::wstring& s)
	{
		text->setText(s);
	}
	std::wstring Button::getText()
	{
		return text->getText();
	}
	void Button::setFontSize(float val)
	{
		text->setFontSize(val);
	}
	void Button::setFontFamily(const std::wstring& val)
	{
		text->setFontFamily(val);
	}
	void Button::setMaxTextWidth(float val)
	{
		text->setMaxWidth(val);
	}
	void Button::setColor(Color color)
	{
		text->setColor(color);
		this->color = color;
	}
	void Button::setBg(const Color& color)
	{
		bgColor = color;
		normalBrush = win->compositor.CreateColorBrush(color.getUIColor());
		if (!isHover) visual.Brush(normalBrush);
	}
	void Button::setBorderColor(const Color& color)
	{
		borderColorNormal = color;
		// 正常态直接落到底层；hover 态则保留 hoverBorderColor 不动。
		if (!isHover) Node::setBorderColor(color);
	}
	void Button::setHoverColor(Color color)
	{
		hoverColor = color;
	}
	void Button::setHoverBg(Color color)
	{
		hoverBg = color;
		hoverBrush = win->compositor.CreateColorBrush(color.getUIColor());
		if (isHover) visual.Brush(hoverBrush);
	}
	void Button::setHoverBorderColor(Color color)
	{
		hoverBorderColor = color;
		hasHoverBorderColor = true;
		if (isHover) Node::setBorderColor(color);
	}
	void Button::onMove(POINT pos)
	{
		if (!enabled) return;
		auto hoverFlag = isPosIn(pos);
		if (isHover == hoverFlag) return;
		isHover = hoverFlag;
		if (isHover) {
			visual.Brush(hoverBrush);
			text->setColor(hoverColor);
			if (hasHoverBorderColor) Node::setBorderColor(hoverBorderColor);
			onEnter(this);
		}
		else {
			visual.Brush(normalBrush);
			text->setColor(color);
			if (hasHoverBorderColor) Node::setBorderColor(borderColorNormal);
			onLeave(this);
		}
	}
	void Button::onDown(POINT pos, bool isRight)
	{
		// 按下只进入按压态并捕获鼠标，不算点击 —— 点击与否等抬起时再判。
		// 这样"按住拖出去松手"不会误触发（Windows 标准按钮语义）；
		// 捕获后即使指针移出窗口，WM_LBUTTONUP 也会送到本窗口，保证能收到抬起。
		if (isRight || !enabled || !isPosIn(pos)) return;
		pressed = true;
		SetCapture(win->hwnd);
	}
	void Button::onUp(POINT pos, bool isRight)
	{
		if (isRight || !pressed) return;
		pressed = false;
		if (GetCapture() == win->hwnd) ReleaseCapture();
		// 抬起时指针还在按钮内（含移出又移回）才算点击；禁用态不触发
		if (enabled && isPosIn(pos)) {
			onClick(this);
		}
	}
	void Button::setEnabled(bool val)
	{
		if (enabled == val) return;
		enabled = val;
		if (!val) {
			// 按着的时候被禁用：把捕获还回去，按压态清掉
			pressed = false;
			if (GetCapture() == win->hwnd) ReleaseCapture();
			// 复位 hover 外观再整体压暗，避免停在 hover 色上
			isHover = false;
			visual.Brush(normalBrush);
			text->setColor(color);
			if (hasHoverBorderColor) Node::setBorderColor(borderColorNormal);
			visual.Opacity(0.35f);
		}
		else {
			visual.Opacity(1.f);
		}
	}
}
