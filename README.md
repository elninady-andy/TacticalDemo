# TacticalDemo

基于 **Unreal Engine 5.3.2、C++ 和本地 Qwen** 的飞机场景与编队编辑演示。通过自然语言或窗口编辑配置，在三维场景中查看结果；所有聊天修改先生成候选，手动应用后才改变场景，并支持撤销。

这是可编辑的 UE 源工程，需安装 UE 和编译工具后运行；尚未验证独立 exe 打包。

## 已有功能

- 机场、基础演示和 map3 三张场景，默认进入机场。机场使用 J-20/J-10，map3 使用 J-20/F-35。
- 点击选中、Ctrl 多选、临时高亮、批量位置与朝向编辑，以及拖动、撤销和重做。
- 编队创建、成员管理、改名、解散和横排、纵列、楔形排列。常用编队自然语言指令在本地解析，无需 Qwen。
- 方案保存、加载、JSON 导入导出；单机或多选航点列表编辑，可手动播放轨迹预览并停止复位。
- 飞机发光高亮和自动机型标牌；J-20、J-10 标牌文字为粉红色。
- 可选本地 Qwen 聊天接口，用于其他自然语言场景指令。模型仅输出受校验的配置，不直接执行代码。

飞机默认静止。轨迹播放是演示预览，不包含气动力、碰撞、避障或完整飞行仿真。

## 首次运行

需要 Windows、UE **5.3.2**、Visual Studio 2022 的“使用 C++ 的游戏开发”工具及 Windows SDK。GitHub 克隆还需 Git LFS；完整步骤见 [GitHub 使用与交付说明](docs/GITHUB.md)。

在仓库根目录执行，`EngineRoot` 按本机安装位置填写：

```powershell
git lfs install
git lfs pull
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\SetupPortable.ps1 -EngineRoot 'D:\Epic Games\UE_5.3' -SkipQwen
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\RunDemo.ps1
```

也可打开 `TacticalDemo/TacticalDemo.uproject`，在编辑器中点击 Play。运行时飞机由 C++ 根据 JSON 生成，仅打开地图时不会出现可编辑编队。

`-SkipQwen` 可先完成场景、编队和航迹功能的构建与使用。GitHub 仓库不包含模型权重和 Python 虚拟环境；需要聊天时按 [Qwen 配置说明](docs/GITHUB.md#可选本地-qwen) 补齐。已配置 Qwen 后可使用 `TacticalDemo/StartDemo.cmd` 同时启动服务和演示。

## 目录与维护

| 位置 | 用途 |
| --- | --- |
| `TacticalDemo/Source/TacticalDemo` | 场景管理、指令解析、窗口、预览及自动化测试 |
| `TacticalDemo/Config` | 场景、设备库、地图登记与本地接口配置 |
| `TacticalDemo/Content` | 当前地图、飞机、材质和贴图，二进制资源由 Git LFS 管理 |
| `TacticalDemo/Scripts` | 环境配置、构建、测试和资源维护入口 |
| `QwenRuntime` | 本地服务源码与依赖锁定；模型及 venv 在本机补齐 |
| `ResourceIndex` | 地图资源清单与资产依赖索引 |
| `TacticalDemo/Saved/Scenarios` | 本机个人方案，运行时生成，不上传仓库 |

实际操作见 [OPERATIONS](TacticalDemo/OPERATIONS.md)，模块定位见 [PROJECT_MAP](PROJECT_MAP.md)，开发与验收记录见 [PROJECT_STATUS](PROJECT_STATUS.md)。`docs/superpowers` 和 `PROJECT_FRAMEWORK.md` 保留历史设计，尚未实施的计划不代表当前功能。

先关闭 UE，再运行构建或自动化测试：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\Build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\TacticalDemo\Scripts\Test.ps1
python .\VerifyPackage.py --manifest RepositoryManifest.json
```

测试报告写入本机 `TacticalDemo/Saved`，文件校验不替代编译和实际渲染验收。换电脑后应重新验证，不能将历史报告视为新机器已通过。

本机路径写入不受版本控制的 `LocalPaths.json`，模板见 `LocalPaths.example.json`。不要重建已有项目或在文件系统中移动 UE 资产；配置和当前 `Content` 为资源真源。

## 资源与许可

本仓库按私有项目托管，尚未对整个工程授予开源许可。机场、飞机和其他导入素材的再分发授权尚未核实；第三方资源与 Qwen 来源见 [THIRD_PARTY](THIRD_PARTY.md)。完整迁移包与 GitHub 源码交付的区别见 [交付说明](docs/GITHUB.md#仓库与完整迁移包)。
