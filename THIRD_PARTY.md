# 第三方组件与资源来源

此文件记录目前能从本地工程确认的信息，不替代各组件和素材的原始许可。项目原创 C++ 代码、Python/PowerShell 脚本和原创文档采用根目录的 [MIT License](LICENSE)。

MIT 许可不包含 `TacticalDemo/Content` 中的任何美术资源（包括地图、模型、材质、贴图和 FBX），也不包含 Unreal Engine、Qwen 模型或其他第三方组件。第三方内容仍保留原有权利及原始许可；本项目没有对这些内容重新授权。

2026-10-30，项目所有者明确确认有权公开当前仓库的605项地图、飞机模型、材质和贴图资源，并要求公开完整仓库。该确认用于当前公开托管，不将第三方资源转换为MIT授权；具体作者及原始许可文件记录仍待补齐。

## Unreal Engine

工程基于 Epic Games Unreal Engine **5.3.2**。引擎、Visual Studio 和 Windows SDK 需在本机安装，不包含于本仓库。`/Engine/` 资源由对应 UE 安装提供。

## 导入的地图与飞机素材

| 内容 | 工程位置 | 本地可确认的来源与状态 |
| --- | --- | --- |
| 机场及布景 | `TacticalDemo/Content/VRAirport` | 用户已有工程资源；未发现完整的独立再分发授权记录 |
| J-20、J-10 | `TacticalDemo/Content/plane/J20`、`plane/J10` | 用户已有模型及贴图；未核实原始作者和再分发许可 |
| map3、J-20、F-35 及配套材质 | `Content/Maps/map3.fbx`、`Content/Map3`、`Content/plane/J20_Map3`、`Content/plane/F35`、`Content/Airfight` | 从用户提供的 Move 场景、FBX 和 UE 工程提取并接入；未核实原始素材许可 |
| 其他配套素材 | `Content/SoftOutline`、`Content/Fab` | 为现有工程资源或补齐依赖；目录名不能证明许可范围，授权待核实 |

具体资源与依赖见 [ResourceIndex](ResourceIndex/MAP_RESOURCES.md)。这些文件保留原包路径以维持运行依赖，原始素材来源和许可记录仍待补齐。复用或再分发第三方素材前，请遵循其原始许可或取得对应权利人的授权；不要将本项目代码的 MIT 许可套用于这些素材。

## Qwen

- 本地模型标识：**Qwen/Qwen2.5-0.5B-Instruct**。
- 官方模型仓库：[Hugging Face 上的 Qwen2.5-0.5B-Instruct](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct)。
- 当前本地模型附带的 `README.md` 标明 **Apache-2.0**，并附带 `LICENSE`；原文见 [模型许可](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct/blob/main/LICENSE)。
- GitHub 源码仓库不上传 `QwenRuntime/model`，也不上传 Python 虚拟环境。自行获取模型时请保留并遵守随模型提供的许可文件与说明。

`QwenRuntime/qwen_local_server.py` 为本地 HTTP 服务入口。Python 第三方依赖的锁定版本列在 `QwenRuntime/requirements-lock.txt`，安装时使用各软件包自身的许可；依赖代码不随本仓库复制。
