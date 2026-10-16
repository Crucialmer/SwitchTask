# SwitchTask

![GitHub release (latest by date)](https://img.shields.io/github/v/release/Crucialmer/SwitchTask)
![Github Release Downloads](https://img.shields.io/github/downloads/Crucialmer/SwitchTask/total)
![Language](https://img.shields.io/badge/language-C++-239120)
![OS](https://img.shields.io/badge/OS-Windows-0078D4)
![License](https://img.shields.io/github/license/Crucialmer/SwitchTask)

一款 macOS 风格的 Windows 窗口切换器：列表中的**每个窗口独立一项**、按最近使用排序、即选即切。支持 `Windows` 10 / 11（64 位）。

![ui](img/ui.png)

## 📦 下载与安装

到 [Releases](https://github.com/Crucialmer/SwitchTask/releases/latest) 下载 `SwitchTask.zip` 解压即用（解压所得文件夹即为标准的 `SwitchTask`），运行 `SwitchTask.exe`（内置 Qt 运行库，免安装）。已安装用户可在托盘菜单使用「Check for Updates」一键应用内更新。

## ⚡ 主要功能

### 1. `Alt+Tab`：在窗口间切换

- 按住 `Shift` 反向切换；选中项高亮（方圆角选中底 + 图标放大动效）
- 松开 `Alt` 后切换器保持显示：鼠标单击图标**直接切换**（`Enter` / 空格亦可确认），`Esc` / 点击别处取消
- `Ctrl`+`Alt`+`Tab`：同 `Alt`+`Tab`（拦截系统固定切换器，行为与按下 `Alt`+`Tab` 一致）
- 每个窗口独立一项，按各自**最近使用**排序；底部实时显示选中窗口的标题（超长中部省略）

![switching](img/switching.gif)

### 2. 鼠标悬停即时选中 · 中键关闭窗口

- 鼠标悬停在图标上**实时选中**：底部标题随鼠标位置即时更新，无需先点击
- **鼠标中键**单击图标 = 关闭该窗口（发送正常关闭请求，可触发应用自身的保存确认）
- `Ctrl`+`W`：关闭当前选中的窗口（等效中键单击）

![hover](img/hover.gif)

### 3. `Alt+`` `：在同一应用的多个窗口间轮换

- 按住 `Shift` 反向切换；**最近活动**的窗口优先访问
- 切换器未显示时用于同应用窗口轮换（显示时 `` ` `` 键用于列表选择）

### 4. 滚轮与键盘操作

- 滚轮悬停图标：↑ 该窗口**置顶预览**（不改变焦点）/ ↓ **最小化**该窗口
- 循环选择：`Tab`·`Shift`+`Tab` / `` ` ``·`Shift`+`` ` `` / `←`·`→`；`↑`·`↓`：该窗口**置顶预览** · **最小化**
- Vim 风格快捷键：`H` `L` 切换选中 / `K` 置顶预览 · `J` 最小化（与方向键 ↑/↓ 语义一致）
- `Enter` / 空格：确认切换 · `Esc` / 点击别处：取消

### 5. 任务栏滚轮（Beta）

在任务栏应用图标上滚动：↑ 切换到上一个窗口 / ↓ 最小化上一个窗口。实验性功能，部分应用可能失效。

## 🌟 更多特色

- **macOS 风格设计**：常暗深色通透 HUD 面板（深石墨半透明底 + 浅色文字 + 顶缘受光 + 细描边，切换器 HUD 规范）；马卡龙·婴儿蓝连续曲率方圆角选中底（n=5 超椭圆，圆润饱满零折痕，无描环）；无损保留应用图标原生 1:1 像素锐利度；底部标题水平恒定绝对居中（无横向跳动，中段智能省略）；垂直留白黄金视觉平衡；面板按本组全部任务标题的最大宽度自适应加宽（同一组任务宽度恒定）
- **macOS 26 风格弹出窗口与右下角贴靠**：独立设置窗口（`SettingsDialog`）与检查更新窗口（`UpdateDialog`）完全按照 macOS 26 窗口设计规范打造，集成红黄绿三色矢量交通灯自绘标题栏（支持无边框拖拽移动）、Hero 头部 48px 原生方圆角应用图标与版本胶囊、磨砂卡片式更新日志容器、动态下载数据读数、亚克力通透毛玻璃与 Win11 12px 窗口平滑圆角，并由 `positionBottomRight` 算法智能贴靠屏幕右下角呼出
- **macOS 26 原生自绘悬浮气泡通知（`MacToast`）**：彻底淘汰 Win11 系统消息弹窗，采用纯自绘亚克力毛玻璃、12px 平滑圆角、左侧 32px 方圆角应用图标与三色矢量状态徽标（成功绿/提示蓝/警告橙）、零焦点抢占（`WA_ShowWithoutActivating` 不打断任何打字或操作）、平滑淡入淡出及悬停驻留机制，右下角优雅呼出，带来沉浸式 macOS 桌面体验
- **macOS 原生级托盘弹出菜单与静音无扰**：集成两行式专属品牌状态卡片（应用图标 + 品牌名 + 磨砂版本胶囊 + 矢量呼吸圆点 + 实时状态行，支持单击直达设置）、268px+ 宽屏黄金比例、统一 38px 文本对齐列、14px 居中对号与 12px+ 舒适间距，以及 Apple 原厂高饱和系统蓝勾选标记与深浅色毛玻璃适配；菜单功能切换（自启动、显示器切换）与配置更新全面静音无扰，杜绝 Win11 系统消息弹窗打扰
- **深浅色模式自适应**：弹窗、设置窗口、更新窗口、托盘菜单（圆角 + 毛玻璃）跟随系统浅色 / 深色模式实时切换；可用 `config.ini` 的 `ThemeMode` 强制指定
- 窗口背景**毛玻璃**特效 + 适配 `Win11` 窗口圆角
- **高 DPI 适配**：Per-Monitor V2 逐屏缩放感知，支持 4K / 8K 等高分辨率显示器
- 支持多屏幕显示（跟随鼠标所在屏幕 / 主屏幕）
- 聊天窗口（QQ）多开时，可在应用图标上叠加**当前会话头像角标**，各窗口一眼可辨
- 更高权限的窗口（如任务管理器）也能正常切换
- 应用过多时**自动压缩**图标与单元格，保证整行容纳在屏幕内
- 内置**自动更新**检查；标题字体可配置

## ✒️ 更多任务（可选开发）

- [ ] 自定义配置
- [ ] 多行布局（窗口过多时的备选呈现方案）
- [ ] ...

## 🚀 开机自启动

在托盘菜单中点击「开机自启动」即可设置开机自启动（再次点击取消），启动权限与当前程序权限一致。

## ⚙ 配置

配置保存在 `config.ini` 文件中，有两种方式可以修改配置：

1. 直接修改程序目录下的 `config.ini` 文件，并重启程序（若程序目录不可写——如安装在 Program Files——配置会自动回退到 `%APPDATA%`（Roaming 应用数据目录））
2. 🌟\[推荐\] 单击托盘菜单顶部状态卡片或右键托盘菜单中的「设置...」选项，此时会自动用文本编辑器打开配置文件；修改后，保存并**关闭文件**，程序会自动重载配置

### 配置项

#### 字体

```ini
[label]
font_family = "Microsoft YaHei UI"
font_size = 10.5
font_weight = bold

[menu]
font_family = "Microsoft YaHei UI"
font_weight = bold
font_size = 13
```

> `font_family` 字体族：弹出窗口（`[label]`）与托盘菜单（`[menu]`）均默认使用简体中文系统默认字体 **`Microsoft YaHei UI`（微软雅黑）**，支持在配置文件中自由定制（如 `"Segoe UI"`, `"PingFang SC"` 等）。
> `font_size` 支持小数，如 `10.5`（100% 缩放下恰好为 14px，显示更清晰）。
> `font_weight` 字重：支持 `400` 常规 / `500` 中等 / `600` 半粗 / `700` 粗体，或 `normal` / `medium` / `bold`。弹出窗口与托盘菜单均默认以 `bold`（700）饱满字重协调渲染，支持在配置文件中随心定制。

#### 界面

```ini
ThemeMode = auto
DisplayMonitor = 1
```

> `ThemeMode`：`auto` 跟随系统深浅色（默认）/ `light` 浅色 / `dark` 深色；也可写 `0` / `1` / `2`。
> `DisplayMonitor`：`0` 主屏幕 / `1` 跟随鼠标所在屏幕（默认）；与托盘菜单「显示器设置」同步。

## 📄 License

本项目采用 [GPL-3.0](LICENSE) 许可发布：可自由使用、修改与分发，衍生作品须同样以 GPL-3.0 开源。

© 2026 Crucialmer
