# 第三方组件与来源

本项目使用以下组件和素材。发行包中的路径相对于解压后的 `files/` 目录。

| 组件或素材 | 版本 / 来源 | 许可及文件 |
|---|---|---|
| 存档与资源解析 | [xPaw/order-of-the-sinking-cost](https://github.com/xPaw/order-of-the-sinking-cost)，提交 `0381631b4ac29cd9128041895949f00977b2c78e` | MIT；Copyright © 2026 Pavel Djundik。许可全文见仓库 [LICENSE](files/source/worker/upstream/LICENSE)，发行包同样保留。 |
| 地图底图 | [joric/ootss_tileset](https://github.com/joric/ootss_tileset)，提交 `818260c8b5fe704362722a9369350807ba635e68`；[互动地图](https://joric.github.io/ootss/) | 底图库未单独声明图片许可，游戏美术归原权利人。发行包的 `SinkingMap/terrain/provenance.json` 记录来源与文件校验值。 |
| 地图与存档数据参考 | [Jimmyzzt/OotSS_Sand](https://github.com/Jimmyzzt/OotSS_Sand) | 参考地图数据入口与坐标关系。 |
| Node.js | [Node.js 24.22.1](https://nodejs.org/dist/v24.22.1/) | 发行包 `SinkingMap/runtime/LICENSE.txt` 包含 Node.js 及附带组件许可。 |
| MinGW-w64、GCC 运行库、winpthreads | 构建工具链及运行库 | 发行包 `SinkingMap/licenses/` 包含各组件许可、GPL 和 GCC Runtime Library Exception。 |
| Windows 组件 | Win32、GDI+、DXGI、.NET Framework / Windows Forms | 使用系统组件。 |
| 界面星纹 | 根据游戏标题截图使用 ImageGen 制作 | 发行包 `SinkingMap/ui/star-rosette.png`，制作说明见同目录 `ASSET-NOTES.txt`。 |

项目新增代码采用 [MIT 许可](LICENSE)。第三方代码与素材遵循各自的许可和权利归属。
