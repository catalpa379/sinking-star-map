# 来源、依赖与许可核查

## 用户要求的完整离线测试包

本包按用户后续要求，把已准备的地图缓存与通用运行库放入离线测试包，在当前聊天中交付；不包含开发者存档或进度图。游戏美术不属于本项目 MIT 许可，来源记录保留于 SinkingMap/terrain/provenance.json。

打包时从官方来源取得并核对 SHA-256：Node.js 24.22.1。原始许可保留在各自运行库中。安装器只核验并使用包内文件，不注册系统 Python、修改 PATH 或安装全局软件。

官方元数据来源：
- https://nodejs.org/dist/v24.22.1/SHASUMS256.txt

安装器为 files/installer/Installer.cs，使用系统 .NET Framework / Windows Forms；源码采用本包 MIT。安装与日常游戏进度读取均不联网。source/tools/prepare_terrain.py 保留为开发工具，离线安装器不会调用它或下载文件。

## 用户指定参考

- 网站：https://sand.zzt.si/
- 源码：https://github.com/Jimmyzzt/OotSS_Sand
- 核查文件：`docs/save-reading.md`、`ootss/templates/atlas-save.js`、`ootss/maps.py`、`data/map_assets.json`、`docs/hosting.md`、README、package.json、pyproject.toml。

该项目提供 Leaflet 地图、谜题索引与本地 Status SOLVED 读取；其文档明确不读取迷雾、位置或世界历史。因此原有显示逻辑不能直接满足本任务。也没有发现项目整体 LICENSE；没有复制其网页实现、人工视频标注或攻略数据到发行包。

参考项目依赖包括 Python 的 numpy/scipy/OpenCV/Pillow/yt-dlp，网页使用 Leaflet，托管侧有 Wrangler 和 jose。本模组不需要这些网页、视频处理或云托管依赖。复用的是公开的数据入口和地图坐标关系，并独立编写可见性裁除、原生交互与安装逻辑。

## 地形图片

来源：参考项目指向的 https://github.com/joric/ootss_tileset ，固定提交 `818260c8b5fe704362722a9369350807ba635e68`。

对应地图代码仓库 https://github.com/joric/ootss 的软件许可为 Unlicense；但本次核查的独立 tileset 仓库根目录没有单独声明图像许可。不能把另一个仓库的软件许可自动解释成所有游戏图片的再分发授权。底图由游戏画面衍生，游戏美术权利仍归原权利人。

本包按用户明确要求包含此前准备的三层详细图像缓存，用于此次离线测试交付，并保留出处记录。这不表示素材取得了 MIT 授权；未来公开发布素材包仍应先取得或确认素材的再分发许可。

坐标：原图投影左边 x=-24.5，顶边 y=170.5，宽高各190世界单位。第4级16×16个512像素瓦片形成8192方图。实际显示还须经过本存档的揭露掩码，不能直接展示完整图。没有复用答案、录像、关卡预览和剧情数据。

## 存档、资源包及实体结构解析

来源：https://github.com/xPaw/order-of-the-sinking-cost

固定提交：`0381631b4ac29cd9128041895949f00977b2c78e`，MIT，Copyright (c) 2026 Pavel Djundik。许可全文保留于 `source/worker/upstream/LICENSE` 和运行目录对应文件。

使用其 Reader、Status、History、实体 blob / schema、只读 package、坐标 / 分层 / 体素辅助模块的子集。主要改动：严格 UTF-8、长度校验、Status 尾部拒绝、显式本机路径；新增游戏文件哈希与世界版本校验、拒绝部分历史解析、统一防剧透投影和匿名化未进入入口。没有使用上游的解谜、路线推荐、答案或可达性求解功能。

曾只读研究 qiekn/ootss-cmd 的相机文档与许可，以评估原 Tab 地图接入；没有将其相机、迷雾开关、作弊或内存写入代码纳入模组。

## 运行库和构建运行库

- Node.js 24.22.1，完整 Node 及附带组件许可：`SinkingMap/runtime/LICENSE.txt`。运行解析不依赖第三方 npm 包。
- GDI+、Win32、DXGI 使用 Windows 系统组件，不随包复制系统 DLL。
- MinGW-w64、GCC libstdc++ / libgcc / winpthreads 静态运行库，相关许可和 GCC Runtime Library Exception 保留于 `SinkingMap/licenses`。没有把系统 dxgi.dll 打包进本项目；代理通过 System32 绝对路径转发。

新增源代码采用 MIT，见 LICENSE。第三方代码和游戏素材不因本项目许可而改变归属。

## 界面装饰

`SinkingMap/ui/star-rosette.png` 使用内置 ImageGen，根据用户提供的游戏标题截图重建无文字的星纹装饰。它仅是 UI 底层，不包含地图或进度。提示词摘要见同目录 ASSET-NOTES.txt。参考中的游戏视觉元素仍属于其原权利人。

0.1.8 已移除此前可达分析模块及数据；Python/Pillow 只属开发工具，发行包不再附带。

## 2026-10-30 来源补充核对

用户确认下载来源为 https://joric.github.io/ootss/ 。该站对应主仓库 https://github.com/joric/ootss 标示Unlicense，许可全文 https://github.com/joric/ootss/blob/main/LICENSE 。其Wiki的Map Capture章节将高分辨率底图指向独立仓库 https://github.com/joric/ootss_tileset 。该独立仓库本轮页面未展示LICENSE文件或图像许可说明。
发布材料保留joric及两个仓库/网站的出处，不把底图标为本工具原创或MIT素材。注明来源本身不构成对独立底图分发条件的确认；此处记录待确认范围，不推定作者禁止分发。
