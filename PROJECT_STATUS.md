# 项目状态

代码整理与GitHub源码交付（2026-10-20，本机验证）:
- 删除无引用的MyClass空模板；将标牌运行验收原样移至DemoGameModeNameplates.cpp，绘制文件移除按钮、截图、计时器等验收依赖。Gather先收集飞机编号，再单次遍历已生成单位，减少每帧逐架FindUnit查找；未引入跨重载缓存，不改变机型、粉红色文字、避让、选中及预览行为。备份在Saved/Cleanup/Before。
- Qwen服务清理CATIA身份、零件提示及旧completion编号；保持模型推理和协议路由。SetupPortable在-SkipQwen时不再强制要求模型目录，仍检查UE和完整项目资源；无SkipQwen时继续检查本地模型。
- 新根README统一最新功能入口，UE子目录README精简为导航。docs/GITHUB记录克隆/LFS/无模型构建与可选Qwen部署；THIRD_PARTY记录导入素材许可待核实，未对整个工程授予开源许可。GitHub按用户授权托管私有elninady-andy/TacticalDemo，完整605项Content使用LFS；权重、venv、DLL、缓存、LocalPaths与个人Saved不提交，不删除本地文件。
- 完整迁移快照CURRENT_MANIFEST仍在本地保留原含义；RepositoryManifest只记录Git已跟踪源码和实际LFS资源，使用VerifyPackage.py --manifest RepositoryManifest.json校验。历史FILE_MANIFEST及MoveIntegrationManifest不覆盖。
- Saved/Cleanup记录本轮证据：基线及整理后31项UE回归均通过（21成功、10带测试警告、0失败/未运行）；BuildFinal标准构建成功，机场/map3真实标牌/移动/复位验收通过，截图在Saved/Nameplates。Qwen纯Python9项、文件校验器5项通过；Windows PowerShell5.1及PowerShell7的无模型部署各6项通过。独立审查复现281字符资源路径导致源码清单生成失败，已按完整迁移校验器补齐长路径处理；发布前移除DefaultEngine.ini中未使用的Android调试令牌并通过Validate。对照625项基线，仅此配置有意变化，624项不变，见UnchangedResources.json；所有Content及个人方案保持原样。未验证独立exe或另一台电脑运行。

飞机头顶机型标牌（2026-10-20，本机完成）:
- 所有运行时飞机自动显示RPG式机型标牌：J20和J20_Map3为J-20，J10为J-10，F35为F-35；基础占位显示“飞机”，其他登记模型显示模型名称。名称来自实际解析成功的模型，资源回退不会显示缺失机型；换机型、加载方案自动刷新。
- 深色底、固定屏幕字号，J-20/J-10文字按用户要求为粉红色#FF80BF，其他机型白字；投影在网格顶部上方，邻近标签尝试上下和横向避让，带指示线。背后和视口外不绘制。界面层不参与点击命中，不改变飞机边界、材质、选择、方案或撤销记录。飞行预览跟随显示副本，原位置不重复，停止后复位。
- BuildFinal.log编译成功；Saved/Nameplates/RegressionFinal/index.json中31项UE回归通过（21成功、10带测试警告、0失败/未运行），覆盖名称、替换、默认回退、编辑跟随、飞行跟随/恢复和顶部/底部密集标签避让。独立审查指出顶部避让只向上导致重叠，已修复并复查。
- Scripts/TestNameplates.ps1在机场和map3实际验收6个绘制标签及正确机型、移动与停止恢复；截图在Saved/Nameplates各地图目录。Content、Config和用户Saved/Scenarios共625项SHA256未变，证据UnchangedResources.json。标牌仅覆盖方案生成的飞机，机场装饰性静态布景保持原状；未验证独立exe打包。

点击多选、批量编辑与航点列表（2026-10-19，本机完成）:
- 用户确认航迹采用“航点列表编辑，先完成可用版本”。点击飞机临时金色高亮，Ctrl＋点击增减多选，空白清空；最近选中为主选。在已选飞机上未拖动的单击恢复单选，拖动则整体移动当前多选；Esc或右键取消。选择高亮不改变快照、场景版本或撤销历史，取消选择仍保留手动保存的高亮。
- 单位页新增选择数量/主选编号、全选/清空、新名称输入及“用当前多选建立编队”。批量坐标以主选为基准平移，朝向采用共同角度差，保留队内相对位置和朝向差；一次应用/拖动/编队只产生一条撤销记录，同名新编队拒绝。过期拖动和过期窗口输入不能覆盖新编辑，换场景不会沿用旧Actor选择。
- 新增航迹页DemoRouteWidget，支持世界米坐标航点表、增点/删末点、时长、闭合循环、手动应用、恢复默认和草稿连线。实例可选flightRoute保存points/periodSeconds/closed；旧方案兼容。一次可应用多选，每架按相对位置偏移，其他飞机不变；完整校验后一次提交，可撤销并随方案保存。
- 航迹草稿记录选择顺序、选择版本、场景版本和实际文本基线。选择或场景变化时，未应用文本保留但禁止误应用，需重新读取；未创建Slate时SetText不触发事件的问题已通过实际文本比较修复，WidgetDraftIsolation保持原测试验证。
- 飞行预览优先单机航迹、原编组航迹、map3源航迹。机场/Demo也可播放自定义航迹；开始从原位置飞向首点，不瞬移。开放时长包含接入段，到末点保持；闭合时长为每圈，接入段只走一次且另计时间。原Actor/快照不动，停止恢复编辑状态。预览副本每帧同步选中高亮，切换单位列表后发光目标正确变化；所有地图开放播放入口。
- Build.log标准构建成功。增加源文件时遇到UBT旧文件列表导致验收函数链接缺失，刷新构建列表后标准Build.ps1复验通过；修正UMG局部Slot遮蔽警告。Saved/MultiSelection/RegressionRelease/index.json最终30项UE测试通过（21成功、9带测试警告、0失败/未运行）。初次失败证据Red、Regression、RegressionFinal保留，不能作为最终通过依据。
- Scripts/TestSelectionAndRoutes.ps1在机场和map3实际执行拾取射线、两机多选发光、直接编队、批量编辑及撤销重做、航点表格、隔离验收方案保存和实际播放。日志含MULTI_SELECTION_RUNTIME_VERIFIED；Selected、BatchEditor、RouteEditor、RouteMoving截图已检查。根据截图将编队创建、航迹时长和应用按钮移到直接可见处。保存验收副本在Saved/MultiSelection，未写入用户Scenarios。
- 独立审查提出机场播放按钮仍隐藏、播放副本高亮不随选择变化两项问题，已修复并复查。原Content、Config和用户Saved/Scenarios对照上一版CURRENT_MANIFEST共625项SHA256未变，证据UnchangedResources.json；原map3源轨迹配置保留。交接文档与当前迁移清单同步，历史FILE_MANIFEST及MoveIntegrationManifest不改。未验证独立exe打包、真实飞行物理或三维拖动航点。

飞机发光高亮（2026-10-19，本机完成）:
- 接续上次额度暂停的内容：此前只保存了高亮字段、指令解析和失败测试；本轮补齐聊天手动应用、材质生成、持久化和验证。支持按完整飞机实例编号或“选中的飞机”逐架开启/关闭固定金色发光；“高亮帮助”提供用法，无需Qwen连接。未增加聊天改色或其他特效类型。
- 新材质Content/Materials/M_AircraftHighlight采用加法、Unlit、Fresnel叠加，原飞机材质和贴图不被替换。HighlightMaterialPath在DefaultGame.ini配置；材质缺失时拒绝整个事务，原场景及撤销缓存保留。高亮不改变飞机数量、型号、阵营、位置、朝向和编队。
- schemaVersion2实例可选布尔highlight，缺省关闭，仅飞机可开启；旧方案兼容。候选由ApplyHighlightCandidate限制只能改变该字段，仍需手动应用，支持撤销重做、取消、版本过期检查和方案保存恢复。普通聊天局部修改保留高亮；map3轨迹副本继承overlay，应用编辑时停止预览。
- 修复合并编译中ScenarioConfigLoader与EquipmentLibrary匿名ReadString重名，场景辅助函数改为ReadScenarioString/ReadScenarioNumber。独立审查发现“高亮编队/发光编队”被特效关键词拦截，NameRoutingRed先复现5项失败；编队与高亮路由现共享IsNameMention边界，覆盖创建、查询、排列、改名、问句、感叹号，以及字面名为“高亮”时真实复合操作拒绝。复查通过。
- 最终Build.log构建成功。Saved/Highlight/RegressionFinal/index.json中26项UE测试通过（19成功、7带警告、0失败/未运行）；警告来自既有6项无WorldContext测试及新增缺失高亮材质的拒绝用例。覆盖单架目标、非法选择/数据、权限隔离、原材质与位置保留、撤销、保存恢复、取消、过期候选和编队兼容。
- Scripts/TestHighlight.ps1在机场和map3实际渲染通过：Single/BothModels/Restored截图已检查，J20/J10/J20_Map3/F35均有可见金色发光，关闭恢复原外观；map3预览的两个高亮副本与保存状态隔离通过。日志及截图在Saved/Highlight/airport和map3；HIGHLIGHT_RUNTIME_VERIFIED记录单架、所选飞机、撤销重做与材质恢复。
- ExportResourceIndex现读取DefaultGame.ini动态材质路径，根ResourceIndex更新为604资产、3地图、缺失项目依赖0。迁移预检检查高亮材质，PowerShell5.1与7各7个隔离用例通过（新增缺失高亮材质）；真实Validate通过，报告Saved/PortableValidation/61e751cd39e44e9b807c434a66d4e441及b49aa56ef80e49cfbd06f245181b9872。
- 原Content、场景/设备配置及用户保存方案对照上轮CURRENT_MANIFEST共623项SHA256未变；有意修改DefaultGame.ini并新增1个材质。证据Saved/Highlight/UnchangedResources.json。操作说明、接手入口和当前迁移清单已同步，历史FILE_MANIFEST与MoveIntegrationManifest保留。未验证独立exe或另一台电脑运行。

自然语言创建与管理编队（2026-10-16，本机完成）:
- 用户本轮优先要求“把红方飞机编成编队3”。补齐创建、加入/移出成员、改名、解散；保留已有查询、排列、平移、高度和朝向。新入口FormationMembershipCommands在聊天发送时本地解析，无需Qwen连接；其他场景指令继续走原有流程。
- 支持阵营、准确机型（F35/F-35归一，J20与J20_Map3区分）、实际实例/组编号，以及已有编队成员选择；编号混合选择必须全部有效。重复创建、未知成员/目标、无实际变化及复合含糊指令不给修改候选。转入其他编队会提示数量；每架飞机保持单一归属。解散只清除归属，不删除飞机。
- 新成员管理候选与原排列候选分开校验：前者只允许formation字段变化，后者只允许位置/朝向变化。聊天修改仍手动应用，可撤销重做；取消和版本过期候选不能覆盖新编辑。成员管理不动飞机位置、朝向、型号、数量或地图；初始机场、静止默认值和轨迹预览逻辑不变。
- 左侧提示新增创建示例；“编队帮助”“查看飞机编号”提供离线用法和真实成员编号。OPERATIONS已更新完整操作、保存方案及限制。复杂复合操作请逐条发送；本轮未增加自然语言飞行控制。
- 先复现创建指令无候选：Saved/FormationLanguage/Red为失败基线。最终编译通过Build.log；Regression/index.json中24项UE测试通过（18成功、6既有警告，0失败/未运行），覆盖成员解析、跨阵营选择、候选权限、手动应用、撤销重做、取消、过期候选及原功能回归。
- 独立审查发现合法名称中的“和/与”及“当前/所有”前缀会干扰成员分词。NamesRed先复现8个失败，再改为完整成员标识优先匹配；回归覆盖和平编队、当前编队、所有编队、与风同行及混合编号选择。
- Scripts/TestFormationLanguage.ps1已在本机机场和map3实际运行通过；两份Runtime.log均含FORMATION_LANGUAGE_RUNTIME_VERIFIED且无Error/ensure。截图CreationCandidate.png与Arranged.png已检查，创建3架红方、保留6架总数、再排列V形及撤销重做成立；证据在Saved/FormationLanguage/airport与map3。
- Content、Config和用户Saved/Scenarios对照上一版CURRENT_MANIFEST共624项SHA256未变，证据UnchangedResources.json。本次有意更新源码、运行DLL、操作说明和当前迁移清单；原FILE_MANIFEST与MoveIntegrationManifest保留历史。仍未验证独立exe打包或另一台机器运行。

演示版交付预检收尾（2026-10-16）:
- 本轮按现有地图/编队/轨迹演示版继续收尾；历史路线中的语音、图像部署、约束布置、微调，以及完整源游戏BP不在“演示版完成”范围。此前1–3小时估计仅指收尾和迁移检查，不是整个历史路线工期。
- 实际复现Validate.ps1在Windows PowerShell5.1下因Double.IsFinite不存在而退出1。改用兼容的NaN/Infinity检查，并以UTF8读取JSON。预检覆盖SceneLibrary登记的3张地图、3份初始方案（合计23个配置单位）、设备库直接引用的网格/材质/贴图、Map3FlightPaths配置文件；仍由UE负责完整类型、资产依赖和运行时验证。
- SetupPortable -CheckOnly接入上述预检，避免缺失map3或F35时仍报告目录可用。TestPortableValidation.ps1使用独立空资产夹具，PowerShell5.1与7分别通过6例：完整、缺map3、缺轨迹配置、缺F35、map3数量无效、还原。真实项目在PowerShell5.1下Validate与SetupPortable -CheckOnly均通过。报告在Saved/PortableValidation/edc1ba39e3da4723a231349e87a85d0c及00b4adb61dc041d4964074398e962ae4。
- VerifyPackage优先使用新的CURRENT_MANIFEST，保留--manifest明确选择历史快照的能力；旧包无CURRENT时继续使用FILE_MANIFEST。新增缺失文件提示和Windows长路径支持。5个隔离Python测试通过，覆盖当前快照、显式原快照、同长度篡改、缺失文件和旧包兼容。
- CreateCurrentManifest.py记录当前可迁移源码、配置、全部Content、运行DLL、模型权重和已保存方案，不包含本机LocalPaths、venv、缓存与历史Saved日志。FILE_MANIFEST和MoveIntegrationManifest继续保留各自历史快照。当前清单是文件完整性基线，不等于已生成独立exe或已在另一台电脑运行。
- 本轮未修改C++、地图、飞机资产或保存方案，因此未重复上一轮23项UE回归和GPU场景切换；这些证据仍属于前一轮。后续若修改运行代码或资源，需重新做相应UE验证。

Move飞机与轨迹接入（2026-10-16，本机完成）:
- 用户补充Move目录，含map3.fbx及UE5.3原工程。FBX与现有Content/Maps/map3.fbx哈希相同；沿用已有map3，不重建项目、地图或旧飞机。读取原工程中的J20十二部件、F35、材质依赖和两条飞机样条，未整体迁入游戏/天气/导弹BP。
- 新飞机模型位于Content/plane/J20_Map3/mesh/SM_J20_Map3和Content/plane/F35/mesh/SM_F35；通过UE原生FBX导入，取消源世界位置并保留源旋转/比例。28个原始材质/贴图保持Content/Airfight包路径，两机型原槽位及Mountain原始地形材质已接回。Landscape1仍为基础材质；原机场4个空槽位仍按此前方案使用引擎默认材质。J20源FBX存在平滑组/退化切线导入Warning，实际渲染可用，未重制源网格。
- EquipmentLibrary新增J20_Map3/F35，保留旧J20/J10及默认映射。Map3Scenario为schemaVersion2，3架J20_Map3组成编队1、3架F35组成编队2；初始仍静止。机场入口、初始机型、聊天手动应用、编队撤销和已有保存方案保留。
- 用户本轮明确要求尝试运动，故加入可选FlightPreview，覆盖早期“只静止”的限制仅限手动预览。map3右侧播放/停止按钮及Esc停止；相机跟随所选单位所在编组，可在单位列表切换。临时网格移动，原Actor/保存快照/撤销记录不改；编辑、重载、清空或失效时结束，停止恢复原显示和视角。
- Map3FlightPaths.json包含两条源闭合样条各257点，逐坐标与导出结果一致。轨迹以当前组首架为锚点平移，偏移随航向旋转；采用源Timeline长度40秒作为等距离演示周期（约396.5/399m/s），不是完整还原未导出的Timeline曲线和BP行为。不包含碰撞、避障、气动力、导弹或动态方案进度保存；跨组自定义编队仍按原配置组分别取轨迹。
- 新构建成功：Saved/MoveImport/Build-Release.log。23项TacticalDemo回归通过：17成功、6既有预期警告、0失败/未运行，报告Regression-Release/index.json。修复了新轨迹闭环方向数组自引用追加断言；测试World采用独立包名，避免与Map3Startup夹具冲突。初次失败日志保留，不能用作最终结果。
- 实际GPU运行通过：FlightRuntime-Release.log含FLIGHT_RUNTIME_MOVING、FLIGHT_FOLLOW_VERIFIED BLUE01_1、FLIGHT_RUNTIME_VERIFIED；运行无Error/ensure，J20/F35跟随及停止截图已检查。预览移动约2.32km后保存快照和场景版本仍相同，停止恢复原网格显示。维护验收入口Scripts/TestFlightPreview.ps1。
- 真实本地Qwen双向场景切换通过：机场→map3、map3→机场，候选等待手动应用且应用前快照不变，目的地图均生成6架正确飞机并完成新截图，进程退出0，日志无Error/ensure；证据MoveImport/airport-to-map3及map3-to-airport日志与截图。
- 地图复核通过：105个环境对象、96复用网格、9新增环境网格、默认机场及原变换/实例材质绑定正确。资源注册表603个资产、3地图、缺失项目依赖0，根ResourceIndex已同步。原迁移Content、7个原保存方案和机场/默认配置共572项SHA256一致；EquipmentLibrary属于本次有意修改，不列入未变集合。
- 本轮证据统一位于TacticalDemo/Saved/MoveImport；源依赖报告SourceProject.json、导入报告AircraftImport.json、MaterialMigration.json、MapVerification.json、OriginalResourcesVerification.json可追溯。FILE_MANIFEST仍为原包快照；本轮文件清单见ResourceIndex/MoveIntegrationManifest.json。未验证独立打包发布。

map3 Integration (2026-10-15，接续使用上限暂停后的收尾已完成):
- 用户确认“加入可切换场景，启动时仍进入机场”，且目前只有 `Content/Maps/map3.fbx`，先完成可用版本。已生成并登记 `/Game/Maps/map3`；EditorStartupMap、GameDefaultMap 和 StartDemo 启动入口仍为原机场。
- 新地图105个环境实例，复用96个已有网格，仅新增9个环境网格与1个地形基础材质。复用前核对源FBX网格边界，保存后核对位置、旋转、比例与新网格烘焙边界。原FBX保留；没有重建已有地图或飞机资源。
- 修复map3初始视角被地形挡住的问题：SceneLibrary支持可选scenario文件名，map3使用Map3Scenario.json，复用3架J20和3架J10，在Z=231.54米的山地上方静止展示。机场仍使用原AirportScenario.json。聊天修改手动应用、编队撤销和已有方案保持原有规则。
- 2026-10-15补齐复用网格的关卡实例材质。根因是原网格部分材质只在机场关卡实例上绑定，且FBX省略闲置槽位，不能直接按FBX材质数组索引覆盖。MaterialBindings.py读取原机场实际槽位，只将绑定写入map3。96个复用实例均校验通过，空绑定为0。
- 视觉限制：缺少配套贴图的Mountain和Landscape1使用基础地形材质；原机场本就未分配的4个槽位在map3中显式使用引擎默认材质。没有恢复原天气插件、F35行为或飞行动画；源地形作为静态网格接入。详细范围见TacticalDemo/Scripts/Map3/README.md。
- 本机新验证：2026-10-15的21项TacticalDemo回归通过（15成功、6带既有预期警告，0失败、0未运行）。本次C++变更已于2026-10-14编译通过，10-15未再修改C++，使用该DLL进行最终回归。StartupRed保留修复前单位高度/型号/数量失败证据。
- 真实本地Qwen与GPU双向切换通过：机场→map3、map3→机场，均验证候选生成前后场景不变、应用后地图加载、6架飞机存在、新截图生成、进程退出0；两个最终运行日志无Error/ensure。实机截图已检查。证据TacticalDemo/Saved/Map3Import/airport-to-map3.*、map3-to-airport.*，旧截图已归档Prior-20261014。
- UE资源注册表复核573个资产、3张地图，缺失项目依赖0，ResourceIndex的JSON/CSV/MAP_RESOURCES.md已更新。原迁移清单中的562个Content文件、7个原有保存方案及3个机场/设备配置共572项SHA256全部一致。FILE_MANIFEST.json继续保留原迁移包快照，不表示新增map3后的完整目录清单。
- 汇总证据：TacticalDemo/Saved/Map3Import/AssetVerification.json、OriginalResourcesVerification.json、Regression-20261015/index.json、Map3Acceptance.json；本次新增/修改文件哈希见ResourceIndex/Map3ChangeManifest.json。后续可补齐原始地形贴图；本轮未验证打包发布和动态仿真。

Build and Runtime Validation (2026-10-14，本机开发环境验证完成):
- 已确认Visual Studio Community 2022安装在E:/visual，MSVC工具链14.44.35229（目录14.44.35207）。首次验证仍因Windows SDK只有Catalogs/Redist而失败；随后通过VS Installer补装Microsoft.VisualStudio.Component.Windows11SDK.22621，安装退出0，windows.h和kernel32.lib现已存在。
- 本机Build.ps1增量构建成功，使用Windows SDK 10.0.22621.0；本次执行DLL链接与元数据写入，复用本机当日已生成的目标文件，未Full Rebuild。新DLL生成时间为本机2026-10-14 16:29。36个Source文件与迁移包清单一致，本轮未改源码。证据TacticalDemo/Saved/BuildValidation-20261014/Build-WithSDK.log；初次失败保留为BeforeSDK-Build.log，原DLL也已备份。
- 使用新构建DLL完成20项TacticalDemo自动化回归：14成功、6项带预期警告，0失败/0未运行；真实Qwen的Integration.CommandCatalog通过全部12类指令，Integration.LayoutQwen通过贴图/型号/阵营修改并保留6架单位的位置、朝向、编队。这两个集成测试各1项成功，带测试世界清理警告，0失败/0未运行。报告分别在BuildValidation-20261014/Regression、CommandCatalog、LayoutQwen。
- TestLayout.ps1 -Formation通过真实GPU两阶段验收：机场单机编辑、撤销/重做、拖动取消/提交、跨阵营楔形编队及独立编队2、保存/加载；然后从Demo地图重新启动并加载方案，准确恢复机场地图、6架飞机、型号/贴图及变换。FormationEdit.log含FORMATION_RUNTIME_VERIFIED和LAYOUT_RUNTIME_VERIFIED；FormationRestart.log含LAYOUT_RESTART_VERIFIED，两个日志均无Error/ensure，进程退出0。截图与日志归档在同一BuildValidation-20261014目录。
- 原有7个Saved/Scenarios方案SHA256均未改变；GPU验收另保存一个“阶段1验证方案-4CA6C36D”供复查。原布景TESTBOX空引用Warning仍保留。Qwen的本机共享依赖引用仍按LOCAL_SETUP.md记录。
- 结论：当前UE5.3.2静态场景编辑与本地Qwen验证Demo在本机可增量构建、运行及继续开发。此结论不包含打包发布/独立安装包、长时间稳定性、任意自然语言或尚未实现的动态仿真。此前Local Recovery中的编译阻碍已经解决，历史记录保留。

Local Recovery (2026-10-14，本机迁移修复收尾):
- 当前工程位于 E:/TacticalDemo-Portable-20261013/UEPack；LocalPaths.json 已登记本机 UE5.3.2：E:/UE5/UE_5.3。E:/UEPack 为指向同一工程的目录联接，方便短路径访问，不是第二份工程。机器路径只写本机配置，没有恢复旧机器源码硬编码。
- 2026-10-13 从 E:/TacticalDemo-Portable-20261013.zip 精确补回两张缺失的机场长文件名贴图；写入前后均匹配 FILE_MANIFEST 的 SHA256。开始运行前，以短路径核对原清单全部870个文件通过。Windows普通路径访问这两张贴图仍受长度限制，短路径可访问；UE从原目录实际加载两张贴图成功。启动后的Saved配置、日志及本次文档更新不再与原包快照相同，这是正常变化。
- 新建本工程 QwenRuntime/venv，模型与服务使用包内版本。依赖下载停滞后，用 local_shared_dependencies.pth 引用本机 E:/CodexTools/QwenRuntime/venv/Lib/site-packages；35项依赖与锁定清单匹配（torch为2.13.0+cpu），pip check通过。没有复制旧venv或修改共享服务；当前运行依赖该共享库目录仍存在。详见 LOCAL_SETUP.md。
- 2026-10-13本机新证据：TacticalDemo回归20项通过（14成功、6带预期警告），0失败/0未运行；真实 Integration.LayoutQwen 1项通过（测试世界清理警告），验证贴图、型号、阵营修改后仍保留6架飞机的独立位置、朝向与编队成员。报告位于 TacticalDemo/Saved/LocalRecovery-20261013/Automation/index.json、LayoutQwen/index.json。
- 2026-10-14再次执行真正的 StartDemo.cmd -Verify（Windows PowerShell5.1链路）通过：本机Qwen就绪、机场启动/重载、生成新截图、进程退出0，运行日志无Error/ensure。证据保存在 TacticalDemo/Saved/LocalRecovery-20261014/，含Demo.png、Runtime.log、StartDemoEntry.log、StartDemoEntry.err.log。原机场J20蓝图TESTBOX空引用Warning仍保留，未修改原布景蓝图。
- 源码重新编译尚未打通：2026-10-13 Build.ps1 报 Win64不是有效构建平台；本机未发现VS2022 C++工具，Windows SDK仅发现Catalogs而无完整开发组件。2026-10-14仍未发现VS安装发现工具。现有DLL与安装UE的BuildId均为27405482，本轮实际测试使用迁移包DLL，不代表新机器C++编译通过。继续修改C++前需补齐VS2022 C++工具链与Windows SDK；错误证据 BuildAttempt.log。
- 本次没有修改C++、游戏规则、地图布景、模型或已保存方案。飞机静止、编队手动应用/可撤销、不增加聊天改色的约束保持。

Portable Handoff (2026-10-13):
- 整理完整可编辑工程迁移目录，含Source/Config/Content、2张umap、已保存方案、验证证据、项目文档、原始素材归档，以及本地Qwen权重/服务/精确Python依赖清单。UE/VS安装与旧venv不复制；目标Windows电脑需配置UE5.3.2、VS2022 C++工具和Python3.14 x64。
- UE资产注册表检查全部562个资源的硬/软包依赖，缺失/Game资源为0。发现机场材质引用的SoftOutline依赖未导入，从jicc/Content/SoftOutline补齐10个原始文件，不修改已有地图、型号或贴图。
- ResourceIndex的JSON/CSV提供全部资源包路径、磁盘文件、类型、直接依赖；MAP_RESOURCES.md区分地图引用与C++/JSON动态生成的飞机资源。全部Game资源随Content迁移；Engine资源由目标UE提供。
- 迁移包脚本支持LocalPaths.json、显式参数及自动发现UE目录；Qwen默认包内相对目录，修复进程识别和含空格路径。保留本机原有启动脚本行为；加入AGENTS.md、PROJECT_MAP.md、MIGRATION.md供Codex接手。
- 在E:/UEPack迁移目录实际RunDemo -Verify通过，机场启动与重载成功，截图Saved/Evidence/Demo.png，日志无Error/ensure；所有迁移PowerShell脚本语法检查及Windows PowerShell5.1 SetupPortable -CheckOnly通过。
- Python依赖重建需首次联网；未在另一台电脑实测。不包含Codex聊天会话/账号、派生缓存、旧sln、崩溃转储或重复UE工程。机场贴图文件名长，目标解压根目录建议D:/UEPack。

Named Formation Chat (2026-10-13):
- 自然语言支持查询当前编队、查看成员、调用并选中已有编队、横排/横队、纵列/纵队、楔形/V形/V字形/人字形、整体XY移动、绝对世界高度和单机朝向批量设置。自定义名称与编号编队共用；仅操作当前场景已保存成员，重启后先加载方案。
- 本地规则解析这些指令，不依赖Qwen服务；其他聊天仍走原接口。修改生成候选后手动应用，不自动移动；保留型号、贴图、阵营、地图和成员，整个操作可撤销。排列与窗口共用计算逻辑；应用前校验场景版本和候选基础状态。
- 修复测试变量未声明、参数布尔变量未初始化、重叠名称误选、未知前后缀名称误选、无效重复参数漏检及普通对话被编队名称拦截。问号、常见查询表达可用；多编队或多操作组合拒绝。
- 最新编译成功，完整20项TacticalDemo回归通过：0失败/0未运行，6项预期拒绝/回退/测试世界清理警告。新增覆盖高度与朝向实际Actor应用、位置保留、完整撤销、名称边界和非法参数。证据 Saved/Evidence/FormationChatReleaseBuild.log、FormationChatReleaseTests.log；独立代码复查无剩余重要发现。
- DirectX 11真实渲染环境的FormationChat自动化复验通过：1成功，0警告/0失败/0未运行，进程退出0，日志无Error/ensure。证据 Saved/FormationChatGpuAutomation/index.json、Saved/Logs/FormationChatGpu.log；这次未新增人工鼠标窗口验收截图。操作说明更新于 TacticalDemo/OPERATIONS.md。

Floating Window (2026-10-12):
- 场景编辑窗口新增标题栏左键拖动、鼠标捕获与松开结束；排除输入框和关闭按钮。按父画布坐标换算鼠标位移，适配显示缩放，并限制窗口边界。关闭/重新打开保留本次运行位置；重启仍默认居中。
- 窗口拖动不修改场景数据，移动后的窗口仍阻挡场景鼠标操作。捕获丢失或关闭窗口结束拖动。
- 最新编译成功；19项回归通过（6项预期拒绝/测试世界清理警告），0失败/0未运行。新增 FloatingWindow 验证缩放、边界、捕获丢失和重开位置；先复现缺失拖动的失败，再实现修复。
- 实际GPU窗口调用鼠标事件处理验证标题拖动、输入/关闭排除、松开、重开位置及移动后的点击拦截，场景JSON与版本均未改变。FLOATING_RUNTIME_VERIFIED、最终日志无Error/ensure；截图 Saved/Evidence/FloatingWindow.png。证据 FloatingBuild.log、FloatingTests.log、FloatingRuntime.log 与 Saved/Logs/FloatingWindow.log。
- 首次运行的UE GPU追踪校准ensure发生在窗口拖动前；复验采用原有验证脚本的cpu/frame/log/bookmark追踪参数后通过，初始日志保存在 FloatingGpuTraceInitial.log。未修改模型、Qwen或编队行为。

Current Stage:
2026-10-12：第1阶段单位编辑与方案管理开发及验证完成。每架飞机可独立编辑位置和朝向，支持左键拖动、取消、撤销及重做。已新增自由编队与横排、纵列、楔形排列，成员可跨阵营和型号选择。新增方案窗口，保存地图、型号、贴图、独立位置、朝向与静态航迹，并支持加载、JSON导入和导出。J20/J10、机场、原有聊天及贴图功能保留；单位不自行移动，聊天不修改颜色。操作见 TacticalDemo/OPERATIONS.md。


Window Refinement (2026-10-12):
- 场景编辑窗口改为浅色背景、深色文字，单位、编队、方案库三个页签；单位输入采用两列，反馈与撤销/重做位于底部。编队页已实现自由勾选飞机、命名编队1/2或自定义名称，以及横排、纵列、楔形排列；保存成员后收起勾选区，切换页签保留未保存勾选。
- 编译成功，16项回归通过，GPU编辑及跨地图重启恢复复验通过；证据 Saved/Evidence/FormationUIBuild.log、FormationUITests.log、FormationUIRuntime.log、LayoutEdit.png。


Free Formation (2026-10-12):
- 编队独立于原有阵营/型号编组，以实例 formation 字段保存名称；旧JSON兼容。成员可跨阵营、型号选择，每架只有一个编队，转入时从旧编队移除。自定义名称1至32字符；取消所有勾选并保存可清空编队。
- 按当前实例顺序排列选中编队，首架放在XY起点；横排沿右向、纵列向后、楔形交替分配左右翼。方向绕世界Z轴，0度采用世界X前向，各架世界高度保留。楔形间距为每层后退/展开距离；不自动避障或贴地。
- 编辑历史改为完整配置快照，一次排列或成员修改可整体撤销/重做。无效成员、重复ID、越界坐标或非法参数在修改前拒绝。所有改动更新场景版本、使旧聊天候选失效。
- 编队页先保存勾选成员再排列；未保存的名称/成员改动会拒绝排列并提示。保存/导入/导出/跨地图重启恢复均保留编队信息。编队排列目前通过窗口操作，未新增按编队名称的聊天指令。
- 最新编译成功；18项回归通过（6项测试世界清理或预期拒绝警告），0失败/0未运行。包括混合红蓝3架楔形、其他3架保留、单次整体撤销、任意编队名、非法成员/越界事务拒绝、窗口页签草稿保留及方案恢复。
- 本地Qwen真实集成通过：编队1/2保存并重新加载后，换贴图、换型号、改阵营均保留6架各自编队/位置/朝向；Saved/LayoutQwenAutomation/index.json为1项成功带清理警告，0失败/0未运行。
- GPU窗口验证混合编队楔形与独立编队2，实际重启从另一地图恢复完整方案；Scripts/TestLayout.ps1 -Formation，证据 Saved/Evidence/FreeFormationBuild.log、FreeFormationTests.log、FreeFormationRuntime.log、FreeFormationQwen.log、FormationEdit.png、FormationRestart.png。窗口截图已检查排列按钮完整可见。
- 核心代码独立审查无可执行问题；后续成员区收起/草稿保留等界面改动通过回归和实际渲染复验。红灯证据 FreeFormationRedTests.log。

Layout Stage 1 (2026-10-12):
- 保留旧编组结构，增加 schemaVersion=2、mapAsset 和每组 instances（独立稳定ID、世界坐标米、绕Z轴yaw角度）。旧配置仍可加载，聊天数量、阵营、型号、贴图和航迹处理兼容。独立位置不再受无效的旧编组生成边界限制。
- ScenarioManager 维护当前场景状态与版本，人工编辑同步聊天上下文；撤销精确使用配置中的原值，避免角度转换导致保存文本变化。无移动的选中操作不增加版本。旧候选即使在编辑后撤销回相同JSON，也因版本不匹配而拒绝；等待中的请求和地图候选同样失效。
- 控制器按模型边界选择系统生成单位，拖动预览沿原高度水平面；松开提交，Esc或进入右键观察取消。预览不写入方案，一次手势作为一次撤销。可见编辑窗口、侧栏和顶部面板阻止点击穿透。
- 右侧增加“单位编辑与方案管理”，支持单架坐标/朝向编辑及方案列表。侧栏和弹窗支持滚动，按钮文字避免自动折行。朝向0°指模型原始方向；不新增聊天朝向指令，也不自动贴地，部署约束属于下一阶段。
- ScenarioPresetStore 以随机ID文件保存命名方案到 Saved/Scenarios，UTF-8写入临时文件后移动；名称重复拒绝，不以输入名称拼接路径。导出不覆盖文件；导入最多2MB且先校验资源，导入到库不立即改变场景。加载前校验型号、贴图与实际.umap资源，不将网格包当成地图。
- 不同地图的方案通过 OpenLevel 和 LayoutPreset 选项恢复。无地图字段的旧配置使用当前地图。原机场布景不参与单架编辑；数据保存为复用方案，不代表已建立训练样本或微调模型。
- C++最新编译成功；16项 TacticalDemo 回归全部通过（4项带预期拒绝/回退或测试世界清理警告），报告 Saved/Automation/index.json，日志 Saved/Evidence/LayoutReleaseBuild.log、LayoutReleaseTests.log。
- 实际本地Qwen集成通过：人工移动并旋转、保存并重新加载后，依次切换红方贴图、替换红方型号为J10、将全部飞机改为蓝方，六架飞机各自位置和朝向保持。报告 Saved/LayoutQwenAutomation/index.json（1项成功带清理警告，0失败、0未运行），脚本 Scripts/TestLayoutQwen.ps1。
- 实际GPU窗口已验证编辑、撤销、重做、拖动取消/提交、其他五架位置不变及保存加载；证据 Saved/Evidence/LayoutEdit.png、LayoutAcceptance.json、Saved/Logs/LayoutEdit.log。Scripts/TestLayout.ps1 最终复验通过：从另一地图重启后加载保存方案，准确恢复机场、六架飞机的位置、朝向及资源；日志包含 LAYOUT_RESTART_VERIFIED，无 Error/ensure，截图 Saved/Evidence/LayoutRestart.png。
- 独立代码审查发现并修复地图类型检查、实例边界检查、UI点击穿透等问题；复查无剩余重要发现。源代码修改前备份在 Backup/LayoutStage1-2026-10-12/Source；本目录无Git仓库。

Aircraft Textures, Parking and Readability (2026-10-09):
- 两个模型的四张 Texture2D 均能加载。原材质不变，新增 M_J20_TextureSwitch 与 M_J10_TextureSwitch 副本，将机身 BaseColor 接入 AircraftDiffuse 贴图参数；仅替换slot0，J10座舱slot1保留。EquipmentLibrary.json登记每机型贴图1/2与材质，AirportScenario.json默认textureId=1。
- 红方起点(550,-515,3.5744)m、蓝方(585,-515,4.0685)m，两组各3架，沿Y以35m间距排列。对机场原始地面三角形、建筑和飞机布景做检查，30个中心/角点落在上层铺装地面，无障碍AABB重叠，飞机底部与地面误差小于0.01cm。证据 Saved/Evidence/AirportPlacement.json 与 AirportPlacementPlan.png。
- 输入框17px深色文字、浅色实底，聚焦/只读状态也有明确颜色。移除原始Candidate JSON历史消息与“待应用配置”标签；窗口只展示简短说明、应用按钮和当前贴图编号。HTTP窗口测试确认发送请求不会自动修改场景，手动应用后才生效。
- 贴图指令仅接受update参数；拒绝替换整个场景/创建单位的回复。按用户明确机型补齐或纠正AI漏掉的模型筛选；接受“切换到第二款”。贴图请求剔除AI额外输出的型号、位置、数量等字段，无效资源在生成新Actor前拒绝。模型替换会清除旧贴图选择。
- C++真实编译成功。10项项目回归通过，覆盖贴图协议、四张贴图实际材质参数、座舱保留、无效贴图保留Actor、输入对比度和隐藏JSON。新增协议用例先观察到失败再修复。独立只读审查提出的三处范围问题已修复并复核。
- 真实Qwen四阶段切换按红方/蓝方/J20/J10分别选择贴图1/2，断言实际材质、模型、数量及位置。报告 Saved/TextureQwenAutomation/index.json，0失败/0未运行（测试世界清理警告）。实际GPU窗口用自然语言应用两种机型的贴图2，日志 Saved/Logs/TextureRuntime.log 有 TEXTURE_RUNTIME_VERIFIED、无Error/ensure，截图 Saved/Evidence/AircraftTexture2.png。
- 原机场 SoftOutline 描边依赖缺失与 J20 蓝图 TESTBOX 空引用警告仍存在；原有飞机布景不受聊天控制。此次完成的是已导入并登记的贴图切换，外部贴图文件运行时导入仍未实现。

Imported Aircraft and Airport Verified (2026-10-09):
- EquipmentLibrary.json 登记 J20/J10，scale=1、preserveMaterials=true；原始占位模型仍可选择。AirportScenario.json 配置机场初始6架飞机与30米间距。SceneLibrary.json 登记机场和原始 Demo 地图。
- 默认编辑器地图及 StartDemo.cmd 的游戏入口已切到机场，使用现有 DemoGameMode 和聊天 UI。地图切换候选点击应用后执行 OpenLevel，并恢复目的地图初始配置，不跨地图保存单位编辑。
- 修复 J10B 网格的旧材质路径与 02_-_Default 的旧机身贴图路径。地图游戏模式绑定到当前 C++ 控制逻辑。修改前 uasset/umap 备份在 TacticalDemo/Backup/ImportedAssets-2026-10-09。
- 首次真实 Qwen 模型替换产生截断/重复字段 JSON，严格校验拦截并保留场景。改用专用简短模型替换提示后，真实集成通过，未放宽无效 JSON 校验。
- 最新 C++ 编译成功；8项 TacticalDemo UE Automation 全部通过（2项带预期拒绝/回退警告），报告 Saved/Automation/index.json。
- Scripts/TestImportedQwen.ps1 通过：UE聊天窗口向真实本机 Qwen 发送红方换J20、蓝方换J10、切换机场；前两项从基础模型开始，实际应用并断言网格名称、scale=1、原始材质。报告 Saved/ImportedQwenAutomation/index.json：1项带测试世界清理警告成功，0失败、0未运行。
- 同一脚本在真实GPU游戏窗口从 Demo 地图发送机场指令，应用候选并完成地图跳转。日志 Saved/Logs/AirportSceneSwitch.log 有 SCENE_SWITCH_VERIFIED、6架实际模型且无 Error/ensure；截图 Saved/Evidence/AirportSceneSwitch.png。默认机场启动/重载真实渲染也通过，截图 Saved/Evidence/Demo.png。
- 已知原素材问题：机场缺少 /Game/SoftOutline/SoftOutline 描边函数，原有 J20S 蓝图 TESTBOX 空引用发出警告；这两项未修复。机场自带飞机为原有布景，不属于配置生成单位，不受聊天操作控制。外部文件运行时导入仍未实现。

Window and Chat Completed (2026-09-24):
- C++ 动态 UMG：左侧对话、中央三维场景、右侧本地模型设置/单位参数/场景按钮，顶部显示场景状态。
- 使用 UE HTTP 异步调用本机 OpenAI 兼容 `/v1/chat/completions`；端点仅允许 localhost/127.0.0.1，300 秒超时并支持取消。
- `ChatProtocol` 强制 reply/scenario 协议；场景候选经过现有严格 Schema 校验后才可手动应用，普通聊天不会修改场景。
- 候选写入 `Saved/Chat/GeneratedScenario.json`，不覆盖原始 `Config/Scenario.json`；加载原始配置、清空、重新取景均有按钮。
- Manager 保存当前成功应用的 JSON 快照，保证后续对话基于画面中的真实状态；失败回复、失败重载和取消均保留当前场景并清理失效候选。
- 默认鼠标操作 UI；按住右键进入观察模式并启用 WASD/鼠标，松开恢复 UI。旧 R/F 热键已移除，避免聊天输入触发场景操作。
- 增加 `StartDemo.cmd`、`Config/Qwen.json`、本地 HTTP 测试 fixture 及使用文档。

Window and Chat Verified (2026-09-24 10:17 Asia/Shanghai):
- Level 1 校验通过；增量编译通过，证据 `TacticalDemo/Saved/Evidence/WindowBuild.log`。
- 6 项 TacticalDemo Automation 通过（4 成功 + 2 带预期拒绝/回退警告），0 失败、0 未运行。新增协议和窗口按钮测试。
- 独立本地 HTTP fixture 往返测试通过：真实 POST、合法候选、应用为 2 个单位、非法响应保留场景、上下文使用当前已应用场景。该 fixture 明确不是 Qwen。
- 实际 GPU 渲染窗口通过，`Saved/Evidence/Demo.png` 显示中文聊天面板、场景和单位参数；运行日志无 Error/ensure。
- 独立代码审查发现并修复两项 P2：聊天上下文与画面可能不一致、取消/切场景后旧候选仍可应用；均有回归断言。

Qwen Interface Fixed and Verified (2026-09-24 14:51 Asia/Shanghai):
- 定位已有部署：`E:/CodexTools/QwenRuntime/qwen_local_server.py`、模型目录 `model`、接口 `http://127.0.0.1:8000/v1/chat/completions`、模型名 `qwen`。
- 502 根因是 Qwen 服务未监听且系统代理在 `127.0.0.1:10090`；`Config/Qwen.json` 已写入正确端点，`DefaultEngine.ini` 的 `HttpNoProxy` 明确排除 localhost。
- 首次真实生成超过原 120 秒。模型声明 BF16，但本机 11 代 Intel CPU 无原生 BF16；服务改用 float32 后同一真实请求由超时降为约 2.2 秒并返回 HTTP 200。
- 服务增加健康检查、单实例生成锁、取消连接容错和 256 token 上限；UE 超时调整为 300 秒。
- `Scripts/StartQwen.ps1` 负责幂等启动和健康等待；`StartDemo.cmd` 会先启动 Qwen 再打开 UE。
- `Integration.RealQwen` 从 UE HTTP 模块调用真实本地 Qwen，并经过 `ChatProtocol` 接受回复：2.04 秒，1 成功、0 警告、0 失败。证据 `Saved/RealQwenAutomation/index.json` 和 `Saved/Logs/RealQwen.log`。

Qwen Startup Regression Fixed (2026-09-24 15:05 Asia/Shanghai):
- 用户实际启动遇到 `Qwen stopped during startup`。当时推测为启动器生命周期问题，并确认重复启动产生多份模型；但测试仅使用 PowerShell 7，未覆盖真实双击入口，不能据此判定用户问题已解决。后续已确认 PowerShell 5.1 的 NoProxy 参数不兼容是健康检查始终失败的直接原因。
- `StartQwen.ps1` 现在使用跨进程 Mutex 串行化启动，获得锁后重新检查健康状态；健康接口是启动成功的唯一依据，不再依据启动器是否退出。已有模型进程加载时只等待，不再重复生成实例。
- 新增 `TestStartQwen.ps1`。修复前并发冷启动稳定复现四个进程并失败；修复后两个并发启动收敛为一组 launcher/child，随后重复启动不增加进程。
- 最终验证：并发冷启动与重复启动通过；UE → 真实 Qwen 通过；6 项 TacticalDemo 测试通过、0 失败；实际运行渲染通过且日志无 Error/ensure；健康状态 `ok`，仅一组服务（两个父子进程）。证据位于 `Saved/Evidence/StartQwenRegression.log`、`RealQwenAfterStartupFix.log`、`TestsAfterStartupFix.log`、`RuntimeAfterStartupFix.log`。

Actual Batch Entry Verified (2026-09-24 15:42 Asia/Shanghai):
- 当前启动问题最终验证（2026-09-24 15:42）：旧版本在 PowerShell 5.1 下复现两分钟超时（服务实际健康），直接原因为 `Invoke-RestMethod -NoProxy` 在 5.1 不存在且异常被吞掉。不是模型加载卡住；此前 7.x 测试遗漏了真实入口运行时。
- 健康检查改为兼容 5.1/7 的 HttpWebRequest，单次请求显式 Proxy=null；只处理连接异常，参数/解析错误直接报告。保留启动锁；进程检测限定实际 Python 服务及目标端口。
- StartDemo.cmd 的错误处理改为括号块，防止 `& exit` 提前结束；支持传递 `-Verify`，测试不再绕过批处理。
- Windows PowerShell 5.1 并发冷启动、重复启动通过：`Saved/Evidence/StartQwenPS51Green.log`；真实 `StartDemo.cmd -Verify` 完成 Qwen 就绪→UE 实际渲染→新截图→退出 0，证据 `StartDemoEntryTest.log`、`StartDemoEntry.log`；真实 UE→Qwen 对话 2.42 秒通过，0 警告/失败，证据 `RealQwenPS51Fix.log`。
- 15:45 再通过不带参数的 StartDemo.cmd 正常启动，确认 TacticalDemo 游戏窗口有有效窗口句柄且 Responding=True，Qwen /health=ok。窗口保留供用户使用。

Pending:
- 后续根据用户实际提供的资源格式接入涂装、模型及轨迹/弹道导入；当前已保存来源并支持场景 JSON 加载。

Completed:
- 建立 TacticalDemo/TacticalDemo.uproject 和可用 C++ 模块、Game/Editor Target；生成 TacticalDemo.sln。
- 实现 ScenarioConfig、ScenarioConfigLoader、EquipmentLibrary、DemoScenarioManager、DemoUnitActor 和 DemoGameMode/PlayerController。
- 实现严格 JSON 类型/范围校验、数量/位置/红蓝配置、模型注册和类型默认回退。
- 实现事务式重载：新场景全部成功才替换旧场景，错误输入和默认模型损坏保留原场景。
- 实现 R / Demo.Reload、Demo.Clear、F 重新取景、观察视角。
- 生成真正的 Demo.umap、M_Unit/M_Ground.uasset，基础形状直接引用 Engine 资源。
- 提供构建、资产生成/检查、自动化测试、实际渲染和配置变化验证脚本及中文 README。
- 更新 PROJECT_FRAMEWORK.md、PROJECT_MAP.md 和实施记录。现有素材及 Qwen 部署未改动。

Verified:
- Level 1：Scripts/Validate.ps1 通过，关键源文件、资产及配置存在，当前有效示例共 5 个单位。
- Level 2：TacticalDemoEditor Win64 Development 增量编译通过，UHT、C++、链接完成；证据 Saved/Evidence/Build.log。
- Level 3：4 项 UE Automation 测试通过（3 项成功 + 1 项带预期警告成功），0 失败、0 未运行。
  - ValidAndEmpty：有效和空场景配置。
  - RejectInvalidWithoutMutation：数字/布尔冒充字符串、非法数量、重复 id、错误位置、极值和总数上限。
  - LibraryStrictStrings：注册表数字 asset 被拒绝。
  - ReloadAndRollback：真实 World 中生成、米到厘米/间距、3→5、旧 Actor 清理、阵营材质、未知模型/失效资源回退、新模型注册、错误 JSON/极值/坏默认项保留旧场景、清空和恢复、无碰撞模型取景。
- 原场景真实渲染：3 个红方圆锥、2 个蓝方方块，颜色和形状可见；Saved/Evidence/Demo.png。
- 配置变化真实渲染：第一组改为 5 个蓝方圆锥、高度 12 米，全场景共 7 个单位且完整进入画面；Saved/Evidence/ChangedScenario.png。
- 两次渲染均观察到启动加载和 Demo.Reload 成功；最终脚本运行日志无 Error/ensure。
- VerifyScenarioChange.ps1 退出码 0；Scenario.json 已按字节恢复，SHA256 与备份相同。
- 独立代码审查提出的字符串类型和极值范围两项 P2 已修复并完成只读复核；后续实际画面发现的取景问题也已修复并加入回归。

Environment（后续未变化时不重复完整检查）:
- UE 5.3.2：E:/ue/UE_5.3。
- Visual Studio Community 2022 17.14.37710.0：E:/VISUAL。
- MSVC 工具目录 14.44.35207，编译器 14.44.35229；当前工程真实编译通过。
- Windows SDK 10.0.22621.0：C:/Program Files (x86)/Windows Kits/10/。原环境缺少开发组件，已于 2026-09-23 通过现有 VS 安装器补装。
- 使用 Engine/Build/BatchFiles/Build.bat 和 UE 自带 .NET 6.0.302；不使用本安装中不存在的 RunUBT.bat。

Failed:
- 当前无未解决的第一阶段验收失败。
- 已修复历史问题：SDK 缺失、材质颜色输入未连接、字符串隐式转换、数值极值、取景忽略无碰撞组件。测试 World 缺少控制器注册的夹具问题亦已修复；正确夹具已验证旧取景行为失败、新行为通过。

Known Issues:
- UE 5.3 会提示当前编译器较新；不影响本工程已完成的编译结果。
- 本机 GPU 性能追踪曾出现时间戳校准 ensure，来源为引擎 GPU trace；RunDemo 脚本选择 CPU/帧/日志追踪，真实 GPU 渲染保留。直接从编辑器运行时仍可能出现此非致命诊断告警。
- 当前只支持静态网格和占位模型公共材质；Blueprint 模型、真实模型材质适配、打包后的资源/配置部署尚未实现。
- Qwen 对话与 UMG 参数面板已实现；编队、运动和雷达尚未实现。
- 当前目录不是 Git 仓库，没有提交、推送或 PR。

Next:
- 双击 TacticalDemo/StartDemo.cmd 启动本地服务和游戏；使用窗口按钮加载配置和重新取景。
- 后续接入用户提供的场景、涂装、模型和轨迹/弹道来源；当前窗口先保存来源并加载场景 JSON。航迹继续静态显示，单位不会自行移动。

Last Validation:
2026-10-08：主项目重新编译成功，DLL 已更新。7项 UE Automation 通过，新增配置窗口打开/关闭、来源保存/恢复、有效场景 JSON 加载与无效文件保留场景断言。实际本机 Qwen 逐项验证12类标准操作全部通过（四类单位创建、数量、阵营、位置、高度、间距、模型、航迹设置和清除）；混合红蓝场景“飞机全部改为蓝方”通过。过程中修复未要求字段复制、遗漏源阵营筛选、把范围 all 写入数量字段、无匹配阵营时扩大修改范围等问题。真实集成报告 TacticalDemo/Saved/CommandCatalogAutomation/index.json 为0失败/0未运行，仅测试世界销毁警告。配置窗口GPU渲染通过，截图 TacticalDemo/Saved/Evidence/Resources.png，运行日志无 Error/ensure。涂装、外部模型文件、外部地图包和弹道模拟导入仍为预留入口，未宣称已实现。
2026-09-29：修复“移动/移到/挪到”未被识别为场景指令的问题，并补充移动提示示例。Qwen 对坐标修改偶尔返回 `set` 单元素数组，或把 `targetSide` 放入重复嵌套 `set`；协议在保持字段严格校验的前提下接受单元素对象数组并归一化已观察到的嵌套 selector。6 项 UE Automation 通过；真实 Qwen 集成（创建 3 架、修改为 5 架、移动至 X=20/Y=10 米并保持高度/间距）通过，报告 `TacticalDemo/Saved/RealQwenAutomation/index.json`，含测试世界清理预期警告；真实 GPU 启动渲染通过，截图 `TacticalDemo/Saved/Evidence/Demo.png`。
2026-09-28：修复真实 Qwen 漏回 `reply` 导致的失败。新增创建/修改参数协议：C++ 严格校验模型返回并生成完整场景，修改操作基于发送时快照合并，场景变化时自动丢弃旧候选。6 项 UE 自动化测试通过；真实 Qwen 三阶段（创建 3 架、改为 5 架、改蓝方并改高度 30 米）通过，报告 `TacticalDemo/Saved/RealQwenAutomation/index.json`（含测试世界销毁产生的预期警告）。
2026-09-24 15:45（Asia/Shanghai）。当前启动修复证据：TacticalDemo/Saved/Evidence/StartQwenPS51Green.log、StartDemoEntryTest.log、StartDemoEntry.log、RealQwenPS51Fix.log；真实模型测试结果：TacticalDemo/Saved/RealQwenAutomation/index.json。此前 C++ 回归结果保留在 Saved/Automation/index.json。
