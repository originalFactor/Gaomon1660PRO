# 开发与安装说明

## 安装

GitHub Releases 提供 Arch Linux x86_64 的 pacman 包和二进制压缩包，也可以从源码构建。

在 Arch Linux 或 CachyOS 上，下载 Release 中的 `.pkg.tar.zst`，然后运行：

```sh
sudo pacman -U ./gaomon-manage-panel-*.pkg.tar.zst
```

二进制压缩包包含 `usr/bin` 下的面板、快捷键 helper 和权限设置工具及 `usr/share` 下的桌面资源，并非静态链接；需要兼容的 x86_64 Arch Linux 环境以及 Qt 6、libevdev、polkit 和 Qt Wayland 运行时。可将压缩包解压到临时目录后直接运行 `usr/bin/gaomon-manage-panel`，或优先使用 pacman 包安装以便管理桌面资源和依赖。

以下源码构建步骤适用于已进入项目目录的 CachyOS / Arch Linux 用户；其他发行版需要自行安装对应依赖。

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

此源码安装方式不会被 pacman 追踪。如果已经通过 pacman 安装本项目，请继续使用该包的更新方式，避免同时安装两份。

### 源码安装后的卸载

卸载前，先在主面板关闭“登录后自动启动面板”和“免密启动快捷键”。若面板无法打开，可在当前用户的终端撤销免密规则：

```sh
pkexec /usr/local/bin/gaomon-permission-setup --disable
```

该命令只撤销调用用户的规则；其他曾启用免密的用户也应各自撤销。删除用户配置文件不会撤销系统中的免密规则。

仅当按上面的步骤安装到 `/usr/local` 时，运行：

```sh
pkexec /usr/bin/rm /usr/local/bin/gaomon-manage-panel /usr/local/bin/gaomon-shortcut-helper /usr/local/bin/gaomon-permission-setup /usr/local/share/applications/gaomon-manage-panel.desktop /usr/local/share/icons/hicolor/128x128/apps/gaomon-manage-panel.png /usr/local/share/licenses/gaomon-manage-panel/LICENSE /usr/local/share/doc/gaomon-manage-panel/NOTICE.md
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

GitHub Actions 在分支推送和 Pull Request 时构建 Debug 版本、运行测试并上传二进制压缩包及 pacman 包。推送标签（例如 `v0.1.0`）时，工作流先运行 Debug 测试，再构建 Release 版本，将两种产物上传到对应的 GitHub Release。标签转换为 pacman 版本时会移除开头的 `v`，并将不支持的字符替换为点；建议使用递增的版本标签。构建使用当前 Arch Linux 环境，产物依赖目标系统中兼容的动态库。工作流打包的第三方图标的许可情况见下方说明。

## 实现概览

- 面板通过当前会话的 KWin D-Bus 接口读取和写入设备设置，因而依赖 KDE Plasma Wayland。屏幕映射和压感设置由 KWin 保存。
- 预留软键时，界面将绘图区作为 100%，写入 KWin 前换算为完整感应区坐标。软键带高度暂按官方布局图的 `15 / 356` 估算，仍需实际设备验证。
- 压感曲线使用 KWin 的两个三次贝塞尔控制点；快捷键处理器运行时，面板会将映射和压感设置同步给虚拟笔。
- 启用快捷键时，面板通过 `pkexec` 启动辅助程序。辅助程序仅打开指定 1060PRO 的笔与按键接口，创建虚拟输入设备，并在打开设备后降回用户权限。辅助程序随面板停止；它不作为常驻系统服务安装。
- 登录自启动使用 `$XDG_CONFIG_HOME/autostart/gaomon-manage-panel.desktop`（未设置时为 `~/.config/autostart/`），保存开启该选项时的程序绝对路径。
- 免密授权由 `gaomon-permission-setup --enable|--disable` 管理，必须通过 `pkexec` 调用。工具从 `PKEXEC_UID` 识别调用用户，只写入或删除 `/etc/polkit-1/rules.d/49-gaomon-shortcuts-<UID>.rules`，不接受自定义用户、命令或输出路径。规则匹配 `org.freedesktop.policykit.exec`、构建时确定的 helper 安装路径及当前本地活动用户；启用前验证 helper 及上级目录由 root 拥有且组和其他用户不可写。免密模式始终使用该安装路径，修改 CMake 安装前缀后应重新构建、安装并重新启用免密规则。
- 笔侧键、12 个板侧键的输入组合依据本机 1060PRO 的实际 HID 报告识别。两枚笔侧键分别产生 `B` 和 `E` 键盘事件；重新绑定时这些原始事件会被拦截。不同固件可能使用其他编码。
- 默认情况下，应用配置由 Qt 保存到 `~/.config/GaomonManagePanel/Mapping.conf` 和 `~/.config/GaomonManagePanel/Shortcuts.conf`；设置了 `XDG_CONFIG_HOME` 时位置会随之改变。按键配置在保存后保留；成功启用后保存 `enabled` 状态，下次打开面板自动恢复，手动停用会清除此状态。`passwordless` 记录是否使用系统安装的 helper；取消免密必须通过权限工具移除系统规则。

## 资源与许可

本项目代码和文档以 [MIT 许可证](../LICENSE)发布。应用图标与 `assets/shortcut-preview/` 设备预览素材提取自 GAOMON 官方 Linux 驱动安装包，不适用项目的 MIT 许可证；其权利和再分发许可见[素材说明](../assets/NOTICE.md)。
