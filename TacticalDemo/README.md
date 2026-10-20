# TacticalDemo UE 工程

项目介绍、环境配置和构建入口见上一级 [README](../README.md)，GitHub 克隆及 Git LFS 使用见 [交付说明](../docs/GITHUB.md)。本目录中的 `TacticalDemo.uproject` 是 UE **5.3.2** 工程入口。

默认进入机场；点击 Play 后由 C++ 按配置生成飞机。支持场景切换、自然语言编队管理、点击多选、批量编辑、航点列表、手动轨迹预览、高亮和头顶机型标牌。飞机默认静止，聊天修改需要手动应用。

- [OPERATIONS.md](OPERATIONS.md)：具体按钮、自然语言示例、保存方案及功能限制。
- [PROJECT_MAP.md](../PROJECT_MAP.md)：目录与核心模块定位。
- [PROJECT_STATUS.md](../PROJECT_STATUS.md)：开发记录和已执行的验证范围。
- [MIGRATION.md](../MIGRATION.md)：完整可移动工程包的跨电脑迁移。

GitHub 不包含个人 `Saved/Scenarios`、模型权重、运行 DLL 或缓存。首次使用请从根 README 的构建步骤开始；已有资源无需重新导入或创建。
