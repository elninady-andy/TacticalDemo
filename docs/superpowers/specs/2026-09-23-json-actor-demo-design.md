# JSON 驱动 UE Actor：第一阶段设计

状态：用户已明确要求开始执行；第一阶段已实现。实际验证结果见根目录 PROJECT_STATUS.md。

## 1. 目标与边界

在 E:/UE5Project/TacticalDemo 创建 UE 5.3.2 C++ 项目，完成本地 JSON → C++ 配置 → 可见 Actor 的验证。用户修改配置后，无需修改 C++ 或重新编译即可重新加载场景。

本阶段包含数量、位置、红蓝区分、模型注册表和默认模型回退。不接入 Qwen，不实现复杂 UI、运动、阵型、雷达、武器或数据链。现有素材保留，基础模型满足验收即可。

成功标准：真实编译通过，并在 UE 中实际验证生成、修改与错误处理。仅生成代码或通过 JSON 检查不算完成。

## 2. 项目与职责

- TacticalDemo.uproject、模块与 Game/Editor Target：标准 C++ 工程入口。
- Config/Scenario.json：用户可修改的场景描述。
- Config/EquipmentLibrary.json：模型注册与类型默认项。
- ScenarioConfig.h：场景、单位组的数据结构；不访问文件或生成 Actor。
- ScenarioConfigLoader.h/.cpp：文件读取、JSON 解析和字段校验；失败给出文件及字段信息。
- EquipmentLibrary.h/.cpp：模型注册表解析、资源解析与回退。
- DemoScenarioManager.h/.cpp：加载配置、创建新单位、替换旧场景和 Reload 入口。
- DemoUnitActor.h/.cpp：模型组件、实例标识、阵营和外观。
- 轻量 GameMode：启动 Manager，提供可看见全部测试单位的初始视角；不增加 Manager 层级。
- Content/Maps/Demo.umap：演示地图，包含照明与适合占位模型的场景。
- Content/Materials：支持颜色参数的公共材质，用于默认占位模型的红蓝区分。
- Scripts：必要的构建、结构检查及运行验证入口。

加载链路：GameMode → ScenarioManager → ConfigLoader / EquipmentLibrary → DemoUnitActor。
根目录 PROJECT_MAP.md 与 PROJECT_STATUS.md 作为后续任务入口；实施时更新真实文件清单。

## 3. 场景配置契约

```json
{
  "scenarioName": "Demo01",
  "units": [
    {
      "id": "RED01",
      "modelId": "aircraft_default",
      "type": "aircraft",
      "side": "red",
      "count": 3,
      "position": { "x": 0, "y": 0, "z": 10 },
      "spacing": 10
    }
  ]
}
```

- scenarioName、units 和组内 id/modelId/type/side/count/position 必填。
- id 为非空且组间唯一的字符串；运行时实例使用“组 id + 序号”区分。
- type 支持 aircraft、vehicle、radar、target；side 支持 red、blue。
- count 必须为 0～100 的整数，全场景最多 500 个实例；0 表示本组不生成。
- position 的三个分量必须是有限数值且各自在 ±1,000,000 米内；对外单位米，在生成时统一转为 UE 厘米。坐标遵循 UE 的 X/Y/Z，Z 向上。
- spacing 可选，默认 10 米，必须为有限正数且不超过 10,000 米，所有生成位置也必须在上述范围内。组内第 i 个实例放在 position + (0, i × spacing, 0)，i 从 0 开始。本阶段只是避免重叠，不表示正式阵型。范围限制是在代码审查中针对极值导致渲染/相机溢出增加的实现约束。
- units 可以为空，成功加载后清空生成单位。
- 未实现字段（如 speed、route、formation）不执行，记录警告；不声称支持这些能力。

## 4. 模型注册表

```json
{
  "defaults": {
    "aircraft": "aircraft_default",
    "vehicle": "vehicle_default",
    "radar": "radar_default",
    "target": "target_default"
  },
  "models": {
    "aircraft_default": { "type": "aircraft", "kind": "staticMesh", "asset": "/Engine/BasicShapes/Cone.Cone" },
    "vehicle_default": { "type": "vehicle", "kind": "staticMesh", "asset": "/Engine/BasicShapes/Cube.Cube" },
    "radar_default": { "type": "radar", "kind": "staticMesh", "asset": "/Engine/BasicShapes/Cylinder.Cylinder" },
    "target_default": { "type": "target", "kind": "staticMesh", "asset": "/Engine/BasicShapes/Sphere.Sphere" }
  }
}
```

模型资源路径只来自注册表。kind 第一版支持 staticMesh，资源必须能加载成静态网格。未来可在此处增加 Blueprint 资源适配，场景 JSON 仍只引用 modelId；本阶段不承诺 Blueprint 加载已实现。

指定 modelId 不存在、类型不匹配或资源不可加载时，尝试该 type 的默认模型并记录警告。默认项自身不可用时拒绝本次加载，保留原场景并报告修复注册表的具体字段，避免生成不可见单位或偷偷硬编码资源路径。

占位模型统一通过材质区分红蓝。未来导入真实模型时需适配其材质，不把本阶段着色能力等同于任意模型材质兼容。

## 5. 加载与错误处理

启动游戏时读取项目 Config 目录中的两个 JSON；提供游戏控制台命令 Demo.Reload，再次读取场景及注册表。默认路径由项目路径计算，不绑定机器盘符。不实现自动监控文件。

先解析并校验完整输入、解析资源，再暂存新生成的 Actor。全部成功后销毁旧的受管 Actor 并启用新场景。任何阶段失败都清理本次新建实例，保留原有场景。只操作 Manager 管理的 Actor，不清理地图内其他对象。

缺文件、非法 JSON、重复 id、非法数值、数量超限或无法使用的默认模型都输出明确日志；重新加载失败后仍可修正配置再试。多次 Reload 不累积重复 Actor。

## 6. 演示操作

打开 TacticalDemo.uproject，进入 Demo 地图并运行。初始 JSON 生成三个红方圆锥和两个蓝方方块。修改 count、side、position 或 modelId，保存文件后按 R 或执行 Demo.Reload，即可看到变化。F 键重新取景；WASD 和鼠标控制观察视角。

交付包含中文操作说明和可重复的构建命令。当前目标是编辑器内运行，打包发布与打包后的配置路径另行处理。

## 7. 验证与环境处理

沿用 PROJECT_STATUS.md 中的环境检查。缺失的 Windows SDK 已通过现有 VS 安装器补齐，MSVC 14.44 与 UE 5.3.2 的组合已经通过项目编译。后续只针对新阻塞检查，不重装 UE、Visual Studio 或 Qwen。

Level 1：关键文件存在、JSON 可解析、字段契约一致、默认模型注册完整、文档状态一致。

Level 2：使用现有 UE 的 Build.bat 增量构建 TacticalDemoEditor Win64 Development，确认 UHT、编译和链接成功。不 Clean 或 Full Rebuild。

Level 3：运行 UE，验证以下案例并保留日志；画面相关结论需实际画面证据，不能仅凭日志认定。

1. 初始配置生成三个可见红方 Actor，位置和数量符合配置。
2. 修改 count 为 5 后 Reload，数量准确且没有旧实例残留。
3. 修改 side、position、modelId 后，颜色、位置、形状对应变化。
4. 在注册表添加新模型 id 并引用，无需编译即可加载。
5. 未知 modelId 或失效资源回退到该类型默认占位模型。
6. 非法 JSON、非法字段或失效默认模型不会破坏已经加载的场景。
7. 空 units 清空受管单位；恢复配置后可再次加载。

验证后恢复有效演示配置。未执行或失败的步骤如实写入 PROJECT_STATUS.md；只有本阶段验收通过才进入 Qwen 等下一阶段。
