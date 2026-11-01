# Flutter 全域功能点测试用例清单与验证矩阵 (Test Inventory)

> **基准功能集**：[`01-feature-set.md`](./01-feature-set.md) 中的 70+ 个核心功能点（涵盖 14 大领域：运行期 8 领域 + 编译期 6 领域）  
> **检索范围**：`flutter_source/flutter` 全树（包含 `packages/flutter/test/`、`engine/src/flutter/` 与 `packages/flutter_tools/test/`）  
> **检索方式**：实名核对官方实际存在的测试文件与测试用例函数，建立 1:1 映射  
> **文档索引**：[`00_master_plan_and_methodology.md`](./00_master_plan_and_methodology.md) | [`00_compile_time_master_plan.md`](./00_compile_time_master_plan.md) | **上游**：[`01-feature-set.md`](./01-feature-set.md) | **下游**：[`03-roadmap.md`](./03-roadmap.md)

---

## 0. 测试资源与验证方法论

### 0.1 四类官方测试资源分类

| 类型 | 源码位置 | 测试形态 | 驱动方式 |
|---|---|---|---|
| **A. Framework 单元测试** | `packages/flutter/test/*/*_test.dart` | 纯 Dart 单元测试，基于 `flutter_test` / `test` 包 | `dart test` 命令行执行 |
| **B. Engine C++ 单元测试** | `engine/src/flutter/*/*unittests.cc` | 基于 GoogleTest (gtest) 的原生 C++ 单元测试 | 原生可执行二进制运行 |
| **C. Android Embedder 测试** | `engine/src/flutter/shell/platform/android/test/*Test.java` | 基于 JUnit / Robolectric 的 Android 宿主测试 | Gradle `test` 任务执行 |
| **D. Toolchain 工具链测试** | `packages/flutter_tools/test/*/*_test.dart` | 纯 Dart 单元测试，基于 FakeFileSystem、FakeProcessManager | `dart test` 命令行执行 |

### 0.2 NanoFlutter 仿真三阶测试矩阵

为了彻底保证复刻代码的真实性与稳定性，NanoFlutter 对每个功能点实施**三阶测试（Three-Tier Validation）**：
1. **Tier 1: 纯 Dart Headless 基础单测**：在无图形界面的终端环境下，直接验证算法核心逻辑、状态转移与边界断言（毫秒级执行）。
2. **Tier 2: 官方孪生差分比对测试 (Differential Test)**：
   * 输入完全相同的初始参数/组件树；
   * 断言两端的几何坐标、尺寸、脏节点标记集合完全一致：
     $$|\text{Size}_{\text{Official}} - \text{Size}_{\text{Nano}}| \le 0.001\text{px}, \quad |\text{Offset}_{\text{Official}} - \text{Offset}_{\text{Nano}}| \le 0.001\text{px}$$
3. **Tier 3: 故障注入与混沌测试 (Chaos / Negative Testing)**：
   * 故意传入破坏性参数（如负数约束、同级 Key 碰撞、在微任务制造死循环）；
   * 断言系统的异常抛出行为与官方保护机制完全对齐。

---

## 1. 线程并发与帧时钟调度（SCH: 1~6）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **SCH-01** | FML 消息循环与 TaskRunner | `engine/src/flutter/fml/message_loop_unittests.cc`<br>`fml/task_runner_unittests.cc` | `TEST(MessageLoop, Basic)`<br>`TEST(TaskRunner, PostTask)`<br>`TEST(TaskRunner, DelayedTask)` | `tests/unit/task_runner_test.dart`<br>通过 C++ 编译共享库并测试跨线程任务投递与超时唤醒。 |
| **SCH-02** | 硬件 Vsync 脉冲捕获与分发 | `engine/src/flutter/shell/platform/android/test/io/flutter/view/VsyncWaiterTest.java`<br>`tests/unit/engine_ffi_test.dart` | `testVsyncCallbackInvoked()`<br>`Vsync Heartbeat Dispatch from C++ TaskRunner to Dart` | `tests/unit/engine_ffi_test.dart`<br>通过 Dart FFI `NativeCallable` 接收高频 60Hz 脉冲并断言纳秒单调递增。 |
| **SCH-03** | SchedulerBinding 帧五阶段状态机 | `packages/flutter/test/scheduler/binding_test.dart` | `test('Frame phases transition correctly')`<br>`test('handleBeginFrame dispatches transient callbacks')`<br>`test('handleDrawFrame dispatches persistent callbacks')` | `tests/unit/scheduler_binding_test.dart`<br>断言回调在 `transient` 与 `persistent` 阶段间的先后严格顺序。 |
| **SCH-04** | Ticker 时钟驱动与启停 | `packages/flutter/test/scheduler/ticker_test.dart` | `test('Ticker starts and ticks with accurate duration')`<br>`test('Ticker stops when inactive')`<br>`test('Ticker mute behavior')` | `tests/unit/ticker_test.dart`<br>模拟时钟流逝，断言 Ticker 接收到的 `elapsed` 单调累加。 |
| **SCH-05** | 微任务清空屏障与批处理 | `packages/flutter/test/scheduler/binding_test.dart` | `test('Microtasks drain before drawFrame completes')`<br>`test('Microtask starvation prevention')` | `tests/unit/microtask_barrier_test.dart`<br>在帧回调中注入微任务，断言所有微任务必在排版前执行完毕。 |
| **SCH-06** | 优先级异步任务调度 | `packages/flutter/test/scheduler/priority_test.dart` | `test('High priority tasks execute before idle tasks')`<br>`test('Priority sorting invariant')` | `tests/unit/priority_queue_test.dart`<br>验证并发插入不同优先级任务时出队顺序严格有序。 |

---

## 2. 单次遍历盒模型与渲染管线微内核（LAY: 1~8）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **LAY-01** | BoxConstraints 约束代数计算 | `packages/flutter/test/rendering/box_test.dart` | `test('BoxConstraints tightness and looseness')`<br>`test('BoxConstraints.constrain dimensions')`<br>`test('BoxConstraints operators (+, -, /)')` | `tests/unit/box_constraints_test.dart`<br>覆盖紧、松、无界约束交叉运算的所有边界用例。 |
| **LAY-02** | 单次遍历排版法则 (One-pass) | `packages/flutter/test/rendering/box_test.dart`<br>`packages/flutter/test/rendering/layout_test.dart` | `test('Constraints go down, sizes go up')`<br>`test('Parent adopts child size without reflow')` | `tests/unit/layout_pipeline_test.dart`<br>单次向下遍历与尺寸反馈闭环，断言性能复杂度为严格 $O(N)$。 |
| **LAY-03** | PipelineOwner 脏节点流水线驱动 | `packages/flutter/test/rendering/pipeline_man_test.dart` | `test('PipelineOwner flushLayout sorts by depth')`<br>`test('Multiple nodes marked dirty layouts once')` | `tests/unit/pipeline_owner_test.dart`<br>批量标记脏节点，断言单帧内每个节点仅触发一次 `performLayout`。 |
| **LAY-04** | RelayoutBoundary 局部重排阻断 | `packages/flutter/test/rendering/relayout_boundary_test.dart` | `test('Tight constraints forms relayout boundary')`<br>`test('parentUsesSize=false isolates parent layout')`<br>`test('markNeedsLayout stops at boundary')` | `tests/unit/relayout_boundary_test.dart`<br>深层叶子标记重排时，断言父节点 `performLayout` 调用计数为 0。 |
| **LAY-05** | sizedByParent 与 dryLayout 预计算 | `packages/flutter/test/rendering/box_test.dart` | `test('sizedByParent triggers performResize only')`<br>`test('computeDryLayout produces accurate bounds')` | `tests/unit/dry_layout_test.dart`<br>无需真正排版直接计算期望尺寸并比对数值。 |
| **LAY-06** | 绝对几何坐标映射与矩阵变换 | `packages/flutter/test/rendering/box_test.dart` | `test('localToGlobal with translation and scale')`<br>`test('globalToLocal handles inverted matrices')` | `tests/unit/geometry_matrix_test.dart`<br>构造三层嵌套平移缩放变换，断言绝对全局坐标误差 $\le 0.0001$。 |
| **LAY-07** | RenderPadding 边距单子偏移定位 | `packages/flutter/test/rendering/shifted_box_test.dart` | `test('Padding reduces inner constraints accurately')`<br>`test('Padding clamps to max constraint')` | `tests/unit/render_padding_test.dart`<br>终端 ASCII 树导出测试，断言内缩偏移量。 |
| **LAY-08** | RenderPositionedBox 居中对齐排版 | `packages/flutter/test/rendering/shifted_box_test.dart` | `test('Center aligns child in loose constraints')`<br>`test('Fractional offset placement')` | `tests/unit/render_center_test.dart`<br>居中对齐几何计算与坐标偏移断言。 |

---

## 3. 弹性盒 Flex 模型、多子容器与滚动切片（FLX: 1~6）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **FLX-01** | 双向链表子节点持有拓扑 | `packages/flutter/test/rendering/container_render_object_mixin_test.dart` | `test('Child chain insertion, removal and reordering')`<br>`test('previousSibling and nextSibling invariants')` | `tests/unit/container_chain_test.dart`<br>测试动态增删节点时双向链表指针完整性。 |
| **FLX-02** | RenderFlex 主轴两轮空间分配 | `packages/flutter/test/rendering/flex_test.dart` | `test('Flex factors distribute remaining space')`<br>`test('Non-flex children measured first')`<br>`test('CrossAxisAlignment stretch and center')` | `tests/unit/render_flex_test.dart`<br>验证 $1:2:1$ 权重弹性盒排版结果与官方差分对齐。 |
| **FLX-03** | 弹性溢出检测 (Overflow Warning) | `packages/flutter/test/rendering/flex_overflow_test.dart` | `test('Flex detects overflow beyond max constraint')`<br>`test('Overflow delta calculation')` | `tests/unit/flex_overflow_test.dart`<br>注入超出约束尺寸的子节点，断言溢出标记与警告抛出。 |
| **FLX-04** | RenderStack 绝对定位与重叠排版 | `packages/flutter/test/rendering/stack_test.dart` | `test('Positioned children adopt Stack dimension')`<br>`test('Stack sizes to non-positioned children')` | `tests/unit/render_stack_test.dart`<br>验证层叠绘制顺序与绝对坐标对齐。 |
| **FLX-05** | RenderViewport 虚拟视口几何裁剪 | `packages/flutter/test/rendering/viewport_test.dart` | `test('Viewport clips children outside visible rect')`<br>`test('ViewportOffset translates child origin')` | `tests/unit/viewport_test.dart`<br>模拟滚动位移，断言视口裁剪矩形截断正确性。 |
| **FLX-06** | RenderSliver 虚拟化按需局部排版 | `packages/flutter/test/rendering/sliver_list_test.dart` | `test('SliverList builds visible range only')`<br>`test('Sliver garbage collection outside cache area')` | `tests/unit/sliver_virtualization_test.dart`<br>构建 1000 个元素列表，断言活跃 RenderBox 节点数 $< 20$。 |

---

## 4. 响应式三棵树状态机与 Reconciliation（WID: 1~7）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **WID-01** | Widget 不可变声明描述 | `packages/flutter/test/widgets/framework_test.dart` | `test('Widgets are immutable configurations')`<br>`test('Widget.key equality semantics')` | `tests/unit/widget_contract_test.dart`<br>验证同一配置多次实例化其哈希与对比行为。 |
| **WID-02** | Element 树形骨架持有与挂载 | `packages/flutter/test/widgets/element_test.dart` | `test('ComponentElement mounts children recursively')`<br>`test('RenderObjectElement manages RenderObject lifecycle')` | `tests/unit/element_tree_test.dart`<br>断言 Element 树成功驱动底层 RenderObject 树的构建。 |
| **WID-03** | Widget.canUpdate Reconciliation 核心 Diff | `packages/flutter/test/widgets/framework_test.dart` | `test('canUpdate matches same type and key')`<br>`test('Type mismatch unmounts old and mounts new')`<br>`test('Key change forces element recreation')` | `tests/unit/reconciliation_diff_test.dart`<br>孪生差分比对：列表项换位、替换与带 Key 复用命中率 100% 对齐。 |
| **WID-04** | BuildOwner 脏节点批量防抖重构 | `packages/flutter/test/widgets/build_owner_test.dart` | `test('Multiple setState in same frame batched once')`<br>`test('buildScope executes from shallow to deep')` | `tests/unit/build_owner_test.dart`<br>同一节点单帧连续 `setState` 10 次，断言 `build()` 仅执行 1 次。 |
| **WID-05** | State 生命周期闭环状态机 | `packages/flutter/test/widgets/stateful_components_test.dart` | `test('State lifecycle: init -> didUpdate -> dispose')`<br>`test('setState after dispose throws assertion')` | `tests/unit/state_lifecycle_test.dart`<br>形式化验证生命周期完整流转，对已销毁状态注入异常。 |
| **WID-06** | InheritedWidget $O(1)$ 依赖注入发布通知 | `packages/flutter/test/widgets/inherited_test.dart` | `test('dependOnInheritedWidget registers exact element')`<br>`test('InheritedWidget update notifies dependents only')` | `tests/unit/inherited_widget_test.dart`<br>树下挂载多个子节点，数据改变时断言无依赖子节点 `buildCount == 0`。 |
| **WID-07** | Element 卸载与 GlobalKey 树间漂移 | `packages/flutter/test/widgets/global_key_test.dart` | `test('GlobalKey reparents Element without state loss')`<br>`test('Duplicate GlobalKey throws error')` | `tests/unit/global_key_reparent_test.dart`<br>模拟组件从左子树移动到右子树，断言 State 内部变量完整保留。 |

---

## 5. 图层树合成、RepaintBoundary 与光栅管线（COM: 1~6）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **COM-01** | Layer 双向链表树结构 | `packages/flutter/test/rendering/layer_test.dart` | `test('ContainerLayer append and remove children')`<br>`test('OffsetLayer applies paint translation')` | `tests/unit/layer_tree_test.dart`<br>构建图层拓扑，断言矩阵变换与节点层级嵌套。 |
| **COM-02** | PaintingContext 绘制指令流记录 | `packages/flutter/test/rendering/painting_context_test.dart` | `test('PaintingContext writes draw commands to canvas')`<br>`test('PaintingContext splits layers on boundary')` | `tests/unit/painting_context_test.dart`<br>使用 `MockCanvas` 拦截绘制指令，断言指令流序列。 |
| **COM-03** | isRepaintBoundary 绘制隔离缓存 | `packages/flutter/test/rendering/repaint_boundary_test.dart` | `test('RepaintBoundary caches raster output')`<br>`test('Subtree paint does not trigger parent paint')` | `tests/unit/repaint_boundary_test.dart`<br>子树动画高频刷新时，断言父节点 `paint()` 命中计数为 0。 |
| **COM-04** | needsCompositingBits 图层提升冒泡 | `packages/flutter/test/rendering/compositing_bits_test.dart` | `test('Adding OpacityLayer marks ancestors needsCompositing')`<br>`test('Compositing bits cleanly reset on flush')` | `tests/unit/compositing_bits_test.dart`<br>深层注入半透明节点，断言祖先链路图层提升标志全量同步。 |
| **COM-05** | SceneBuilder 场景图构建 | `engine/src/flutter/lib/ui/compositing_unittests.cc` | `TEST(Compositing, SceneBuilderPushPop)` | `tests/unit/scene_builder_test.dart`<br>序列化图层树并生成场景快照。 |
| **COM-06** | C++ Flow LayerTree 消费与光栅呈现 | `engine/src/flutter/flow/layer_tree_unittests.cc` | `TEST(LayerTree, PrerollAndPaint)` | `tests/unit/flow_layer_tree_test.dart`<br>C++ 引擎侧消费场景图并打印帧渲染完成信号。 |

---

## 6. 输入事件拓扑路由与手势竞技场（ARE: 1~6）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **ARE-01** | 触控事件数据包解析映射 | `packages/flutter/test/gestures/converter_test.dart` | `test('PointerDataPacket translates to PointerEvents')`<br>`test('Physical to logical coordinate scaling')` | `tests/unit/pointer_converter_test.dart`<br>注入原始数据包，断言坐标平移与缩放无误差。 |
| **ARE-02** | 反向深度优先命中测试 (HitTest) | `packages/flutter/test/rendering/box_hit_test_test.dart` | `test('HitTest traverses children in reverse paint order')`<br>`test('Overlapping siblings: topmost hits first')` | `tests/unit/hit_test_test.dart`<br>给定坐标点，断言返回的 `HitTestResult` 目标链路与官方完全一致。 |
| **ARE-03** | GestureArenaManager 竞争仲裁状态机 | `packages/flutter/test/gestures/arena_test.dart` | `test('Arena resolves winner on sweep')`<br>`test('Early defeat releases hold')`<br>`test('Single participant wins automatically')` | `tests/unit/gesture_arena_test.dart`<br>1:1 跑通单人、双人与多人手势竞技状态迁移断言。 |
| **ARE-04** | TapGestureRecognizer 轻触点击判定 | `packages/flutter/test/gestures/tap_test.dart` | `test('Tap down, up triggers onTap')`<br>`test('Exceeding slop before up cancels tap')` | `tests/unit/tap_recognizer_test.dart`<br>模拟触控时间序列，验证滑动距离是否超出阈值的竞争行为。 |
| **ARE-05** | Pan/Drag 滑动阈值竞技抢占 | `packages/flutter/test/gestures/drag_test.dart` | `test('Drag wins arena when crossing kTouchSlop')`<br>`test('Drag defeats competing TapGestureRecognizer')` | `tests/unit/drag_recognizer_test.dart`<br>同时注册 Tap 与 Drag，滑动 $20\text{px}$ 时断言 Drag 胜出且 Tap 收到 reject。 |
| **ARE-06** | 嵌套同向滑动冲突解决方案 | `packages/flutter/test/gestures/multidrag_test.dart` | `test('Nested drag recognizers arbitration')`<br>`test('Parent absorbs touch event when configured')` | `tests/unit/nested_scroll_conflict_test.dart`<br>验证嵌套滑动中父级与子级根据滑动方向争夺胜出权。 |

---

## 7. 动画时钟驱动与物理场系统（ANI: 1~5）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **ANI-01** | AnimationController 数值状态机 | `packages/flutter/test/animation/animation_controller_test.dart` | `test('Controller forward/reverse linear values')`<br>`test('Status transitions accurately')` | `tests/unit/animation_controller_test.dart`<br>以 60Hz 脉冲驱动 Controller，断言数值随时间单调平滑递增。 |
| **ANI-02** | Curves 三阶贝塞尔曲线插值数学解算 | `packages/flutter/test/animation/curves_test.dart` | `test('Cubic curves bounds [0, 1]')`<br>`test('easeIn, easeOut, bounce curve mathematical convergence')` | `tests/unit/curves_test.dart`<br>高密度采样 1000 个离散时间点，断言拟合误差 $\le 10^{-6}$。 |
| **ANI-03** | Tween 泛型区间映射 | `packages/flutter/test/animation/tween_test.dart` | `test('Tween lerp handles Size, Rect and Color')`<br>`test('Custom object tween interpolation')` | `tests/unit/tween_test.dart`<br>验证任意几何对象区间的线性映射与边界保护。 |
| **ANI-04** | SpringSimulation 弹簧阻尼动力学仿真 | `packages/flutter/test/physics/spring_simulation_test.dart` | `test('Overdamped spring converges asymptotically')`<br>`test('Underdamped spring produces oscillations')` | `tests/unit/spring_simulation_test.dart`<br>求解二阶微分方程，断言振荡周期与衰减指数符合物理定律。 |
| **ANI-05** | AnimatedBuilder 局部微重绘绑定 | `packages/flutter/test/widgets/animated_builder_test.dart` | `test('AnimatedBuilder rebuilds on value change')`<br>`test('Static child subtree skips rebuilding')` | `tests/unit/animated_builder_test.dart`<br>验证动画改变时，静态 `child` 参数节点的 `buildCount` 维持为 1。 |

---

## 8. Android 宿主嵌入层与通信底座（EMB: 1~5）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoFlutter 仿真测试实现路径 |
|---|---|---|---|---|
| **EMB-01** | FlutterActivity 宿主生命周期与 Surface 挂载 | `engine/src/flutter/shell/platform/android/test/io/flutter/embedding/android/FlutterActivityTest.java`<br>`FlutterSurfaceViewTest.java` | `testActivityLifecycleDelegation()`<br>`testSurfaceCreatedConnectsRenderer()` | `tests/unit/android_host_lifecycle_test.dart`<br>模拟 Android Activity 生命周期回调流转。 |
| **EMB-02** | FlutterJNI 跨语言对象指针生命周期绑定 | `engine/src/flutter/shell/platform/android/test/io/flutter/embedding/engine/FlutterJNITest.java`<br>`platform_view_android_jni_impl_unittests.cc` | `testNativeInitialization()`<br>`TEST(PlatformViewAndroidJNI, PointerBinding)` | `tests/unit/jni_pointer_binding_test.dart`<br>验证 NativePointer 销毁与弱引用保护机制。 |
| **EMB-03** | StandardMessageCodec 二进制流编解码 | `engine/src/flutter/shell/platform/android/test/io/flutter/plugin/common/StandardMessageCodecTest.java`<br>`packages/flutter/test/services/standard_message_codec_test.dart` | `testPrimitiveEncodingDecoding()`<br>`testNestedMapAndListSerialization()`<br>`testBigEndianByteArrayAlignment()` | `tests/unit/standard_message_codec_test.dart`<br>对比 Dart 编码与 C++/Java 解码的二进制字节流绝对一致性。 |
| **EMB-04** | VsyncWaiterAndroid 硬件时钟对齐 | `engine/src/flutter/shell/platform/android/test/io/flutter/view/VsyncWaiterTest.java` | `testFrameCallbackRegistration()`<br>`testHardwareVsyncPropagation()` | `tests/unit/vsync_waiter_test.dart`<br>验证 Choreographer 帧脉冲到 C++ 回调的时间戳无抖动。 |
| **EMB-05** | PlatformViews 原生视图混合渲染拓扑 | `engine/src/flutter/shell/platform/android/test/io/flutter/plugin/platform/PlatformViewsControllerTest.java` | `testPlatformViewCreation()`<br>`testTextureLayerComposition()` | `tests/unit/platform_views_test.dart`<br>模拟原生 View 纹理在 Flutter 场景树中的嵌入与拉伸。 |

---

## 9. 构建编排引擎与增量缓存（BLD: 1~5）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoCompiler 仿真测试实现路径 |
|---|---|---|---|---|
| **BLD-01** | BuildSystem DAG 拓扑排序调度器 | `packages/flutter_tools/test/general.shard/build_system/build_system_test.dart` | `testUsingContext('BuildSystem can resolve dependency graph')`<br>`testUsingContext('Throws CircularDependencyException on graph cycle')` | `tests/unit/build_system_dag_test.dart`<br>构建多层菱形与环形 Target 拓扑，断言执行序并拦截死循环。 |
| **BLD-02** | FileStore 文件 SHA-256 指纹持久化 | `packages/flutter_tools/test/general.shard/build_system/file_store_test.dart` | `testUsingContext('FileStore calculates sha256 and detects changes')`<br>`testUsingContext('FileStore persists state to disk')` | `tests/unit/file_store_hash_test.dart`<br>单测修改文件内容与只改时间戳两种场景，断言哈希重新计算行为。 |
| **BLD-03** | Makefile .d Depfile 增量依赖语法解析 | `packages/flutter_tools/test/general.shard/build_system/depfile_test.dart` | `testWithoutContext('Depfile parser handles simple and escaped dependencies')`<br>`testWithoutContext('Depfile handles whitespace and backslashes')` | `tests/unit/depfile_parser_test.dart`<br>测试边界语法输入，断言解析出的输入文件绝对路径列表 100% 对齐。 |
| **BLD-04** | Target.canSkip 增量短路判定机制 | `packages/flutter_tools/test/general.shard/build_system/build_system_test.dart` | `testUsingContext('Target.canSkip returns true if inputs/outputs unchanged')`<br>`testUsingContext('Target.canSkip returns false if output missing')` | `tests/unit/target_skip_test.dart`<br>断言未变动工程第二次执行 `build()` 时所有 Target 均被 skip。 |
| **BLD-05** | Environment 构建上下文与参数求值 | `packages/flutter_tools/test/general.shard/build_system/build_system_test.dart` | `testUsingContext('Environment correctly decodes defines and artifacts')`<br>`testUsingContext('Environment handles missing defines')` | `tests/unit/build_environment_test.dart`<br>断言逗号分隔列表解码与跨平台目录解析逻辑。 |

---

## 10. Dart Kernel 前端编译与 AST 变换（KRN: 1~4）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoCompiler 仿真测试实现路径 |
|---|---|---|---|---|
| **KRN-01** | KernelCompiler 前端批处理调用 | `packages/flutter_tools/test/general.shard/compile_test.dart`<br>`compile_batch_test.dart` | `testUsingContext('KernelCompiler outputs app.dill on success')`<br>`testUsingContext('KernelCompiler surfaces frontend server errors')` | `tests/unit/kernel_compiler_test.dart`<br>通过 FakeProcessManager 拦截命令行参数，断言参数构造与产物检验。 |
| **KRN-02** | TrackWidgetCreation 构造函数坐标注桩 | `packages/flutter_tools/test/general.shard/build_system/targets/common_test.dart` | `testUsingContext('KernelSnapshot passes trackWidgetCreation to frontend server')`<br>`testUsingContext('Release build disables trackWidgetCreation')` | `tests/unit/track_widget_creation_test.dart`<br>输入一段 Dart 源码 AST，断言注桩后每个 Widget 节点均注入 `_Location`。 |
| **KRN-03** | TargetModel 目标平台标准库规范映射 | `packages/flutter_tools/test/general.shard/compile_batch_test.dart` | `testUsingContext('TargetModel matches platform requirements')` | `tests/unit/target_model_test.dart`<br>枚举全部 TargetPlatform，断言映射到的 TargetModel 无一错漏。 |
| **KRN-04** | ConstCanonicalization 常量规范折叠 | `packages/flutter_tools/test/general.shard/compile_expression_test.dart` | `testUsingContext('Const canonicalization in kernel output')` | `tests/unit/const_folding_test.dart`<br>验证两个相同值的 const 表达式在 AST 中被规范化为同一引用。 |

---

## 11. 增量编译守护与热重载重构（HOT: 1~5）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoCompiler 仿真测试实现路径 |
|---|---|---|---|---|
| **HOT-01** | ResidentCompiler Stdio 协议状态机 | `packages/flutter_tools/test/general.shard/compile_incremental_test.dart` | `testUsingContext('ResidentCompiler correctly handles compile, recompile, accept, reject sequence')`<br>`testUsingContext('ResidentCompiler recovers from broken pipe')` | `tests/unit/resident_compiler_protocol_test.dart`<br>模拟 Stdio 字符串交互流，断言状态迁移与看门狗超时机制。 |
| **HOT-02** | Incremental Kernel Delta Dill 生成 | `packages/flutter_tools/test/general.shard/compile_incremental_test.dart` | `testUsingContext('ResidentCompiler generates delta dill on modified files')` | `tests/unit/delta_kernel_generator_test.dart`<br>单测修改单个文件，断言生成的增量 dill 仅包含变更库。 |
| **HOT-03** | DevFS 内存文件系统差分同步协议 | `packages/flutter_tools/test/general.shard/devfs_test.dart` | `testUsingContext('DevFS correctly diffs and syncs updated files to device')`<br>`testUsingContext('DevFS skips unmodified asset files')` | `tests/unit/devfs_sync_test.dart`<br>本地修改文件后比对内存 DevFS，断言增量推送包大小仅等于修改文件字节。 |
| **HOT-04** | VM Service reloadSources RPC 驱动 | `packages/flutter_tools/test/general.shard/hot_test.dart` | `testUsingContext('HotRunner triggers _reloadSources RPC and handles error code')`<br>`testUsingContext('Handles kIsolateReloadBarred exception')` | `tests/unit/vm_service_reload_test.dart`<br>模拟 VM Service RPC 响应，断言热重载失败时降级提示热重启。 |
| **HOT-05** | Element.reassemble 运行时状态重塑 | `packages/flutter/test/widgets/framework_test.dart` | `testWidgets('Element reassemble marks all dirty while preserving State')`<br>`testWidgets('State.reassemble is called on rebuild')` | `tests/unit/reassemble_state_preservation_test.dart`<br>在微内核中调用 `reassemble()`，断言所有 RenderObject 重绘且 State 内存实例地址不变。 |

---

## 12. 生产态 AOT 快照编译与多段二进制生成（AOT: 1~4）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoCompiler 仿真测试实现路径 |
|---|---|---|---|---|
| **AOT-01** | AOTSnapshotter 参数编排与进程调用 | `packages/flutter_tools/test/general.shard/build_system/targets/common_test.dart` | `testUsingContext('AotSnapshotBase invokes AOTSnapshotter with correct arguments')`<br>`testUsingContext('Throws on unsupported AOT platform')` | `tests/unit/aot_snapshotter_args_test.dart`<br>断言 Profile 与 Release 模式下的 gen_snapshot 开关矩阵。 |
| **AOT-02** | 4 大内存段映射结构解析 | `engine/src/flutter/runtime/dart_snapshot_unittests.cc` | `TEST(DartSnapshotTest, MmapAndResolveFourSegments)` | `tests/unit/snapshot_segment_parser_test.dart`<br>解析 ELF 共享库符号表，断言 `kSnapshotData` 与 `kSnapshotInstructions` 四大段偏移与尺寸。 |
| **AOT-03** | Type Flow Analysis (TFA) 全局树摇分析 | `packages/flutter_tools/test/general.shard/build_system/targets/android_test.dart` | `testUsingContext('AotSnapshot builds libapp.so with TFA stripped unused classes')` | `tests/unit/tfa_tree_shake_test.dart`<br>断言未被任何调用链触达的死类死方法从符号表中彻底消失。 |
| **AOT-04** | DWARF 调试符号分离与混淆映射 | `packages/flutter_tools/test/general.shard/build_system/targets/common_test.dart` | `testUsingContext('AotSnapshot outputs split debug info and obfuscation mapping')` | `tests/unit/dwarf_split_test.dart`<br>断言符号分离后 `app.so` 明显精简且产出独立符号映射文件。 |

---

## 13. 资产多维树摇与离线着色器预编译（SHK: 1~4）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoCompiler 仿真测试实现路径 |
|---|---|---|---|---|
| **SHK-01** | IconTreeShaker AST 常量码位扫描 | `packages/flutter_tools/test/general.shard/build_system/targets/icon_tree_shaker_test.dart` | `testUsingContext('IconTreeShaker parses recorded_uses.json correctly')`<br>`testUsingContext('Extracts constant code points from AST')` | `tests/unit/icon_code_point_extractor_test.dart`<br>构造包含 10 个特定 IconData 的用例，断言码位提取结果 100% 精确。 |
| **SHK-02** | font-subset 字体文件物理裁剪集成 | `packages/flutter_tools/test/commands.shard/permeable/icon_tree_shaker_test.dart` | `testUsingContext('Font subset utility reduces TTF font file size by >90%')` | `tests/unit/font_subset_integration_test.dart`<br>基于提取码位裁剪官方 MaterialIcons 字体，断言保留字形与文件体积。 |
| **SHK-03** | impellerc 离线着色器编译与打包 | `packages/flutter_tools/test/general.shard/build_system/targets/shader_compiler_test.dart` | `testUsingContext('ShaderCompiler compiles GLSL and packages ShaderBundle')`<br>`testUsingContext('DevelopmentShaderCompiler supports hot reload')` | `tests/unit/shader_bundle_pack_test.dart`<br>模拟将顶点与片元着色器打包为合法二进制 ShaderBundle。 |
| **SHK-04** | AssetManifest.bin 二进制清单序列化 | `packages/flutter_tools/test/general.shard/build_system/targets/assets_test.dart` | `testUsingContext('CopyAssets produces AssetManifest.bin and depfile')` | `tests/unit/asset_manifest_binary_test.dart`<br>序列化复杂多分辨率资产字典，断言反序列化结果无损对齐。 |

---

## 14. 插件元编程与 Native Assets（GEN: 1~3）

| 功能 ID | 功能名称 | 官方源码对应测试文件 | 官方覆盖的测试用例 / 测试函数 | NanoCompiler 仿真测试实现路径 |
|---|---|---|---|---|
| **GEN-01** | DartPluginRegistrantTarget 代码动态合成 | `packages/flutter_tools/test/general.shard/build_system/targets/dart_plugin_registrant_test.dart` | `testUsingContext('DartPluginRegistrantTarget generates valid dart code')` | `tests/unit/plugin_registrant_generator_test.dart`<br>输入包含 3 个插件的 pubspec，断言生成的 `dart_plugin_registrant.dart` 语法合法且类名一致。 |
| **GEN-02** | Native Assets build.dart 编译钩子编排 | `packages/flutter_tools/test/general.shard/isolated/build_system/targets/native_assets_test.dart` | `testUsingContext('BuildHooks target executes build.dart and maps C-ABI assets')` | `tests/unit/native_assets_hook_test.dart`<br>测试 Native Assets 配置生成与动态库路径解析。 |
| **GEN-03** | GenerateLocalizationsTarget 国际化代码生成 | `packages/flutter_tools/test/general.shard/build_system/targets/localizations_test.dart` | `testUsingContext('GenerateLocalizationsTarget parses arb and generates localization code')` | `tests/unit/localizations_codegen_test.dart`<br>解析中英文 ARB 文件，断言生成的消息格式化方法。 |
