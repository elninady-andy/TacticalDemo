"""Create a relocatable source+assets+local-model handoff; no UE installation included."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--stage", required=True)
parser.add_argument("--archive", required=True)
parser.add_argument("--qwen-runtime", required=True)
parser.add_argument("--stage-only", action="store_true")
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
workspace = project.parent
stage = Path(args.stage).resolve()
archive = Path(args.archive).resolve()
runtime = Path(args.qwen_runtime).resolve()
if stage.exists() or archive.exists():
    raise SystemExit("Output already exists; choose a new stage/archive to preserve earlier packages")
index = project / "Saved/PortableResourceIndex"
if json.loads((index / "MissingDependencies.json").read_text(encoding="utf-8")):
    raise SystemExit("Resolve missing UE dependencies before packaging")
stage.mkdir(parents=True)
target = stage / "TacticalDemo"
target.mkdir()
for folder in ("Config", "Content", "Source", "Scripts"):
    shutil.copytree(project / folder, target / folder)
shutil.copytree(project / "Binaries", target / "Binaries", ignore=shutil.ignore_patterns("*.pdb", "*.patch_*", "*.target"))
for name in ("TacticalDemo.uproject", ".gitignore", ".vsconfig", "README.md", "OPERATIONS.md", "StartDemo.cmd"):
    shutil.copy2(project / name, target / name)
for folder in ("Scenarios", "Evidence", "Automation", "FormationChatGpuAutomation"):
    if (project / "Saved" / folder).exists():
        shutil.copytree(project / "Saved" / folder, target / "Saved" / folder)
for name in ("PROJECT_STATUS.md", "PROJECT_FRAMEWORK.md"):
    shutil.copy2(workspace / name, stage / name)
shutil.copytree(workspace / "docs", stage / "docs")
shutil.copytree(index, stage / "ResourceIndex")
if (workspace / "素材").exists():
    shutil.copytree(workspace / "素材", stage / "OriginalMaterials/素材")
qwen = stage / "QwenRuntime"
qwen.mkdir()
shutil.copytree(runtime / "model", qwen / "model")
shutil.copy2(runtime / "qwen_local_server.py", qwen / "qwen_local_server.py")
lock = subprocess.check_output([str(runtime / "venv/Scripts/python.exe"), "-m", "pip", "freeze"], text=True)
(qwen / "requirements-lock.txt").write_text(lock, encoding="utf-8")
(qwen / "PYTHON_VERSION.txt").write_text("Python 3.14.0 x64\nCPU torch; recreate venv on destination.\n", encoding="utf-8")

# Adapt only the staged copy; preserve the working computer's existing launch behavior.
for script in (target / "Scripts").glob("*.ps1"):
    text = script.read_text(encoding="utf-8-sig")
    if re.search(r"\[string\]\$EngineRoot\s*=\s*'E:\\ue\\UE_5.3'", text):
        text = re.sub(r"\[string\]\$EngineRoot\s*=\s*'E:\\ue\\UE_5.3'", "[string]$EngineRoot=''", text)
        first, rest = text.split("\n", 1)
        text = first + '\n. "$PSScriptRoot\\PortablePaths.ps1"\n$EngineRoot=Resolve-PortableEngine $EngineRoot\n' + rest
    if script.name == "StartQwen.ps1":
        text = text.replace("[int]$Port=8000)", "[int]$Port=8000,[string]$RuntimeRoot='')")
        text = text.replace("$runtime='E:\\CodexTools\\QwenRuntime'", '. "$PSScriptRoot\\PortablePaths.ps1"\n$runtime=Resolve-PortableQwen $RuntimeRoot')
        text = text.replace("'E:\\\\CodexTools\\\\QwenRuntime\\\\qwen_local_server\\.py'", "[regex]::Escape($server)")
        text = text.replace("@($server,'--model-dir',$model,", '@("`"$server`"",\'--model-dir\',"`"$model`"",')
    if script.name == "TestStartQwen.ps1":
        text = '. "$PSScriptRoot\\PortablePaths.ps1"\n$portableServer=Join-Path (Resolve-PortableQwen \'\') \'qwen_local_server.py\'\n' + text
        text = text.replace("'E:\\\\CodexTools\\\\QwenRuntime\\\\qwen_local_server\\.py'", "[regex]::Escape($portableServer)")
    script.write_text(text, encoding="utf-8-sig")

(stage / "LocalPaths.example.json").write_text(json.dumps({"EngineRoot": "D:/Epic Games/UE_5.3", "QwenRuntimeRoot": ""}, indent=2), encoding="utf-8")
(stage / "AGENTS.md").write_text("""# Codex 接手入口
先读 PROJECT_MAP.md、PROJECT_STATUS.md、MIGRATION.md、TacticalDemo/OPERATIONS.md。
本工程为 UE5.3.2 + C++ + 本地Qwen验证Demo；配置和当前Content是资源真源。
目录可移动，不要恢复旧电脑E盘硬编码；机器路径在LocalPaths.json或脚本参数配置。
ResourceIndex/AssetIndex.json包含全部资产及依赖；MAP_RESOURCES.md包含各地图的有效运行资源。
TacticalDemo/Saved/Scenarios保留已保存方案；飞机由C++在Play时生成。
飞机保持静止。编队修改手动应用且可撤销。不添加聊天颜色修改功能。
不要重新创建已有工程、地图、飞机资源。只读相关源码，避免全盘重复扫描。
接手历史以PROJECT_STATUS为准；本包不包含Codex聊天会话或账号配置。
先检查机器配置，再按实际修改范围验证；不要把旧证据当作新机器已验证。
""", encoding="utf-8")
(stage / "PROJECT_MAP.md").write_text("""# 项目入口与目录地图（迁移包）

先读 PROJECT_STATUS.md 了解已完成工作，再读 MIGRATION.md 完成目标电脑配置。

| 目录/文件 | 用途 |
| --- | --- |
| TacticalDemo/TacticalDemo.uproject | UE5.3 C++工程入口 |
| TacticalDemo/Content/VRAirport/Maps/VRAirportMap.umap | 当前机场地图 |
| TacticalDemo/Content/Maps/Demo.umap | 基础演示地图 |
| TacticalDemo/Content/plane/J20/ | J20网格、材质、两款贴图 |
| TacticalDemo/Content/plane/J10/ | J10网格、材质、两款贴图 |
| TacticalDemo/Content/VRAirport/ | 机场几何、布景、材质和贴图 |
| TacticalDemo/Content/SoftOutline/ | 机场材质的补齐依赖 |
| TacticalDemo/Config/EquipmentLibrary.json | 型号、网格、材质、贴图登记 |
| TacticalDemo/Config/SceneLibrary.json | 场景名称与地图映射 |
| TacticalDemo/Config/AirportScenario.json | 机场初始飞机配置 |
| TacticalDemo/Config/Qwen.json | 本地聊天接口 |
| TacticalDemo/Source/TacticalDemo/ | C++实现及Tests |
| TacticalDemo/Saved/Scenarios/ | 已保存场景及编队方案 |
| TacticalDemo/OPERATIONS.md | 可用指令及操作说明 |
| ResourceIndex/MAP_RESOURCES.md | 每张地图的资源文件和运行时依赖 |
| ResourceIndex/AssetIndex.json、AssetIndex.csv | 全部资产类型、路径和直接依赖 |
| FILE_MANIFEST.json | 所有包内文件的SHA256清单 |
| QwenRuntime/model/、qwen_local_server.py | 本地Qwen权重及服务 |
| QwenRuntime/requirements-lock.txt | 当前Python依赖精确版本 |
| docs/ | 原阶段设计与实施计划 |
| OriginalMaterials/ | 工作目录保留的原始素材归档 |

/Game/资源路径映射到TacticalDemo/Content/，例如
`/Game/plane/J20/mesh/J20S.J20S`对应`Content/plane/J20/mesh/J20S.uasset`。
/Engine/资源来自UE安装，不在工程包内。运行时飞机通过JSON动态生成，umap只保存机场布景。

主要源码：DemoGameMode（场景入口/鼠标）、DemoScenarioManager（应用/撤销/快照）、
DemoControlWidget（聊天）、DemoLayoutWidget（单位/编队/方案）、FormationCommands（本地编队指令）、
ChatProtocol（Qwen协议）、ScenarioPresetStore（方案）、EquipmentLibrary（资源登记）。
""", encoding="utf-8")
(stage / "MIGRATION.md").write_text(r"""# 在另一台电脑打开与继续开发

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
7. 保存方案在Saved/Scenarios；加载后才能通过聊天调用其中的编队。没有默认编队时在窗口选择成员并保存。

Qwen接口： http://127.0.0.1:8000/v1/chat/completions，模型名qwen。编队规则指令不依赖Qwen；
可直接运行Scripts/RunDemo.ps1，仅启动UE窗口。StartDemo.cmd会先启动Qwen。
若本机3.14的精确锁定依赖无法从软件源安装，查看requirements-lock.txt，配置可用源或使用已部署兼容环境；
不要复制旧venv，也不要猜测新电脑已经验证成功。

## 文件与资源验证

解压后、首次构建前执行`python VerifyPackage.py`核对SHA256（Python3.10以上即可）。
FILE_MANIFEST包含打包时快照；构建或编辑后的变化属于正常，不再用于比对原包。
ResourceIndex/AssetIndex.json来自UE资产注册表，包含静态地图依赖和JSON动态资源依赖，
AssetIndex.csv可按目录/类型搜索；MAP_RESOURCES.md可直接定位地图所需资源。
导入FBX时保留材质/贴图引用并在EquipmentLibrary登记；移动UE资产用UE编辑器完成，避免破坏包路径。

## 验证范围

本机已通过资源依赖检查、打包文件校验和迁移脚本检查。
目标电脑仍需完成Setup、重新编译、实际运行验证，尤其UE版本/显卡/本地Python依赖。
包含Binaries供相同版本参考；缺少或不兼容时通过Source重新编译。
开发用历史证据在Saved/Evidence，派生缓存/Intermediate/.vs/Autosaves/崩溃转储未复制。
原始重复UE工程和重复总压缩包未复制；当前工程需要的Content已经完整包含并补齐依赖。
""", encoding="utf-8")
(stage / "VerifyPackage.py").write_text('''import hashlib, json
from pathlib import Path
root=Path(__file__).resolve().parent
manifest=json.loads((root/"FILE_MANIFEST.json").read_text(encoding="utf-8"))
for item in manifest["files"]:
    path=root/item["path"]
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(4*1024*1024),b""):
            digest.update(block)
    if path.stat().st_size!=item["bytes"] or digest.hexdigest()!=item["sha256"]:
        raise SystemExit("Mismatch: "+item["path"])
print("PACKAGE_FILES_VERIFIED",len(manifest["files"]))
''', encoding="utf-8")
if args.stage_only:
    print("STAGED_PREPARED", stage, flush=True)
    raise SystemExit(0)
subprocess.run([sys.executable, str(project / "Scripts/ArchivePortable.py"), "--stage", str(stage), "--archive", str(archive)], check=True)
