# 项目入口与目录地图（迁移包）

先读 PROJECT_STATUS.md 了解已完成工作，再读 MIGRATION.md 完成目标电脑配置。

GitHub源码克隆先读README.md与docs/GITHUB.md；完整迁移包按MIGRATION.md操作，两种交付使用不同文件清单。

| 目录/文件 | 用途 |
| --- | --- |
| TacticalDemo/TacticalDemo.uproject | UE5.3 C++工程入口 |
| TacticalDemo/Content/VRAirport/Maps/VRAirportMap.umap | 当前机场地图 |
| TacticalDemo/Content/Maps/Demo.umap | 基础演示地图 |
| TacticalDemo/Content/Maps/map3.umap | 可切换的 map3 山地场景，默认启动仍为机场 |
| TacticalDemo/Content/Maps/map3.fbx | 用户提供的 map3 源文件，保留未修改 |
| TacticalDemo/Content/Map3/ | map3 新增的9个环境网格与地形基础材质 |
| TacticalDemo/Content/plane/J20/ | J20网格、材质、两款贴图 |
| TacticalDemo/Content/plane/J10/ | J10网格、材质、两款贴图 |
| TacticalDemo/Content/plane/J20_Map3/、plane/F35/ | 从用户Move场景提取的两种独立飞机网格 |
| TacticalDemo/Content/Airfight/ | 上述飞机与山地的原始材质及贴图，保留包路径 |
| TacticalDemo/Content/VRAirport/ | 机场几何、布景、材质和贴图 |
| TacticalDemo/Content/SoftOutline/ | 机场材质的补齐依赖 |
| TacticalDemo/Config/EquipmentLibrary.json | 型号、网格、材质、贴图登记 |
| TacticalDemo/Config/SceneLibrary.json | 场景名称与地图映射 |
| TacticalDemo/Config/AirportScenario.json | 机场初始飞机配置 |
| TacticalDemo/Config/Map3Scenario.json | map3 地形上方的 J20_Map3 / F35 初始编队，各3架 |
| TacticalDemo/Config/Map3FlightPaths.json | 从源UE样条提取的两条闭合轨迹，手动播放预览 |
| TacticalDemo/Scripts/Map3/README.md | map3 导入范围、材质处理与维护说明 |
| TacticalDemo/Config/Qwen.json | 本地聊天接口 |
| TacticalDemo/Source/TacticalDemo/ | C++实现及Tests |
| TacticalDemo/Source/TacticalDemo/DemoNameplateWidget.cpp | 自动机型标牌、屏幕避让及飞行预览跟随 |
| TacticalDemo/Source/TacticalDemo/DemoGameModeNameplates.cpp | 标牌实际运行验收，与界面绘制代码分离 |
| TacticalDemo/Scripts/TestNameplates.ps1 | 机场与map3机型标牌实际渲染、跟随与复位验收 |
| TacticalDemo/Source/TacticalDemo/FormationMembershipCommands.cpp | 本地自然语言创建编队、增减成员、改名与解散 |
| TacticalDemo/Source/TacticalDemo/HighlightCommands.cpp | 单架飞机发光高亮与关闭的本地指令 |
| TacticalDemo/Source/TacticalDemo/DemoSelection.cpp、DemoScenarioSelection.cpp | 临时选择高亮、多选和原子批量编辑/编队 |
| TacticalDemo/Source/TacticalDemo/DemoRouteWidget.cpp、DemoScenarioRoutes.cpp | 航点表格、单机/多选航迹保存与校验 |
| TacticalDemo/Scripts/TestSelectionAndRoutes.ps1 | 机场及map3多选、批量编辑、航迹表格和实际播放验收 |
| TacticalDemo/Content/Materials/M_AircraftHighlight.uasset | 保留原贴图的固定金色发光叠加材质 |
| TacticalDemo/Scripts/TestHighlight.ps1 | 机场与map3高亮开关、选中目标、撤销重做及恢复外观验收 |
| TacticalDemo/Scripts/TestFormationLanguage.ps1 | 机场及map3编队聊天流程的实际运行验收 |
| TacticalDemo/Saved/Scenarios/ | 已保存场景及编队方案 |
| TacticalDemo/OPERATIONS.md | 可用指令及操作说明 |
| ResourceIndex/MAP_RESOURCES.md | 每张地图的资源文件和运行时依赖 |
| ResourceIndex/AssetIndex.json、AssetIndex.csv | 全部资产类型、路径和直接依赖 |
| FILE_MANIFEST.json | 原始迁移包的SHA256快照，保留历史 |
| CURRENT_MANIFEST.json、VerifyPackage.py | 当前可迁移文件快照及只读校验入口 |
| RepositoryManifest.json、CreateRepositoryManifest.py | GitHub已跟踪源码和LFS资产快照；校验时显式指定该清单 |
| README.md、docs/GITHUB.md、THIRD_PARTY.md | 当前使用入口、克隆部署与第三方素材说明 |
| CreateCurrentManifest.py | 完成验收后有意更新当前快照；不用于掩盖缺失文件 |
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
