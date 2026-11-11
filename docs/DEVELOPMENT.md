# 开发与安装说明

## 安装

目前仓库提供源码构建。以下步骤适用于已进入项目目录的 CachyOS / Arch Linux 用户；其他发行版需要自行安装对应依赖。

1. 安装构建工具和依赖：

   ```sh
   pkexec pacman -S --needed base-devel cmake pkgconf qt6-base qt6-wayland libevdev polkit dbus
   ```

2. 构建并安装到 `/usr/local`：

   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
   cmake --build build
   pkexec /usr/bin/cmake --install build
   ```

安装后可从应用菜单启动，或运行 `gaomon-manage-panel`。如果命令不在桌面会话的搜索路径中，可运行 `/usr/local/bin/gaomon-manage-panel`。

此源码安装方式不会被 pacman 追踪。如果已经通过 pacman 安装本项目，请继续使用该包的更新方式，避免同时安装两份。仓库目前未提供可直接下载的安装包或打包脚本。

### 源码安装后的卸载

仅当按上面的步骤安装到 `/usr/local` 时，运行：

```sh
pkexec /usr/bin/rm /usr/local/bin/gaomon-manage-panel /usr/local/bin/gaomon-shortcut-helper /usr/local/share/applications/gaomon-manage-panel.desktop /usr/local/share/icons/hicolor/128x128/apps/gaomon-manage-panel.png /usr/local/share/licenses/gaomon-manage-panel/LICENSE /usr/local/share/doc/gaomon-manage-panel/NOTICE.md
```

如果安装时使用了其他前缀，先查看构建目录中最近一次安装生成的 `build/install_manifest.txt`，确认路径后再删除对应文件。以上命令不会删除用户设置。

## 构建和测试

项目使用 C++17、Qt 6 Widgets、Qt D-Bus 和 libevdev。构建时默认包含测试；`pressure-sync` 测试还需要 `dbus-run-session`。建议使用 Debug 构建运行测试，以确保基于 `assert` 的检查没有被关闭：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

也可以不安装，直接运行 `./build/gaomon-manage-panel`。`./build/gaomon-manage-panel --diagnose` 只读取并输出 Qt 平台、设备和当前映射，不会应用设置。

测试覆盖区域拖动与换算、压感曲线与虚拟笔同步、按键匹配、动作输出和设置保存。图形测试使用 offscreen 平台，压感同步测试使用隔离的模拟 D-Bus 会话；这些检查不能替代真实数位笔及目标应用中的交互测试。

## 实现概览

- 面板通过当前会话的 KWin D-Bus 接口读取和写入设备设置，因而依赖 KDE Plasma Wayland。屏幕映射和压感设置由 KWin 保存。
- 预留软键时，界面将绘图区作为 100%，写入 KWin 前换算为完整感应区坐标。软键带高度暂按官方布局图的 `15 / 356` 估算，仍需实际设备验证。
- 压感曲线使用 KWin 的两个三次贝塞尔控制点；快捷键处理器运行时，面板会将映射和压感设置同步给虚拟笔。
- 启用快捷键时，面板通过 `pkexec` 启动辅助程序。辅助程序仅打开指定 1060PRO 的笔与按键接口，创建虚拟输入设备，并在打开设备后降回用户权限。辅助程序随面板停止；它不作为常驻系统服务安装。
- 笔侧键、12 个板侧键的输入组合依据本机 1060PRO 的实际 HID 报告识别。两枚笔侧键分别产生 `B` 和 `E` 键盘事件；重新绑定时这些原始事件会被拦截。不同固件可能使用其他编码。
- 默认情况下，应用配置由 Qt 保存到 `~/.config/GaomonManagePanel/Mapping.conf` 和 `~/.config/GaomonManagePanel/Shortcuts.conf`；设置了 `XDG_CONFIG_HOME` 时位置会随之改变。按键配置在保存后保留；快捷键处理需要面板运行并手动启用。

## 资源与许可

本项目代码和文档以 [MIT 许可证](../LICENSE)发布。`assets/gaomon-manage-panel.png` 提取自 GAOMON 官方 Linux 驱动安装包，不适用项目的 MIT 许可证；其权利和再分发许可见[图标说明](../assets/NOTICE.md)。
