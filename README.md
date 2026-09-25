# Ling

一个使用现代 C++ 开发的自绘 GUI 框架，仅为 Windows 桌面应用服务。

# 特性

- 渲染分两层：窗口背景/边框/圆角/滚动条交给 [Windows.UI.Composition](https://learn.microsoft.com/en-us/uwp/api/windows.ui.composition)
  （合成器线程，不占 UI 线程）；文本/图像/自绘内容用 Direct2D 画到 `CompositionDrawingSurface`。
- 使用 [Yoga](https://github.com/react/yoga) 作为布局引擎，文本/图像的"尺寸由内容决定"
  走 yoga 的 measureFunc 回报 DWrite metric。
- 布局与样式 API 全部收**逻辑像素**，内部按窗口 dpi 换算，DPI 变化时递归重排。
  ⚠ 注意 `WinBase::w/h` 成员存的是**物理像素**（见 `include/WinBase.h` 的注释）。
- Release 编译产物 < 1M，可以编译为单文件 exe。
- 静态库形态：`Ling.lib` + `include/` 头文件，带版本发布（见下）。

## 发布与版本管理

Ling 以 GitHub Release 分发预编译静态库（x64 / Release）：

- 打包：`bash pack_release.sh`（先打 tag，产物 `dist/ling-<版本>-x64.zip`，
  含 `include/` + `yoga/` 头文件 + `x64/Release/` 的 lib 与 pdb，布局镜像源码根）。
- 消费：使用者（如 ZPin）用 `ling_pkg.sh install <版本>` 安装，版本锁定在自己的 lock 文件里。

## 应用此框架的项目

- [ZPin](https://github.com/James-ctrl-Doyle/ZPin)（Windows 截图工具，fork 自 xland/ScreenCapture）
- [ScreenCapture](https://github.com/xland/ScreenCapture)（上游项目）

## 已知边界

- 事件模型是窗口级广播 + 控件自判命中（`isPosIn`），没有树级路由与 z 序仲裁：
  重叠控件会同时收到消息，"谁先处理"由使用方约定。滚动容器的命中偏移框架已处理
  （2026-09 起），但"点在重叠区该归谁"仍需使用方自己排布避免。
- 没有 Tab 焦点顺序与 UIA（无障碍）支持。
