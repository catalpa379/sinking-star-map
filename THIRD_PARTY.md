# 来源与许可

## 地图底图

底图来自 **joric** 的[沉星之序互动地图](https://joric.github.io/ootss/)、[地图项目](https://github.com/joric/ootss)和[底图库](https://github.com/joric/ootss_tileset)（固定提交 `818260c8b5fe704362722a9369350807ba635e68`）。地图代码仓库采用 Unlicense；底图库未单独声明图片许可，游戏美术归原权利人。底图来源记录见发行包的 `SinkingMap/terrain/provenance.json`。

## 存档与资源解析

使用 **Pavel Djundik (xPaw)** 的 [order-of-the-sinking-cost](https://github.com/xPaw/order-of-the-sinking-cost) 部分代码（固定提交 `0381631b4ac29cd9128041895949f00977b2c78e`），采用 MIT 许可。原始许可保留在 `source/worker/upstream/LICENSE` 和发行包中。

地图和存档数据入口参考了 [OotSS Sand](https://github.com/Jimmyzzt/OotSS_Sand)。

## 运行库与构建工具

- Node.js 24.22.1：许可见发行包 `SinkingMap/runtime/LICENSE.txt`。
- MinGW-w64、GCC libstdc++、libgcc 和 winpthreads：许可及 GCC Runtime Library Exception 见发行包 `SinkingMap/licenses`。
- GDI+、Win32 和 DXGI：使用 Windows 系统组件。

## 界面装饰

`SinkingMap/ui/star-rosette.png` 根据游戏标题截图使用 ImageGen 制作；提示词摘要见同目录的 `ASSET-NOTES.txt`。

本项目新增代码采用 [MIT 许可](LICENSE)。第三方代码、游戏美术和字体遵循各自的许可与权利归属。
