# map3 接入记录

## Move资源补齐（2026-10-16）

最新提供的Move目录同时包含与原件相同的map3.fbx及原UE5.3工程。已有map3不重建，只补齐飞机和材质。

- `InspectMoveSource.py` 在源工程中读取资源依赖、关卡实例及样条，输出至当前工程Saved/MoveImport。
- `ExtractAircraft.cpp` 用UE自带FBX SDK分别提取J20的12个部件和F35，取消世界位置并保留源旋转/缩放，输出两个局部坐标FBX。源FBX不修改。
- `PrepareMoveMaterials.py --source-project <源工程目录>` 复制严格依赖闭包中的28个原始材质/贴图；遇到同名不同内容拒绝覆盖。
- `ImportMoveAircraft.py` 通过UE原生导入器写入Content/plane/J20_Map3和plane/F35，按原槽名绑定Content/Airfight材质；只修改已有map3中的Mountain材质。已有飞机网格不重复导入。
- `TuneLighting.py` 调整map3已有灯光（太阳12、天空2），使深色机身可读；不新增或重建灯光。
- Map3Scenario登记J20_Map3/F35两支3机编队；机场初始配置与旧J20/J10保持原有行为。
- FlightPreview读取Map3FlightPaths.json，用临时网格显示源样条相对位移；原Actor、保存方案和撤销状态不移动。40秒为演示周期；不宣称完整恢复原Timeline曲线、游戏BP或真实飞行仿真。

原始材质包路径保留在Content/Airfight，不能直接从文件系统移动，否则会断引用。正常使用只需打开现有工程，源Move目录不是运行依赖。下文为10月15日仅有FBX时的历史处理范围。

当前运动验收入口：`Scripts/TestFlightPreview.ps1`，使用本机配置中的UE目录，实际渲染J20/F35预览及复位画面，校验保存快照、场景版本、选择跟随和运行日志。此脚本需先构建当前源码。

用户确认：加入可切换场景，启动仍进入机场；目前只有 FBX，先完成可用版本。

- 地图：`/Game/Maps/map3`，入口登记在 `Config/SceneLibrary.json`。
- 初始编队：`Config/Map3Scenario.json`，复用已有 J20/J10，各3架，保持静止。Z=231.54米，参考源文件中的 J20 高度，避免默认演示单位被地形遮挡。没有修改机场初始方案。
- 环境：105个静态网格实例。96个复用已有资源，并核对 FBX 本地边界和全局变换；9个新环境网格位于 `/Game/Map3/Geometry`。
- 两个地形材质缺少原始贴图，使用 `/Game/Map3/Materials/M_Map3Terrain`。既有机场的4个未分配材质槽在 map3 中显式绑定引擎默认材质；不修改原机场或网格。
- FBX 导出会省略部分未使用材质槽，因此复用网格的绑定读取原机场关卡实例，不能直接按 FBX 材质数组序号覆盖。`MaterialBindings.py` 处理这一差异。
- 源文件中旧飞机、天气球、云、彩虹及碰撞代理没有作为静态布景重复导入。飞机在 Play 时由 C++ 生成；F35、天气插件、原始动画和飞行路径不在本次实现范围。
- 保留原 FBX 的厘米单位、位置、朝向及比例；不会重新缩放其中远处的机场建筑。山地已作为静态网格导入，不是可雕刻的 UE Landscape。

## 维护与验证

正常使用直接打开已有地图，不需要重新导入。`ImportMap3.py` 在地图存在时拒绝覆盖，保护后续关卡编辑。

导入过程归档：`PrepareInputs.ps1` 使用本机 UE 自带 FBX SDK 和 VS C++ 编译 `ExtractScene.cpp`，只导出 `NewEnvironment.txt` 中9个环境网格，报告写入 `Saved/Map3Import`。UE 路径来自 `LocalPaths.json` 或 `-EngineRoot`，VS 自动发现或通过 `-VisualStudioRoot` 指定。`ImportMap3.py` 用于首次创建地图；不需要删除现有地图来运行它。

`RestoreBindings.py` 是本次补齐 map3 材质的维护脚本，只保存 map3。后续若已人工修改 map3 材质，不要无条件重跑它。

`VerifyMap3.py` 通过 UE Python 命令行检查105个对象、原始变换、材质绑定和机场默认配置。`TestSceneSwitch.ps1 -StartScene airport -TargetScene map3` 及反方向实测本地 Qwen、候选等待应用、实际加载和 GPU 截图。`ExportResourceIndex.py` 输出完整资源索引，验证无缺失后同步到包根 `ResourceIndex`。

`SceneLibrary.json` 的可选 `scenario` 字段指定 `Config` 目录内 JSON 文件名；省略时保留既有机场/基础演示选择规则。配置载入失败会报告错误，不能将缺失配置当作有效初始编队。

证据均位于 `Saved/Map3Import`。`Prior-20261014` 是材质修复前的切换记录；根目录的同名切换日志和截图在2026-10-15重新生成。完整状态以包根 `PROJECT_STATUS.md` 最新记录为准。
