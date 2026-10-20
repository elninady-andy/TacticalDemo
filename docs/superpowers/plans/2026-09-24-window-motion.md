# Window and Motion Implementation Plan

> 在当前会话按 executing-plans 直接实施，用户已要求添加窗口并继续下一阶段。

**Goal:** 用窗口按钮控制 JSON 场景，并让编队按配置速度经过航点。
**Architecture:** 延用 Loader→Manager→UnitActor，新增纯 FormationHelper 与 UMG Widget。
**Tech Stack:** UE5.3.2 C++、UMG、Slate、UE Automation。
**Spec:** ../specs/2026-09-24-window-motion-design.md

## Global Constraints
- 不重新检查/安装环境，不复制素材，不接入 Qwen。
- 坐标米，边界转换厘米；兼容旧配置和事务式重载。
- 无碰撞占位模型必须被取景包含，UI 可用期间相机不能随鼠标乱转。

## Review Focus
重复航点与大时间步不死循环；暂停/重置/失败重载不破坏状态；UI 在 Clear/Reload 后不能持有失效 Actor；参数字段类型严格；相机覆盖路线。

## Task 1 配置与运动
- [ ] 添加 FormationMotionTests，先验证新字段非法输入当前未拒绝、运动未实现的失败。
- [ ] FormationHelper::Offset(Index,Spacing,Formation)，增加 Config Formation/Speed/Route。
- [ ] Unit ConfigureMotion(Config,Offset)、AdvanceMotion(Delta)、ResetMotion()；固定单位路线与初始位置。
- [ ] Manager Start/Pause/Reset/AdvanceSimulation、状态/时间；成功重载初始化，失败保持。
- [ ] 增量编译并运行新增测试及现有回归。

## Task 2 窗口
- [ ] DemoControlWidget 用 UMG 动态创建布局和按钮；绑定 Manager 公共入口。
- [ ] 路径输入、单位选择、状态/参数、按钮可用状态、错误提示。
- [ ] PlayerController 挂载面板，默认界面输入，右键飞行控制；GameMode 覆盖路线取景。
- [ ] 增加界面测试及真实渲染/交互检查。

## Task 3 交付
- [ ] 保留第一阶段示例，更新默认编队运动示例及运行脚本。
- [ ] 完整 UE 测试、界面截图、代码独立审查，修复阻塞。
- [ ] 更新 README、MAP、STATUS 和本记录。

## 执行记录
本项目不是 Git 仓库，继续在原目录实施，不创建无意义的 worktree。窗口选择已提出可选澄清，默认遵循原需求的 UE 内嵌 UMG。
