# 窗口与对话实施计划

Goal: 在现有 UE 地图内提供可操作聊天窗口，连接已有本地 Qwen。
Architecture: UMG 窗口负责交互；ChatProtocol 负责严格解析；UE HTTP 异步请求；现有 Manager 负责事务应用。
Spec: ../specs/2026-09-24-window-chat-design.md

- [x] 协议测试先行，验证纯聊天、候选与错误响应，再实现解析。
- [x] 创建 UMG 窗口及 HTTP 生命周期，接入控制器。
- [x] 增量编译、自动化测试、真实截图、修复问题。
- [x] 更新 README、MAP、STATUS，注明本地模型实际验证范围。

原 window-motion 计划暂停，本轮不执行。用户已授权直接实施，不重复环境扫描或安装。
