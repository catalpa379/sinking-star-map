# 源码与构建

发行包的 `source/native/map.cpp` 是原生 DLL 源码；`source/worker` 是随包 Node.js 读取与投影程序。`installer/Installer.cs` 和 `installer/app.manifest` 是安装器源码。修改代码后必须重新生成 SHA256.json 与 package-integrity.json 中相应文件的 SHA-256，否则安装器会拒绝安装。

在 files 目录下，使用 x64 MinGW-w64 g++（验证版本15.1.0）构建 DLL：

```powershell
g++ -std=c++20 -O2 -static -shared -o dxgi.dll source/native/map.cpp -lgdi32 -luser32 -lshell32 -lgdiplus -lole32 -luuid
```

在独立目录运行原生离屏测试：

```powershell
g++ -std=c++20 -O2 -static -o native-test.exe source/tests/native-test.cpp -lgdi32 -luser32 -lshell32 -lgdiplus -lole32 -luuid
.\native-test.exe .\SinkingMap\terrain
.\SinkingMap\runtime\node.exe --test source/tests/privacy.test.mjs
```

Worker 原始来源的 MIT 许可保留在 SinkingMap/licenses 与 upstream 目录。修改并重新分发前请同时阅读 THIRD_PARTY.md。

发行包根目录不再提供“测试验证记录.txt”和“先读我-安装与测试.txt”。安装说明放在 README 和 Release 页面，验证记录保留在开发记录中。准备好完整离线目录并更新完整性清单后，用以下命令打包；脚本会排除这两个旧文件并生成 ZIP 与 SHA256 文件：

```powershell
python tools/package_release.py <完整离线目录> <输出ZIP路径> --version 0.1.18
```

在完整解压目录中构建安装器（Windows .NET Framework 4.x）：

```powershell
& "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe /platform:x64 /optimize+ /codepage:65001 /r:System.Windows.Forms.dll /r:System.Drawing.dll /r:System.Web.Extensions.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /win32manifest:files\installer\app.manifest /out:安装与卸载.exe files\installer\Installer.cs files\installer\KeySettings.cs
```

安装器的 `--verify`、`--prepare-runtime`、`--install-to`、`--uninstall-from` 参数供隔离测试使用。测试卸载请使用隔离目录，并先退出游戏。

资源使用 .smt 容器，界面装饰位于 ui/star-rosette.png。
自定义按键实现见 native/keybindings.hpp 和 installer/KeySettings.cs；配置保存在用户 LocalAppData，卸载时保留。

0.1.8 花纹位置回归（使用相同的原生绘制路径与窗口缩放公式，无可见窗口）：

```powershell
g++ -std=c++20 -O2 -static -o ornament-test.exe source/tests/ornament-test.cpp -lgdi32 -luser32 -lshell32 -lgdiplus -lole32 -luuid
.\ornament-test.exe .\SinkingMap
```

此测试覆盖 980×620、1440×900、1920×1080、2560×1440、3840×2160，
逐像素确认顶部和侧栏两处花纹均可见，所有装饰差异仅限允许区域。
原生绘图与图片解码使用系统 GDI+；存档解析使用随包 Node。

