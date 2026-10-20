# 在另一台电脑打开与继续开发

本页适用于包含模型及保存方案的完整迁移包。GitHub源码克隆不包含这些本机数据，请先按[GitHub使用说明](docs/GITHUB.md)取回LFS资源、用-SkipQwen构建，并使用RepositoryManifest.json校验。

这是完整可编辑工程包，含源码、全部Content、地图、配置、保存方案、Qwen权重和服务代码。
不包含UE/Visual Studio安装、旧电脑Python虚拟环境、派生缓存或Codex聊天账号数据。

1. 完整解压到短路径，例如D:/UEPack；机场原始贴图文件名较长，请保持最外层目录路径短于15个字符。
   不要只复制umap，也不要在zip内运行。
2. 在Codex中打开解压后的最外层目录（能看到AGENTS.md和PROJECT_MAP.md）。可发送：
   “读取AGENTS.md、PROJECT_MAP.md与PROJECT_STATUS.md，检查迁移环境，在此工程继续开发，不重新建项目。”
3. 安装UE 5.3.2和Visual Studio 2022的C++游戏开发工具、Windows SDK。首次本地Qwen环境使用Python 3.14 x64。
4. 将LocalPaths.example.json复制为LocalPaths.json，把EngineRoot改为目标电脑UE安装目录。
   QwenRuntimeRoot留空使用包内QwenRuntime；也可填目标电脑已部署的兼容服务目录。
5. 用PowerShell在最外层目录执行（第一次安装Python依赖需要联网，权重已在包内，无需重新下载）：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\SetupPortable.ps1 -EngineRoot 'D:\Epic Games\UE_5.3' -PythonExe 'C:\Python314\python.exe'
```

路径按目标电脑实际安装修改。已部署兼容Qwen时可使用-SkipQwen，只配置路径并构建UE。
只检查工程和路径、不安装不编译可加-CheckOnly。
6. 双击TacticalDemo/TacticalDemo.uproject；如提示重编译模块，选择重编译。
   要生成VS工程，右键uproject选择Generate Visual Studio project files；旧机器sln未复制。
   默认打开机场地图，点击Play后出现飞机及C++窗口。或双击TacticalDemo/StartDemo.cmd。
7. 保存方案在Saved/Scenarios；加载后可以继续使用其中的编队、飞机高亮和自定义航迹。Ctrl＋点击可多选飞机，在编辑窗口直接建立编队、批量改位置及编辑航点；临时选中状态不写入方案。没有默认编队时，也可输入“把红方飞机编成编队3”并手动应用。

Qwen接口： http://127.0.0.1:8000/v1/chat/completions，模型名qwen。编队规则指令不依赖Qwen；
可直接运行Scripts/RunDemo.ps1，仅启动UE窗口。StartDemo.cmd会先启动Qwen。
若本机3.14的精确锁定依赖无法从软件源安装，查看requirements-lock.txt，配置可用源或使用已部署兼容环境；
不要复制旧venv，也不要猜测新电脑已经验证成功。

## 文件与资源验证

解压后、首次构建前执行`python VerifyPackage.py`核对SHA256（Python3.10以上即可）。当前目录优先使用CURRENT_MANIFEST.json；旧包没有该文件时仍使用FILE_MANIFEST.json。

CURRENT_MANIFEST记录本轮可迁移源工程文件、Content、模型权重和Saved/Scenarios。它不是独立运行安装包；不包含本机LocalPaths.json、venv、缓存、历史Saved验证日志或调试符号。迁移后仍需安装UE和Python依赖。

FILE_MANIFEST始终保留最初迁移包快照。可用`python VerifyPackage.py --manifest FILE_MANIFEST.json`明确检查原快照；开发后的正常变化会报不一致，不应为了校验通过而恢复旧文件或覆盖保存方案。

有意发布新版本时，先完成构建及验收，再运行`python CreateCurrentManifest.py`生成新的CURRENT_MANIFEST.json，随后运行VerifyPackage.py。遇到缺失或校验错误时先查原因，不要通过重新生成清单掩盖错误。校验器只核对清单中的文件，不会删除额外文件，也不替代UE测试。

`Scripts/SetupPortable.ps1 -CheckOnly`现在同时执行三张登记地图、对应初始方案、设备库直接资源和轨迹配置的文件/基础JSON检查；不加载UE，不代表资产内部依赖或飞行功能已经在目标机验证。Windows PowerShell5.1与PowerShell7均已用缺失资源夹具验证。
ResourceIndex/AssetIndex.json来自UE资产注册表，包含静态地图依赖和JSON动态资源依赖，
AssetIndex.csv可按目录/类型搜索；MAP_RESOURCES.md可直接定位地图所需资源。
导入FBX时保留材质/贴图引用并在EquipmentLibrary登记；移动UE资产用UE编辑器完成，避免破坏包路径。

## 验证范围

本机已通过资源依赖检查、打包文件校验和迁移脚本检查。
目标电脑仍需完成Setup、重新编译、实际运行验证，尤其UE版本/显卡/本地Python依赖。
包含Binaries供相同版本参考；缺少或不兼容时通过Source重新编译。
开发用历史证据在Saved/Evidence，派生缓存/Intermediate/.vs/Autosaves/崩溃转储未复制。
原始重复UE工程和重复总压缩包未复制；当前工程需要的Content已经完整包含并补齐依赖。

