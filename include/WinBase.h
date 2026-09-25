#pragma once
#include <unordered_map>
#include <span>
#include <Windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <winrt/base.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <yoga/Yoga.h>

namespace Ling {
	class Node;
	class ScrollerBox;
	class WinBase
	{
		friend class Node;
		friend class ScrollerBox;
	public:
		WinBase();
		~WinBase();
		// 窗口类注册时加载的图标资源 ID（默认 1 —— 上游的硬编码约定）。
		// ⚠ 窗口类整个进程只注册一次，所以必须在创建**第一个窗口之前**调用。
		static void setAppIconResourceId(int id);
		// 窗口类名（默认 L"Ling"）。同图标：必须在建**第一个窗口之前**设置，
		// 且进程内唯一 —— 两个同名类第二次 RegisterClassEx 会拿到第一次的配置。
		static void setAppWindowClassName(const std::wstring& name);
		void enableShadow();
		void enableBorderResize();
		void disableWinAnimation();
		void disableBorderRadius();
		void show();
		void hide();
		void close();
		void minimize();
		void maximize();
		void restore();
		void refresh();
		void createNativeWindow(DWORD exStyle = NULL, DWORD style = WS_POPUP | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
		void setTimer(UINT elapse, UINT id);
		void killTimer(UINT id);
		void setTitle(const std::wstring& title);
		void setSize(float w, float h);
		void setPosition(int x, int y);
		void setCenter();
		virtual void layout();
		void setMinSize(float w, float h);
		std::wstring openFileDialog(std::span<const COMDLG_FILTERSPEC> filter);
		// 在 body 节点树里按 id 深度优先查找（配 Node::setId 用），找不到返回 nullptr
		Node* findById(const std::wstring& id);
	public:
		int x{ 0 }, y{ 0 };
		// ⚠ w/h 存的是**物理像素**（setSize 收逻辑值，内部乘 dpi），别当逻辑值用。
		// 最小尺寸的成员叫 minWPx/minHPx 而不是 minW/minH —— 故意的：
		// 这两个名字太通用，派生类里文件级常量一叫 minW 就被基类成员静默遮蔽
		// （ZPin 的 WinConfirm 就踩过：clamp 下界 800 压过上界 520，弹框宽度被钉死）。
		float w{ 0 }, h{ 0 }, minWPx{ 800 }, minHPx{ 600 };
		float dpi{ 1.0 };
		HWND hwnd{ nullptr };
		std::wstring title;
		bool isMouseIn{ false }, isMaximized{ false };
		winrt::Windows::UI::Composition::Compositor compositor{ nullptr };
		 
		winrt::event<winrt::delegate<POINT>> onMouseMove;
		winrt::event<winrt::delegate<POINT, bool>> onMouseDown; 
		winrt::event<winrt::delegate<POINT, bool>> onMouseUp;
		winrt::event<winrt::delegate<POINT, float>> onMouseWheel; 
		winrt::event<winrt::delegate<bool*>> onCursor; 
		winrt::event<winrt::delegate<UINT>> onKeyDown;
		winrt::event<winrt::delegate<UINT>> onKeyUp;
		// WM_CHAR：参数是字符码（已经过键盘布局与 IME 转换），文本输入取字符要用它而不是 onKeyDown。
		// 码点 > 0xFFFF 时需要使用者自己拼代理对。
		winrt::event<winrt::delegate<UINT>> onChar;
		// WM_IME_STARTCOMPOSITION：输入法开始组字。订阅者应在此把候选框摆到光标附近。
		winrt::event<winrt::delegate<>> onIME;
		// WM_SETFOCUS / WM_KILLFOCUS：窗口级焦点。控件自身的焦点态由控件维护，
		// 窗口失焦时通常要让控件一并失焦（否则光标会在没焦点的窗口里继续闪）。
		winrt::event<winrt::delegate<>> onFocus;
		winrt::event<winrt::delegate<>> onBlur;
		winrt::event<winrt::delegate<UINT>> onTimer;
		winrt::event<winrt::delegate<>> onSizeChanged;
		winrt::event<winrt::delegate<>> onDpiChanged;
		winrt::event<winrt::delegate<>> onDestroy;
		winrt::event<winrt::delegate<>> onMaximize;
		winrt::event<winrt::delegate<>> onMinimize;
		winrt::event<winrt::delegate<>> onRestore;
		winrt::event<winrt::delegate<>> onMoved;

		std::unique_ptr<Node> body;   // 必须在事件字段之后声明，先于事件字段析构
	protected:
		virtual void onCreated() {};
		virtual LRESULT onHitTest(const POINT pos) { return HTCLIENT; };
		LRESULT borderHitTest(const POINT pt);
		virtual void onMinMaxInfo(MINMAXINFO* mmi);
		virtual BOOL setCursor();
	private:
		static int appIconResourceId;
		static std::wstring appWindowClassName;
		std::wstring& getWinClsName(HINSTANCE hIns);
		static LRESULT CALLBACK winProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		void mouseMove(POINT pos);
		void mouseLeave();
		void mouseWheel(WPARAM wParam, LPARAM lParam);
		void dpiChange(WPARAM wParam, LPARAM lParam);
		void sizeChange(WPARAM wParam, LPARAM lParam);
		void posChange(POINT pos);
		int paint();
	private:
		winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget winTarget{ nullptr };
		bool isDirty{ false };
	};
}
