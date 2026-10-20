# 第三方组件与资源来源

此文件记录目前能从本地工程确认的信息，不替代各组件和素材的原始许可。仓库当前为私有项目，未为整个工程指定开源许可。

## Unreal Engine

工程基于 Epic Games Unreal Engine **5.3.2**。引擎、Visual Studio 和 Windows SDK 需在本机安装，不包含于本仓库。`/Engine/` 资源由对应 UE 安装提供。

## 导入的地图与飞机素材

| 内容 | 工程位置 | 本地可确认的来源与状态 |
| --- | --- | --- |
| 机场及布景 | `TacticalDemo/Content/VRAirport` | 用户已有工程资源；未发现完整的独立再分发授权记录 |
| J-20、J-10 | `TacticalDemo/Content/plane/J20`、`plane/J10` | 用户已有模型及贴图；未核实原始作者和再分发许可 |
| map3、J-20、F-35 及配套材质 | `Content/Maps/map3.fbx`、`Content/Map3`、`Content/plane/J20_Map3`、`Content/plane/F35`、`Content/Airfight` | 从用户提供的 Move 场景、FBX 和 UE 工程提取并接入；未核实原始素材许可 |
| 其他配套素材 | `Content/SoftOutline`、`Content/Fab` | 为现有工程资源或补齐依赖；目录名不能证明许可范围，授权待核实 |

具体资源与依赖见 [ResourceIndex](ResourceIndex/MAP_RESOURCES.md)。这些文件保留原包路径以维持运行依赖，不代表获得了向第三方公开分发的授权。公开仓库、制作公开下载包或采用开源许可证前，需先补齐素材来源和许可记录；不要将自编代码的许可套用于第三方素材。

## Qwen

- 本地模型标识：**Qwen/Qwen2.5-0.5B-Instruct**。
- 官方模型仓库：[Hugging Face 上的 Qwen2.5-0.5B-Instruct](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct)。
- 当前本地模型附带的 `README.md` 标明 **Apache-2.0**，并附带 `LICENSE`；原文见 [模型许可](https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct/blob/main/LICENSE)。
- GitHub 源码仓库不上传 `QwenRuntime/model`，也不上传 Python 虚拟环境。自行获取模型时请保留并遵守随模型提供的许可文件与说明。

`QwenRuntime/qwen_local_server.py` 为本地 HTTP 服务入口。Python 第三方依赖的锁定版本列在 `QwenRuntime/requirements-lock.txt`，安装时使用各软件包自身的许可；依赖代码不随本仓库复制。
