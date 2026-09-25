#pragma once
#include "Ling.h"

namespace Ling {
	class WinBase;
	class Text;   // 内部类型的前向声明
	class Button :public Node
	{
	public:
		Button(WinBase* win);
		~Button();
		void setText(const std::wstring& text);
		std::wstring getText();
		void setFontSize(float val);
		void setFontFamily(const std::wstring& val);
		// 约束按钮文字的最大宽度：按钮本身不裁文字（Text 子节点画在自己的 surface 上），
		// 想让长文案老实待在按钮里就得把宽度告诉 Text。见 Text::setMaxWidth
		void setMaxTextWidth(float val);
		void setColor(Color color);
		void setBg(const Color& color) override;
		void setBorderColor(const Color& color) override;
		void setHoverColor(Color color);
		void setHoverBg(Color color);
		void setHoverBorderColor(Color color);
		// 禁用态：忽略鼠标（hover/点击都不再响应），整体压暗到 35% 不透明度。
		// Control 基类能力（focus/isEnabled/tooltip）的最小起步，见评估报告三-4。
		void setEnabled(bool val);
		bool isEnabled() const { return enabled; }
	public:
		winrt::event<winrt::delegate<Button*>> onClick;
		winrt::event<winrt::delegate<Button*>> onEnter;
		winrt::event<winrt::delegate<Button*>> onLeave;
	private:
		void onMove(POINT pos);
		void onDown(POINT pos, bool isRight);
		void onUp(POINT pos, bool isRight);
	private:
		Text* text{ nullptr };
		winrt::event_token moveTok{}, downTok{}, upTok{};
		Color hoverColor{ 0x333333FF }, hoverBg{ 0 }, hoverBorderColor{ 0 }, color{ 0x333333FF }, borderColorNormal{ 0 };
		// 两个背景刷缓存下来，hover 切换时只做引用替换，不再每次 new。
		winrt::Windows::UI::Composition::CompositionColorBrush normalBrush{ nullptr };
		winrt::Windows::UI::Composition::CompositionColorBrush hoverBrush{ nullptr };
		bool hasHoverBorderColor{ false };
		bool isHover{ false };
		// 按压态：onDown 置位并捕获鼠标，onUp 判定是否算点击，拖出后抬起不算。
		bool pressed{ false };
		bool enabled{ true };
	};
}




