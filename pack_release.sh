#!/usr/bin/env bash
# 打包 Ling 发布产物 —— "pip 包"流程里的打 wheel 环节。
#
# 用法:  bash pack_release.sh [版本号]
#   版本号默认从 git describe 取（有 tag 用 tag，如 v1.0.0；没有 tag 用 短hash）。
#   发正式包前先打 tag：git tag -a v1.0.0 -m "..." && git push origin v1.0.0
#
# 产物: dist/ling-<版本>-x64.zip，布局镜像 Ling 源码根：
#   include/            公开头（ZPin 里 #include <include/Ling.h>）
#   yoga/               只含 .h（Ling 公开头会 #include <yoga/Yoga.h>）
#   x64/Release/        Ling.lib / Ling.pdb / yoga.lib / yoga.pdb
#   VERSION.txt         版本、commit、构建日期、工具链
#
# 布局能镜像源码根，ZPin 的 vcxproj 才能只把包根 prepend 进 include/lib 搜索路径。
# ⚠ 同 rebuild_all.sh：要在沙箱外执行（要真实调用编译器）。
export PATH="/usr/bin:/bin:/c/Windows/System32:$PATH"

unset http_proxy https_proxy all_proxy HTTP_PROXY HTTPS_PROXY ALL_PROXY

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || exit 1

VER="$(git describe --tags --abbrev=0 2>/dev/null)"
if [ -z "$VER" ]; then
    VER="$(git rev-parse --short HEAD)"
fi
if [ -n "$1" ]; then VER="$1"; fi
COMMIT="$(git rev-parse HEAD)"
DATE="$(date +%F' '%T)"

winpath() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s' "$1" | sed -e 's|^/\([a-zA-Z]\)/|\1:\\|' -e 's|/|\\|g'
    fi
}

find_msbuild() {
    local vswhere='/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
    if [ -x "$vswhere" ]; then
        local p
        p="$("$vswhere" -latest -requires Microsoft.Component.MSBuild \
             -find 'MSBuild/**/Bin/MSBuild.exe' 2>/dev/null | head -1)"
        if [ -n "$p" ]; then printf '%s' "$(winpath "$p")"; return; fi
    fi
    printf '%s' 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
}
MSB="$(find_msbuild)"
MSB_DIR="$(winpath "$ROOT")\\"

echo "MSBuild : $MSB"
echo "版本    : $VER  ($COMMIT)"
echo ""

echo "########## 1/3  yoga.lib (Rebuild, Release x64) ##########"
"$MSB" "${MSB_DIR}yoga\\yoga.vcxproj" -p:Configuration=Release -p:Platform=x64 \
    -p:SolutionDir="$MSB_DIR" -restore:false -t:Rebuild -m -v:m > /tmp/yoga_pack.log 2>&1
YOGA_RC=$?
grep -E "error [A-Z]+[0-9]+" /tmp/yoga_pack.log | head -5
echo "yoga exit=$YOGA_RC"
[ $YOGA_RC -eq 0 ] || { echo "!! yoga 编译失败，日志 /tmp/yoga_pack.log"; exit 1; }

echo "########## 2/3  Ling.lib (Rebuild, Release x64) ##########"
"$MSB" "${MSB_DIR}Ling.vcxproj" -p:Configuration=Release -p:Platform=x64 \
    -p:SolutionDir="$MSB_DIR" -restore:false -t:Rebuild -m -v:m > /tmp/ling_pack.log 2>&1
LING_RC=$?
grep -E "error [A-Z]+[0-9]+" /tmp/ling_pack.log | head -5
echo "Ling exit=$LING_RC"
[ $LING_RC -eq 0 ] || { echo "!! Ling 编译失败，日志 /tmp/ling_pack.log"; exit 1; }

echo "########## 3/3  收集产物并打包 ##########"
PKG_NAME="ling-${VER}-x64"
STAGE="dist/${PKG_NAME}"
rm -rf "$STAGE"
mkdir -p "$STAGE/include" "$STAGE/x64/Release"

cp include/*.h "$STAGE/include/"

# yoga 只带头文件，保持目录结构（公开头 #include <yoga/Yoga.h> 及各子头）
(cd yoga && find . -name '*.h' -print0) | while IFS= read -r -d '' f; do
    mkdir -p "$STAGE/yoga/$(dirname "$f")"
    cp "yoga/$f" "$STAGE/yoga/$f"
done

for f in Ling.lib Ling.pdb yoga.lib yoga.pdb; do
    [ -f "x64/Release/$f" ] || { echo "!! 缺产物 x64/Release/$f"; exit 1; }
    cp "x64/Release/$f" "$STAGE/x64/Release/"
done

VSVER=$('/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' \
        -latest -property installationVersion 2>/dev/null | tr -d '\r\n')
cat > "$STAGE/VERSION.txt" <<EOF
name      : Ling
version   : $VER
commit    : $COMMIT
built     : $DATE
config    : Release / x64 (静态库)
toolchain : MSVC (Visual Studio $VSVER), MSBuild: $MSB
runtime   : 与 ZPin 约定一致；Debug 配置请勿链接本包（源码模式编 Ling）
usage     : 解压后把包根加入 include 搜索路径、x64/Release 加入 lib 搜索路径
EOF

mkdir -p dist
ZIP="dist/${PKG_NAME}.zip"
rm -f "$ZIP"
/c/Windows/System32/tar.exe -a -c -f "$(winpath "$PWD/$ZIP")" -C "$(winpath "$PWD/dist")" "$PKG_NAME"
[ -f "$ZIP" ] || { echo "!! 打 zip 失败"; exit 1; }

echo ""
ls -l "$ZIP"
echo "完成。发布：git tag 后用 GitHub API 创建 Release 并上传 $ZIP"
