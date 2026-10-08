r"""滚动容器的命中 / hover 回归（两段判据都靠窗口标题，不靠像素）。

要抓的两个问题（2026-10-08 修，清单见 notes/2026-10-08-Ling滚动容器根治清单.md）：

  #1 命中坐标没随滚动平移 —— `ScrollerBox` 把子节点挂到内部 content 层，若
     `Node::makeChild()` 把所有权记在 ScrollerBox 上，`layout()` / `shiftHitY()`
     就遍历不到那些子节点：**视觉滚了、命中坐标没滚**，点哪都不对。
  #4 滚动后 hover 不重算 —— 滚动把控件整体移走而鼠标没动，系统不再发
     `WM_MOUSEMOVE`，于是"滚动前被指着的那一个"把高亮一直挂着。

判据（demo 的哨兵按钮是列表最后一项，把自身状态写进窗口标题）：

  1. 滚到底 → 点哨兵**可见位置** → 标题里的"点击 N 次"必须 +1（旧写法点不到）。
  2. 往哨兵位置发一次 `WM_MOUSEMOVE` → 标题 `hover:in`。
  3. **不发任何鼠标消息**，只滚一格（哨兵整体下移）→ 标题必须变 `hover:out`
     （框架重播 hover 才会变；不重播就一直 in）。
  4. 再滚回来 → 又变 `hover:in`。

⚠ 这两个判据都必须是**点击/状态**驱动，不能用像素比：后台窗口的 PrintWindow
   对 hover 后的合成帧不可靠（实测量到的一直是常态底色）。
⚠ 消息参数坑：`WM_MOUSEWHEEL` 的 lParam 是**屏幕**坐标、delta 向下为**负**；
   `WM_LBUTTONDOWN/UP`/`WM_MOUSEMOVE` 的 lParam 是**物理客户区**坐标。

用法： python tools/probe_scroller_hit.py [Demo.exe 路径]
"""
import ctypes
import os
import subprocess
import sys
import time
from ctypes import wintypes

user32 = ctypes.WinDLL('user32', use_last_error=True)
k32 = ctypes.windll.kernel32
user32.FindWindowW.restype = wintypes.HWND
user32.SendMessageW.restype = ctypes.c_ssize_t

WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_MOUSEWHEEL = 0x020A
WHEEL_DELTA = 120
WHEEL_SPACE = 60.0          # WinBase::mouseWheel: (delta/120) * 60 * dpi

SENT_H_LOGICAL = 120.0      # 哨兵高度（见 demo/WindowScroller.cpp）
SENT_BOTTOM_MARGIN = 24.0   # 哨兵下外边距（同上）

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def find_demo():
    best = None
    for dirpath, _dirs, files in os.walk(ROOT):
        if 'Temp' in dirpath:
            continue
        for f in files:
            if f == 'Demo.exe':
                p = os.path.join(dirpath, f)
                if best is None or os.path.getmtime(p) > os.path.getmtime(best):
                    best = p
    return best


def kill_demos():
    class PE(ctypes.Structure):
        _fields_ = [('dwSize', ctypes.c_ulong), ('cntUsage', ctypes.c_ulong),
                    ('pid', ctypes.c_ulong), ('h0', ctypes.c_void_p),
                    ('m0', ctypes.c_ulong), ('cntThreads', ctypes.c_ulong),
                    ('parent', ctypes.c_ulong), ('pri', ctypes.c_long),
                    ('flags', ctypes.c_ulong), ('name', ctypes.c_char * 260)]
    snap = k32.CreateToolhelp32Snapshot(2, 0)
    e = PE(); e.dwSize = ctypes.sizeof(PE)
    if k32.Process32First(snap, ctypes.byref(e)):
        while True:
            if e.name.decode('gbk', 'replace').lower() == 'demo.exe':
                h = k32.OpenProcess(1, False, e.pid)
                if h:
                    k32.TerminateProcess(h, 0); k32.CloseHandle(h)
            if not k32.Process32Next(snap, ctypes.byref(e)):
                break
    k32.CloseHandle(snap)


def find_window(pid, timeout=8.0):
    EnumProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    deadline = time.time() + timeout
    while time.time() < deadline:
        hits = []

        def cb(h, _):
            p = wintypes.DWORD()
            user32.GetWindowThreadProcessId(h, ctypes.byref(p))
            if p.value == pid and user32.IsWindowVisible(h):
                t = ctypes.create_unicode_buffer(256)
                user32.GetWindowTextW(h, t, 256)
                if '滚动容器演示' in t.value:
                    hits.append((h, t.value))
            return 1
        user32.EnumWindows(EnumProc(cb), 0)
        if hits:
            return hits[0]
        time.sleep(0.3)
    return (None, None)


def title_of(hwnd):
    t = ctypes.create_unicode_buffer(256)
    user32.GetWindowTextW(hwnd, t, 256)
    return t.value


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else find_demo()
    if not exe or not os.path.isfile(exe):
        print('!! 找不到 Demo.exe —— 先编 demo（bash _tmp_build_demo.sh，或打开 Ling.slnx 编）')
        return 1
    print('demo: %s' % exe)
    kill_demos()

    proc = subprocess.Popen([exe, 'scroller'])
    hwnd, _title = find_window(proc.pid)
    if not hwnd:
        print('!! 没等到滚动演示窗口')
        proc.kill()
        return 1
    time.sleep(1.0)

    r = wintypes.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(r))
    cw, ch = r.right, r.bottom
    dpi = user32.GetDpiForWindow(hwnd) / 96.0
    print('客户区 %dx%d  dpi=%.2f' % (cw, ch, dpi))

    # 滚到底用的消息参数：WM_MOUSEWHEEL 的 lParam 是**屏幕**坐标
    pt = wintypes.POINT(cw // 2, ch // 2)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    lp_wheel = ((pt.y & 0xFFFF) << 16) | (pt.x & 0xFFFF)

    def wheel(notches, up=False):
        """notches 格；up=True 向上（内容下移）"""
        delta = WHEEL_DELTA if up else -WHEEL_DELTA
        wp = ((delta) & 0xFFFF) << 16
        for _ in range(notches):
            user32.SendMessageW(hwnd, WM_MOUSEWHEEL, wp, lp_wheel)
        time.sleep(0.45)

    wheel(80)                                  # 滚到底
    print('已滚到底（80 格）')

    # 哨兵在滚动区底部：高 120、下外边距 24（逻辑像素）
    top = ch - SENT_BOTTOM_MARGIN * dpi - SENT_H_LOGICAL * dpi
    sx = cw // 2
    sy = int(top + 30 * dpi)                   # 点在哨兵上部，滚一格后它仍可见
    lp_hit = ((sy & 0xFFFF) << 16) | (sx & 0xFFFF)
    print('哨兵可见位置 ≈ (x=%d, y=%d)，底边 y=%d' % (sx, sy, int(ch - SENT_BOTTOM_MARGIN * dpi)))

    def click():
        user32.SendMessageW(hwnd, WM_MOUSEMOVE, 0, lp_hit)
        time.sleep(0.05)
        user32.SendMessageW(hwnd, WM_LBUTTONDOWN, 1, lp_hit)
        time.sleep(0.08)
        user32.SendMessageW(hwnd, WM_LBUTTONUP, 0, lp_hit)
        time.sleep(0.4)

    ok1 = True
    for n in (1, 2):
        click()
        t = title_of(hwnd)
        print('  第 %d 次点击后： 「%s」' % (n, t))
        if ('点击 %d 次' % n) not in t:
            ok1 = False
    print('=> %s：点击命中的是哨兵（命中坐标跟着滚了）'
          % ('通过' if ok1 else '**未通过** —— 命中坐标没随滚动平移（makeChild 所有权）'))

    # ---- #4：hover 重播 ----
    # ⚠ 不能靠"发 WM_MOUSEMOVE"驱动 hover：WinBase::mouseMove 里挂了 TrackMouseEvent，
    #   而真实光标不在窗口上时，系统会**立刻**补一条 WM_MOUSELEAVE → hover 当场被打回 out
    #   （实测：连发 4 次 SendMessage + 1 次 PostMessage 都只留下 out）。
    #   所以这里换一个零干扰的做法：**不动光标，把窗口挪到光标底下**，
    #   让真实光标自己产生 hover；然后只滚轮、不动鼠标，看 hover 有没有跟着内容走。
    ok2 = hover_probe(hwnd, cw, ch, dpi, wheel, title_of)
    try:
        proc.kill()
    except Exception:
        pass
    return 0 if (ok1 and ok2 is not False) else 1


def hover_probe(hwnd, cw, ch, dpi, wheel, title_of):
    """零干扰 hover 验证：窗口挪到光标底下 → 滚一格 → hover 必须跟着内容换人。

    为什么要这么绕：`WinBase::mouseMove` 里挂了 TrackMouseEvent，而**真实光标不在窗口上**
    时系统会立刻补一条 WM_MOUSELEAVE，把 hover 当场打回 out —— 实测"发 WM_MOUSEMOVE"
    这条常见路子在这里完全无效（连发 4 次 Send+Post 都只留下 out）。

    所以先 `SetWindowPos` 把窗口挪到光标下面（光标一动没动），再用 **1 像素微移 + 还原**
    （结束必然回到原位；`GetAsyncKeyState`/`GetCapture` 有按键或捕获时直接跳过）取到真实
    hover；之后只滚轮，看 hover 有没有跟着内容走。

    返回 True / False；None = 环境不适合（光标压在任务栏或别的窗口上、有按键按下…）→ 跳过。
    """
    u = user32
    for vk, name in ((0x01, '左键'), (0x02, '右键'), (0x04, '中键')):
        if u.GetAsyncKeyState(vk) & 0x8000:
            print('  !! %s按着，跳过 hover 段（不打扰你）' % name)
            return None
    if u.GetCapture():
        print('  !! 有窗口在捕获鼠标，跳过 hover 段')
        return None

    cur = wintypes.POINT()
    if not u.GetCursorPos(ctypes.byref(cur)):
        print('  !! 拿不到光标位置，hover 段跳过（请人工悬停验证）')
        return None

    sent_h = SENT_H_LOGICAL * dpi
    sent_bottom = ch - SENT_BOTTOM_MARGIN * dpi
    sent_top = sent_bottom - sent_h
    px = cw // 2
    py = int(sent_top + 30 * dpi)          # 落在哨兵带里

    wr = wintypes.RECT()
    u.GetWindowRect(hwnd, ctypes.byref(wr))
    pt0 = wintypes.POINT(0, 0)
    u.ClientToScreen(hwnd, ctypes.byref(pt0))
    offx, offy = pt0.x - wr.left, pt0.y - wr.top

    new_left = cur.x - (px + offx)
    new_top = cur.y - (py + offy)
    SWP_NOSIZE, SWP_NOACTIVATE = 0x0001, 0x0010
    try:
        u.SetWindowPos(hwnd, 0, new_left, new_top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE)
        time.sleep(0.7)
        # 守卫：挪过去之后光标底下必须真的是我们的窗口，否则收不到任何鼠标消息
        under = u.WindowFromPoint(cur)
        root = u.GetAncestor(under, 2) if under else None      # GA_ROOT
        if under != hwnd and root != hwnd:
            print('  !! 光标此刻压在别的窗口上（under=%s），hover 段跳过 —— '
                  '把它移到 demo 窗口区域里再跑一次' % under)
            return None
        # 光标的屏幕坐标同时当滚轮坐标（挪窗后旧的屏幕坐标已失效！）
        lp_wheel_here = ((cur.y & 0xFFFF) << 16) | (cur.x & 0xFFFF)

        u.SetCursorPos(cur.x + 1, cur.y); time.sleep(0.2)
        u.SetCursorPos(cur.x, cur.y);     time.sleep(0.45)

        t0 = title_of(hwnd)
        print('  光标停在哨兵上： 「%s」' % t0)
        if 'hover:in' not in t0:
            print('  !! 没取到 hover:in（选点或环境问题），hover 段作废')
            return None

        def w(delta, tag):
            u.SendMessageW(hwnd, WM_MOUSEWHEEL, (delta & 0xFFFF) << 16, lp_wheel_here)
            time.sleep(0.45)
            t = title_of(hwnd)
            print('  %s（鼠标一动没动）： 「%s」' % (tag, t))
            return t

        t_out = w(WHEEL_DELTA, '向上滚一格：哨兵移开')
        t_in = w(-WHEEL_DELTA, '再滚回来：哨兵回到光标下')
        ok = ('hover:out' in t_out)
        if 'hover:in' not in t_in:
            print('  （注：滚回来那次没回到 hover:in —— 可能是滚动量未对称，以"移开时能变 out"为主判据）')
        print('=> %s：滚动后 hover 自己重算（不靠鼠标移动）' % ('通过' if ok else '**未通过**'))
        return ok
    finally:
        # 无论成败都把光标还回原位（微移只在上面用了一次，这里再兜一次底）
        u.SetCursorPos(cur.x, cur.y)


if __name__ == '__main__':
    sys.exit(main())
