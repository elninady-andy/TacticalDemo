# JSON Actor Demo Implementation Plan

> 执行方式：本任务直接实施，用户已明确要求“开始执行”。采用 executing-plans，验证结果写入 PROJECT_STATUS.md。

**Goal:** 建立能通过 JSON 修改数量、位置、阵营和模型的 UE5 C++ Demo。
**Architecture:** Loader 生成配置，Library 解析模型，Manager 事务式替换单位。GameMode 启动演示，按 R 或 Demo.Reload 重新加载。
**Tech Stack:** UE 5.3.2、C++、Json、JsonUtilities、UE Automation、编辑器 Python 仅用于生成地图和材质。
**Spec:** ../specs/2026-09-23-json-actor-demo-design.md

## Global Constraints
- 只实现第一阶段，保留素材，不部署 Qwen，不做真实仿真。
- 对外米、UE 内部厘米；每组 0～100，全场景最多 500。
- 资源路径来自注册表，坏配置保留旧场景。
- 不 Clean 或 Full Rebuild，实际编译和运行后才能声明成功。

## Review Focus
- 小数数量、重复 id、非数值位置应拒绝，不改变旧配置。
- 默认模型损坏应拒绝，新场景不能部分覆盖旧场景。
- 连续重载不能累积 Actor；空场景清除受管 Actor。
- 模型资源在 GC 后保持有效引用，材质支持红蓝参数。
- 相机要覆盖配置单位，不可只有日志正确而画面不可见。

## Task 1：工程与可编译环境
- [x] 创建 TacticalDemo.uproject、Game/Editor Target、模块 Build.cs 和模块入口。
- [x] 创建 Scripts/Build.ps1 调用现有 UE Build.bat。
- [x] 增量编译；若缺 SDK 或 MSVC 不兼容，只补齐所缺组件，保留原工具链。

## Task 2：配置加载与注册模型
- [x] 添加 Tests/ScenarioTests.cpp：合法配置成功；数量为小数、负数或过大失败；重复 id、坏位置和缺字段失败；空场景成功。
- [x] ScenarioConfig.h 定义 FDemoUnitConfig / FDemoScenarioConfig。
- [x] ScenarioConfigLoader 提供 Parse(Text, OutConfig, OutError) 和 Load(Path, OutConfig, OutError)，bool 返回值；用临时结果保证失败不修改输出。
- [x] EquipmentLibrary 提供 Load(Path, Error) 与 Resolve(ModelId, Type, Error)；保留静态网格强引用；指定项失效时回退，默认失效时明确错误。
- [x] 创建两个配置 JSON 与结构检查脚本。
- [x] 构建后运行 UE Automation RunTests TacticalDemo，报告测试数和失败明细。

## Task 3：场景生命周期与展示
- [x] DemoUnitActor 持有 UStaticMeshComponent、标识、阵营；初始化网格和动态阵营材质。
- [x] DemoScenarioManager::LoadScenario(ScenarioPath, LibraryPath) 先校验和解析，再创建候选 Actor，失败清理候选，成功替换受管数组。
- [x] 测试真实 World 中数量3→5、位置/颜色/模型、注册新 id、未知模型回退、坏输入保留、空场景和恢复。
- [x] GameMode 自动生成 Manager 与相机；控制器支持 R、Demo.Reload 和 Demo.Clear。
- [x] 用编辑器 Python 生成 Demo.umap 与参数化材质；默认地图和 GameMode 写入配置。
- [x] 编辑器实测，保存实际游戏截图，并运行截图相同场景的日志断言。

## Task 4：交付与审查
- [x] 生成 VS 工程文件，提供打开工程、构建、运行、重载的中文说明。
- [x] 运行结构检查、必要的增量编译和 UE 测试；不无理由重复全量编译。
- [x] 独立代码审查实际变更、修复重要问题并复测。
- [x] 更新 MAP/FRAMEWORK/STATUS，区分已验证和未验证，恢复有效演示配置。

## 执行记录
- Ruling: 当前是未初始化 Git 的新项目目录，直接在指定 TacticalDemo 子目录建立工程；不复制大型素材或建立多余 worktree。
- Ruling: 用户已明确开始执行，沿已同意方案在本任务实施，不再次重复请求阶段授权。
- Task 1: 工程创建并真实编译；缺失 SDK 已通过现有 VS 安装器补装 10.0.22621.0，无需更换现有 MSVC 14.44。
- Task 2: 配置/模型加载已实现；初始测试确实检测到未实现的合法输入失败，修复后通过。独立审查发现字符串转换与极值问题，先用回归测试复现再修复。
- Task 3: Actor 生成、候选场景替换、回退、快捷键/控制台重载、地图和材质已实现。真正渲染发现颜色节点未连接，资产验证脚本复现后修复。变化场景截图又发现相机忽略无碰撞模型，已用 World 测试复现并将 GetComponentsBoundingBox 改为包含无碰撞组件。
- Ruling: 为防止极端数值使渲染和相机计算溢出，坐标及生成位置限制 ±1,000,000 米，spacing 限制 (0,10000] 米；已同步设计与使用说明。
- Review: 独立只读审查提出两项 P2（字符串类型、极值范围），已修复并复核确认；未发现其余阻塞。当前环境不是 Git 仓库，代码保留在用户指定目录，没有分支合并或远程操作。
- Camera regression: 修正测试 World 未注册 PlayerController 的夹具问题后，重跑旧行为确认取景断言失败，再恢复包含无碰撞组件的实现进行完整回归。
- Runtime diagnostics: 本机 GPU trace 时间戳校准出现非致命 ensure；RunDemo 明确选择 CPU/帧/日志追踪，保留真实 GPU 渲染，并使验证脚本拒绝任何运行日志 Error/ensure。

