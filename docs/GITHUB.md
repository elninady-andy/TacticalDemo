# GitHub 使用与交付说明

仓库地址：[elninady-andy/TacticalDemo](https://github.com/elninady-andy/TacticalDemo)，为公开仓库，可直接查看和克隆。项目原创代码采用 [MIT License](../LICENSE)，第三方素材的许可范围见 [THIRD_PARTY](../THIRD_PARTY.md)。

## 克隆与首次构建

1. 安装 Git 和 Git LFS、Unreal Engine **5.3.2**、Visual Studio 2022 C++ 游戏开发工具和 Windows SDK。
2. 在短路径下克隆，例如先进入 `C:\` 或 `D:\` 的可写目录，避免机场资源的长文件名超出路径限制：

```powershell
git lfs install
git clone https://github.com/elninady-andy/TacticalDemo.git UEPack
Set-Location UEPack
git lfs pull
```

3. 核对仓库文件。此处的 Python 只用于校验，3.10 以上即可：

```powershell
python .\VerifyPackage.py --manifest RepositoryManifest.json
```

4. 在仓库根目录运行以下命令，UE 路径替换为本机位置：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\SetupPortable.ps1 -EngineRoot 'D:\Epic Games\UE_5.3' -SkipQwen
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\RunDemo.ps1
```

环境配置会写入本机 `LocalPaths.json`，此文件不提交到 GitHub。也可参考根目录的 `LocalPaths.example.json` 手动配置。之后打开 `TacticalDemo/TacticalDemo.uproject` 并点击 Play，可使用编队、选择、编辑、方案和航迹预览功能。

当前工程的地图、飞机和材质全部保留于 `TacticalDemo/Content`；无需重新导入。二进制 UE 资源和 FBX 由 Git LFS 保存。GitHub 的普通源码压缩包不作为此工程的交付方式，优先使用上述克隆与 `git lfs pull`；若内容文件只是以 `version https://git-lfs.github.com/spec/v1` 开头的小型文本，说明尚未取回实际资源。

## 可选本地 Qwen

不部署 Qwen 时，使用 `RunDemo.ps1` 启动场景；本地规则编队指令仍可使用。`StartDemo.cmd` 会先启动 Qwen，因此需先补齐模型和 Python 环境。

1. 从 [官方模型仓库](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct) 获取完整的 **Qwen2.5-0.5B-Instruct** 模型文件，放到 `QwenRuntime/model`。该目录中应直接包含 `config.json`、`model.safetensors` 和分词器等文件，不能再嵌套一层模型目录；保留模型自带许可。
2. 安装 Python **3.14 x64**，按本项目的精确版本锁定文件创建环境：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\SetupPortable.ps1 -EngineRoot 'D:\Epic Games\UE_5.3' -PythonExe 'C:\Python314\python.exe'
```

3. 双击 `TacticalDemo/StartDemo.cmd`。默认接口为 `http://127.0.0.1:8000/v1/chat/completions`，模型名 `qwen`，配置位于 `TacticalDemo/Config/Qwen.json`。

也可在 `LocalPaths.json` 中将 `QwenRuntimeRoot` 指向已部署且目录结构兼容的本地服务，再使用 `-SkipQwen` 构建 UE。接入其他兼容服务时应同时按实际情况配置 `Qwen.json`，并按该服务自身方式启动；当前客户端仅接受本机地址。

模型、venv 和启动日志均保留在本机。首次安装依赖需要可用的软件源，不要复制其他电脑的 venv。

## 仓库与完整迁移包

| 项目 | GitHub 源码仓库 | 完整可移动工程包 |
| --- | --- | --- |
| C++、脚本、配置、当前 Content | 包含；Content 使用 Git LFS | 包含实际文件 |
| UE 引擎及编译工具 | 另外安装 | 另外安装 |
| Qwen 模型权重 | 不上传，另行获取或配置已有服务 | 原迁移包包含 |
| Python venv、UE 缓存及构建中间文件 | 不上传 | 不迁移，目标机重建 |
| 运行 DLL | 不上传，目标机编译 | 当前迁移快照包含，兼容性仍需验证 |
| 个人方案 `Saved/Scenarios` | 不上传 | 当前迁移快照保留 |
| 文件完整性清单 | `RepositoryManifest.json` | `CURRENT_MANIFEST.json` |

`FILE_MANIFEST.json`、`CURRENT_MANIFEST.json` 和资源接入清单保留各自的历史或迁移用途。GitHub 克隆应显式使用 `RepositoryManifest.json` 校验；完整迁移包按照 [MIGRATION.md](../MIGRATION.md) 操作。不要因为克隆不含模型、DLL或个人方案而覆盖迁移清单。

已有个人方案需自行备份。换电脑继续使用时，可将备份的 JSON 复制到 `TacticalDemo/Saved/Scenarios`，或通过方案窗口导入；先检查同名文件，避免覆盖本机方案。保存或导入后仍需在界面中加载，才会改变当前场景。

## 日常维护

先关闭 UE，再运行 `TacticalDemo/Scripts/Build.ps1` 和 `Test.ps1`。测试报告、截图及日志写入本机 `Saved`；实际交互和渲染验证按 [OPERATIONS](../TacticalDemo/OPERATIONS.md) 中各功能的验收脚本执行。

提交前检查差异，确认没有本机路径、个人方案、模型权重、日志或缓存混入。修改已有 UE 资源应在编辑器内完成，不能直接移动文件破坏包引用。完成必要构建和验证后才有意更新对应交付清单。

MIT 许可仅适用于本项目原创代码、脚本和原创文档。地图、模型、贴图等第三方素材的原始许可和再分发授权仍须单独核实，不能因为仓库公开而将这些内容视为 MIT 授权；详见 [第三方资源许可](../THIRD_PARTY.md)。
