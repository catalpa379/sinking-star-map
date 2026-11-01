<h1 align="center">沉星之序地图</h1>
<p align="center">读取本地存档，查看未完成的谜题，方便后期查漏补缺。</p>
<p align="center">
  <img alt="版本" src="https://img.shields.io/badge/version-0.1.18%20%E6%B5%8B%E8%AF%95%E7%89%88-c5a568">
  <img alt="平台" src="https://img.shields.io/badge/platform-Windows%20x64-4476b5">
  <a href="LICENSE"><img alt="代码许可 MIT" src="https://img.shields.io/badge/license%20%28code%29-MIT-739b82"></a>
</p>

《沉星之序》（Order of the Sinking Star）的非官方 Windows 地图工具。玩到后期找不到之前漏掉的谜题时，可以读取本地存档，在地图上查看未完成地点。

**默认保留探索迷雾，也可主动开启剧透模式，查看全地图未完成谜题。**

**当前版本：0.1.18 测试版。** 支持 Steam Build **26006400 / 26249670** 的匹配版本。详见 [发布说明](https://github.com/catalpa379/sinking-star-map/releases/tag/v0.1.18)。

## 下载与安装

**[下载 0.1.18 完整离线安装包](https://github.com/catalpa379/sinking-star-map/releases/download/v0.1.18/Sinking-Star-Map-0.1.18-Offline.zip)** · [版本发布页](https://github.com/catalpa379/sinking-star-map/releases/tag/v0.1.18) · [SHA256 校验文件](https://github.com/catalpa379/sinking-star-map/releases/download/v0.1.18/Sinking-Star-Map-0.1.18-SHA256.txt)

安装步骤：

1. 完整解压安装包。
2. 正常退出游戏，运行“安装与卸载.exe”。
3. 核对Steam游戏目录，点击“安装 / 更新”。
4. 启动游戏，用设置的按键打开地图，选择与游戏一致的档案。

初始打开/关闭按键为 **F8**，刷新为 **F5**，均可在安装器或地图右上角“按键设置”中修改。卸载使用对应版本安装器，未知文件、存档、按键设置与旧版备份保留。

## 操作

| 按键或操作 | 功能 |
|---|---|
| F8 | 打开 / 关闭地图 |
| F5 | 刷新地图 |
| 按键设置 | 在安装器或地图右上角修改快捷键 |
| 搜索与筛选 | 按名称、区域或完成状态查找地点 |
| 地图操作 | 缩放、拖动地图，切换存档已揭露的地图层 |
| 剧透模式 | 点击地图顶部的“剧透：关 / 开”按钮，切换全地图未完成模式 |

地图以最近一次存档为准，保存游戏后可手动刷新。默认搜索、地图内统计基于所选存档已揭露的范围，谜题按关卡去重；“存档总完成”直接读取存档记录的完成总数。关闭地图、切档或失焦后恢复默认显示范围。

![剧透模式独立按钮](docs/images/spoiler-toggle.png)

## 界面预览

<details>
<summary>展开地图主界面预览（含已探索地图画面）</summary>

![地图主界面](docs/images/map-overview.jpg)

</details>

完成状态示例：编号使用辅助色，未完成柔粉、已完成灰绿。

| 未完成 | 已完成 |
|---|---|
| ![未完成详情](docs/images/detail-unfinished.png) | ![已完成详情](docs/images/detail-complete.png) |

详情卡使用示例数据。

## 适用范围

| 项目 | 当前范围 |
|---|---|
| 系统 | Windows x64 |
| 游戏 | Steam Build **26006400 / 26249670** 的精确匹配版本 |
| 建议窗口模式 | 窗口或无边框 |

游戏更新后可能需要重新适配；遇到未知版本时会停止读取或安装。

## 源码与构建

- `files/source/native`：Win32/GDI+界面与DXGI代理。
- `files/source/worker`：本地存档、资源解析与可见性投影。
- `files/installer`：WinForms安装器。
- `files/source/tests`：逻辑与原生测试。

构建说明见 [BUILD.md](BUILD.md)，命令在 `files` 目录执行。完整离线发行包需另行准备 Node 二进制、地形缓存和游戏字体，并生成完整性清单。

```powershell
cd files
node --test source/tests/privacy.test.mjs source/tests/refresh.test.mjs source/tests/misc-areas.test.mjs source/tests/region-acquisition.test.mjs
```

逻辑测试使用 Node.js 24。真实存档集成测试需要本机受支持游戏与独立临时目录。

## 许可证

项目新增代码采用 [MIT](LICENSE)。第三方组件和素材的来源与许可见 [THIRD_PARTY.md](THIRD_PARTY.md)。



