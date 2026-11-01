# Flutter 引擎与框架功能全集（NanoFlutter 需求追踪矩阵 RTM）

> **基准源码**：`flutter_source/flutter`（Monorepo 快照，包含 Framework 与 Engine）  
> **设计方法论**：Triple-Axis Cross-Verification —— 官方源码实现 × 运行时架构契约 × 1:1 仿真微内核 三维对账  
> **文档性质**：NanoFlutter 解构与 1:1 原理仿真的全量功能需求集（Requirements Traceability Matrix, RTM）  
> **文档索引**：[`00_master_plan_and_methodology.md`](./00_master_plan_and_methodology.md) | **下游**：[`02-test-inventory.md`](./02-test-inventory.md)、[`03-roadmap.md`](./03-roadmap.md)

---

## 0. 追踪体系与功能编码规范

### 0.1 三维交叉对账矩阵

```
[维度 1: 官方源码物理实现]  ──►  [NanoFlutter RTM 需求矩阵]  ◄──  [维度 2: 架构契约与状态机]
                                           │
                                           ▼
                                [维度 3: 1:1 独立微内核复刻]
                         (纯 Dart Framework + 现代 C++ 宿主桥接)
```

- **维度 1（源码无遗漏）**：`flutter_source` 下相关源码文件中的关键类、核心方法必须 100% 映射到功能点 ID。
- **维度 2（契约无死角）**：渲染管线单次遍历法则、三棵树解耦契约、手势仲裁协议完整覆盖。
- **维度 3（微内核独立性）**：所有功能点均在 `nano_flutter` 中具备无官方框架依赖的纯净代码实现。

### 0.2 功能 ID 编码规则

| 前缀 | 所属子系统 / 领域 | 覆盖模块 |
|---|---|---|
| **SCH-nn** | 线程并发、消息循环与时钟调度 (Scheduler & Concurrency) | 模块 2 |
| **LAY-nn** | 单次遍历盒模型与渲染管线微内核 (Box Layout & Pipeline) | 模块 3 |
| **FLX-nn** | 弹性盒 Flex 模型、多子容器与滚动切片 (Flex & Slivers) | 模块 4 |
| **WID-nn** | 响应式三棵树状态机与 Reconciliation (Widgets & Elements) | 模块 5 |
| **COM-nn** | 图层树合成、RepaintBoundary 缓存与光栅管线 (Layer & Compositing) | 模块 6 |
| **ARE-nn** | 输入事件拓扑路由与手势竞技场仲裁 (HitTest & Gesture Arena) | 模块 7 |
| **ANI-nn** | 时钟驱动动画、缓动曲线与物理仿真 (Animation & Physics) | 模块 8 |
| **EMB-nn** | Android 宿主嵌入层与跨语言双向通讯 (Android Embedder & JNI) | 模块 1 |

---

## 1. 线程并发与帧时钟调度（SCH: 1~6）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **SCH-01** | FML 消息循环泵与 TaskRunner | `engine/src/flutter/fml/message_loop.cc:32`<br>`task_runner.cc:28` | Platform 与 UI 线程隔离；任务队列遵循 FIFO；支持基于时间戳的延迟任务优先级堆。 | 现代 C++20 `NanoTaskRunner`，使用 `std::condition_variable` 管理事件泵。 |
| **SCH-02** | 硬件 Vsync 脉冲捕获与注入 | `engine/src/flutter/shell/platform/android/vsync_waiter_android.cc:34`<br>`lib/ui/hooks.dart:140` | `Choreographer` 脉冲纳秒时间戳单调递增；经 JNI/FFI 跨线程派发至 Dart UI 线程。 | `VsyncWaiter` 纳秒时钟发生器，通过 `dart:ffi` 的 `NativeCallable` 唤醒 Dart。 |
| **SCH-03** | SchedulerBinding 帧五阶段状态机 | `packages/flutter/lib/src/scheduler/binding.dart:880` | 帧状态严格流转：`idle` $\rightarrow$ `transientCallbacks` $\rightarrow$ `midFrameMicrotasks` $\rightarrow$ `persistentCallbacks` $\rightarrow$ `postFrameCallbacks`。禁止逆流。 | 1:1 复刻 `NanoSchedulerBinding`，精确管理动画回调与渲染流水线回调。 |
| **SCH-04** | Ticker 时钟驱动与周期管理 | `packages/flutter/lib/src/scheduler/ticker.dart:70` | 仅在 `isTicking == true` 且处于活跃状态时向 Scheduler 注册；组件不可见时必须能挂起。 | 纯 Dart `NanoTicker`，接收每帧纳秒增量 `Duration elapsed`。 |
| **SCH-05** | 微任务清空屏障与批处理 | `packages/flutter/lib/src/scheduler/binding.dart:1140`<br>`lib/ui/hooks.dart:180` | 在 `persistentCallbacks`（排版渲染）前必须彻底排空微任务队列，确保所有 `setState` 状态全部入树。 | 微任务安全屏障（Drain Barrier），杜绝跨帧脏状态悬挂。 |
| **SCH-06** | 优先级异步任务调度 | `packages/flutter/lib/src/scheduler/priority.dart:12` | 动画任务优先级高于普通空闲任务（idle/touch/animation），低优先级任务在掉帧时自动让步。 | 带优先级的双向链表任务队列。 |

---

## 2. 单次遍历盒模型与渲染管线微内核（LAY: 1~8）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **LAY-01** | BoxConstraints 约束代数系统 | `packages/flutter/lib/src/rendering/box.dart:60` | 二维区间 `[minWidth..maxWidth, minHeight..maxHeight]`；不变式：$0 \le \min \le \max \le \infty$。支持 `tight`, `loose`, `isBounded` 判定。 | 纯 Dart 几何代数类，提供约束松弛、紧绷与交集运算。 |
| **LAY-02** | 单次遍历排版法则 (One-pass Layout) | `packages/flutter/lib/src/rendering/object.dart:1750`<br>`box.dart:2100` | **约束向下传递，尺寸向上反馈**；禁止子节点反向决定父级约束，严禁二次回流（Reflow）。 | `RenderBox.layout()` 核心遍历算法，严守单次向下单次向上的复杂度 $O(N)$。 |
| **LAY-03** | PipelineOwner 脏节点流水线驱动 | `packages/flutter/lib/src/rendering/pipeline_owner.dart:120` | 管理 `_nodesNeedingLayout`；按树深从小到大排序依次调用 `performLayout()`，杜绝重复排版。 | `NanoPipelineOwner`，提供 `flushLayout()`、`flushCompositingBits()` 与 `flushPaint()`。 |
| **LAY-04** | RelayoutBoundary 局部重排阻断边界 | `packages/flutter/lib/src/rendering/object.dart:1820` | 满足任一条件即为边界：① `constraints.isTight`；② `!parentUsesSize`；③ `sizedByParent`。阻断 `markNeedsLayout` 冒泡。 | 1:1 复刻重排隔离算法，深层叶子变更时保证祖先节点 100% 拦截跳过。 |
| **LAY-05** | SizedByParent 协议与 dryLayout | `packages/flutter/lib/src/rendering/box.dart:2050` | 若 `sizedByParent == true`，尺寸在 `performResize()` 仅凭父约束计算，不依赖子节点测量。 | 剥离排版与测度计算，实现尺寸预解耦。 |
| **LAY-06** | 绝对几何坐标映射与矩阵变换 | `packages/flutter/lib/src/rendering/box.dart:2500`<br>`matrix_utils.dart:120` | `localToGlobal()` 与 `globalToLocal()`；支持 4x4 仿射矩阵变换与逆矩阵求解。 | 实现无误差的坐标转换，用于命中测试与渲染图层映射。 |
| **LAY-07** | RenderPadding 边距单子偏移定位 | `packages/flutter/lib/src/rendering/shifted_box.dart:180` | 子节点约束由父约束缩减 EdgeInsets 产生；自身尺寸为子尺寸叠加边距（受约束 clamp）。 | 实现基础单子容器，用于管道冒泡断言验证。 |
| **LAY-08** | RenderPositionedBox 居中与对齐排版 | `packages/flutter/lib/src/rendering/shifted_box.dart:320` | 父级提供松约束；根据 `Alignment` 将子节点居中或贴边偏移。 | 实现 Center/Align 几何排版。 |

---

## 3. 弹性盒 Flex 模型、多子容器与滚动切片（FLX: 1~6）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **FLX-01** | ContainerParentDataMixin 双向链表子节点 | `packages/flutter/lib/src/rendering/object.dart:3100` | 渲染树多子节点不使用 `List` 数组存储，而是通过 `previousSibling / nextSibling` 指针构成双向链表。 | 1:1 实现双向链表侵入式拓扑，降低动态重排内存抖动。 |
| **FLX-02** | RenderFlex 主轴交叉轴两轮空间分配 | `packages/flutter/lib/src/rendering/flex.dart:750` | ① 第一轮：先测量非弹性子项（FlexFactor==0），计算剩余空间；② 第二轮：按 Flex 比例均分剩余空间给弹性子项。 | 复刻 Row/Column 经典弹性布局，支持 MainAxisAlignment 对齐策略。 |
| **FLX-03** | 弹性溢出检测 (Overflow Yellow Tape) | `packages/flutter/lib/src/rendering/flex.dart:980` | 当所有子节点尺寸总和超出父约束主轴最大限制时，计算溢出像素 `_overflow` 并标记溢出。 | 精确判定溢出阈值，提供边界断言。 |
| **FLX-04** | RenderStack 绝对与相对层叠排版 | `packages/flutter/lib/src/rendering/stack.dart:510` | 区分非定位子项（决定 Stack 尺寸）与 `Positioned` 绝对定位子项（依据 Stack 尺寸定位）。 | 实现重叠布局算法与层叠层级绘制。 |
| **FLX-05** | RenderViewport 虚拟视口几何裁剪 | `packages/flutter/lib/src/rendering/viewport.dart:220` | 视口建立局部可滚动笛卡尔系；将 `ViewportOffset` 转换为几何裁剪区域。 | 实现滚动视口基础排版与偏移矩阵。 |
| **FLX-06** | RenderSliver 虚拟化按需局部排版 | `packages/flutter/lib/src/rendering/sliver.dart:600`<br>`sliver_list.dart:180` | 仅为视口可见区域内的 item 创建并排版 RenderBox，滑出可视区的节点立即回收。 | 实现极简 SliverList 虚拟化列表微内核。 |

---

## 4. 响应式三棵树状态机与 Reconciliation（WID: 1~7）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **WID-01** | Widget 不可变声明式配置原子 | `packages/flutter/lib/src/widgets/framework.dart:380` | `@immutable`；频繁实例化与销毁，自身无状态，仅作为 Element 的孵化图纸。 | 纯 Dart 基类，定义 `createElement()` 与 `key` 属性。 |
| **WID-02** | Element 树形骨架持有与挂载体系 | `packages/flutter/lib/src/widgets/framework.dart:1000` | 长期驻留堆中；持有父子关系与生命周期；驱动底层 `RenderObject` 实例的创建与属性同步。 | 复刻 `ComponentElement` 与 `RenderObjectElement` 双主干。 |
| **WID-03** | Widget.canUpdate 核心比对与复用 | `packages/flutter/lib/src/widgets/framework.dart:410` | 当且仅当 `oldWidget.runtimeType == newWidget.runtimeType && oldWidget.key == newWidget.key` 时复用旧 Element。 | 1:1 实现三棵树 Reconciliation Diff 核心算法。 |
| **WID-04** | BuildOwner 脏节点批量管理与防抖 | `packages/flutter/lib/src/widgets/framework.dart:2600` | 管理 `_dirtyElements`；单帧内多次 `setState()` 仅加入队列一次；按深度由浅入深批量 `rebuild()`。 | `NanoBuildOwner`，提供 `scheduleBuildFor()` 与 `buildScope()`。 |
| **WID-05** | State 生命周期闭环状态机 | `packages/flutter/lib/src/widgets/framework.dart:1150` | 严格状态迁移：`created` $\rightarrow$ `initState` $\rightarrow$ `didChangeDependencies` $\rightarrow$ `build` $\rightarrow$ `deactivate` $\rightarrow$ `dispose`。 | 1:1 完整生命周期钩子派发，保证状态安全。 |
| **WID-06** | InheritedWidget $O(1)$ 依赖注入发布订阅 | `packages/flutter/lib/src/widgets/framework.dart:1700` | 子 Element 通过 `dependOnInheritedWidgetOfExactType` 注册依赖；数据变更时仅精准刷新依赖节点。 | 基于哈希表实现 $O(1)$ 查找与定向通知机制。 |
| **WID-07** | Element 卸载与 GlobalKey 树间漂移 | `packages/flutter/lib/src/widgets/framework.dart:1280` | 被移除的 Element 进入 `inactive` 列表；单帧结束前若被 GlobalKey 拾取则重新挂载，否则彻底 `unmount()`。 | 模拟元素回收池与全局 Key 救回机制。 |

---

## 5. 图层树合成、RepaintBoundary 与光栅管线（COM: 1~6）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **COM-01** | Layer 双向链表树结构 | `packages/flutter/lib/src/rendering/layer.dart:180` | 区别于渲染树；Layer 树专用于提交给光栅化引擎。包含 `OffsetLayer`, `ClipRectLayer`, `TransformLayer`, `PictureLayer`。 | 纯 Dart 图层拓扑树，支持图层追加与矩阵剪裁嵌套。 |
| **COM-02** | PaintingContext 绘制指令流记录 | `packages/flutter/lib/src/rendering/object.dart:120` | 为 RenderObject 分配绘制画布；当遇到重绘边界时，新开独立 `OffsetLayer`。 | `NanoPaintingContext`，驱动指令录制到 `MockCanvas`。 |
| **COM-03** | isRepaintBoundary 局部绘制隔离缓存 | `packages/flutter/lib/src/rendering/proxy_box.dart:880` | 当 `isRepaintBoundary == true` 时，子树绘制产出独立的 Layer；子树重绘绝不污染父节点图层缓存。 | 1:1 复刻重绘边界机制，拦截动画对全局界面的绘制污染。 |
| **COM-04** | needsCompositingBits 图层提升冒泡 | `packages/flutter/lib/src/rendering/object.dart:2150` | 若子节点包含必须由合成器处理的图层（如 Clip/Opacity），必须沿渲染树向上传播图层提升标记。 | 实现 `flushCompositingBits()` 状态位级联标记与冒泡。 |
| **COM-05** | SceneBuilder 场景图序列化构建 | `engine/src/flutter/lib/ui/compositing.dart:20` | 将 Layer 树拓扑展平打包为 C++ 引擎可消费的 `Scene` 对象。 | 纯 Dart 抽象 `NanoSceneBuilder`，生成绘制指令场景包。 |
| **COM-06** | C++ Flow LayerTree 消费与光栅化提交 | `engine/src/flutter/flow/layer_tree.cc:50` | C++ 引擎消费 Scene，调用底层 Skia/Impeller 指令集完成多重缓冲交换呈现。 | C++ 引擎侧实现极简的 LayerTree 遍历器与帧 Present 打印。 |

---

## 6. 输入事件拓扑路由与手势竞技场（ARE: 1~6）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **ARE-01** | 触控事件数据包标准化转换 | `engine/src/flutter/shell/platform/android/io/flutter/embedding/android/AndroidTouchProcessor.java:70`<br>`lib/ui/pointer.dart:40` | Android 原生 `MotionEvent` 转换为高内聚 `PointerDataPacket`，统一物理坐标与逻辑坐标。 | 实现触控数据包解析器，支持 Down, Move, Up, Cancel 映射。 |
| **ARE-02** | 反向深度优先命中测试 (HitTest) | `packages/flutter/lib/src/gestures/hit_test.dart:80`<br>`rendering/box.dart:2650` | 自顶向下反向深度优先遍历渲染树；子节点优先入链；构建 `HitTestResult` 目标路径链。 | 1:1 实现 `RenderBox.hitTest()` 递归判定与矩阵逆变换。 |
| **ARE-03** | GestureArenaManager 竞争仲裁状态机 | `packages/flutter/lib/src/gestures/arena.dart:120` | 手势竞争池：`add()` 入围 $\rightarrow$ `hold()` 观察 $\rightarrow$ `resolve()` 决胜；胜者触发 `accept()`，败者触发 `reject()` 淘汰。 | 1:1 复刻手势竞技场核心仲裁算法。 |
| **ARE-04** | TapGestureRecognizer 单击与长按判定 | `packages/flutter/lib/src/gestures/tap.dart:150` | 在手指移动距离不超过 `kTouchSlop`（18px）且在时限内抬起时判定胜出；超时未抬起转长按。 | 纯 Dart 实现轻触识别器与时限状态机。 |
| **ARE-05** | Pan/DragGestureRecognizer 滑动阈值竞争 | `packages/flutter/lib/src/gestures/monodrag.dart:180` | 触控点滑移距离突破 `kTouchSlop` 瞬间宣布胜利，立即淘汰同竞技场内的所有 Tap 识别器。 | 实现滑动手势识别器与主轴/交叉轴位移累加器。 |
| **ARE-06** | 嵌套同向滑动冲突解决方案 | `packages/flutter/lib/src/gestures/arena.dart:210` | 父容器与子容器同时监听同向滑动时，由竞技场裁决胜负归属，支持通过 `GestureRecognizer.claimVictory` 抢占。 | 编写标准冲突场景仲裁器，消除嵌套滚动死锁。 |

---

## 7. 动画时钟驱动与物理场系统（ANI: 1~5）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **ANI-01** | AnimationController 数值状态机 | `packages/flutter/lib/src/animation/animation_controller.dart:220` | 四种状态迁移：`dismissed` $\leftrightarrow$ `forward` $\leftrightarrow$ `completed` $\leftrightarrow$ `reverse`；基于 Vsync 时间增量推进。 | 纯 Dart 实现 Controller，支持时长设定与数值线性流逝。 |
| **ANI-02** | Curves 三阶贝塞尔缓动曲线拟合 | `packages/flutter/lib/src/animation/curves.dart:120` | 基于牛顿迭代法求解三次贝塞尔方程 $B(t) = (x, y)$；实现 `easeIn`, `easeOut`, `easeInOut`。 | 1:1 实现 `Cubic` 曲线数学解算器，精度误差 $\le 10^{-6}$。 |
| **ANI-03** | Tween 泛型区间映射与插值 | `packages/flutter/lib/src/animation/tween.dart:50` | 定义 $f(t) = \text{begin} + (\text{end} - \text{begin}) \times t$；支持 `Size`, `Offset`, `Color` 的区间线性插值。 | 泛型插值器体系，驱动 RenderObject 几何属性平滑演变。 |
| **ANI-04** | SpringSimulation 弹簧阻尼微分方程 | `packages/flutter/lib/src/physics/spring_simulation.dart:70` | 求解二阶弹簧阻尼动力学方程 $m x'' + c x' + k x = 0$；支持过阻尼、欠阻尼与临界阻尼振动。 | 纯数学解析求解器，提供拟真物理弹性回弹数值流。 |
| **ANI-05** | AnimatedBuilder 局部微重绘绑定 | `packages/flutter/lib/src/widgets/animated_builder.dart:60` | 通过 `Listenable` 机制仅触发该子组件局部的 `build()`，父级绝不发生无谓重建。 | 实现高效动画局部刷新组件模式。 |

---

## 8. Android 宿主嵌入层与通信底座（EMB: 1~5）

| 功能 ID | 功能名称 | 官方源码规范依据 | 状态机/不变式契约 | NanoFlutter 仿真目标 |
|---|---|---|---|---|
| **EMB-01** | FlutterActivity 宿主生命周期与 Surface 载体 | `engine/src/flutter/shell/platform/android/io/flutter/embedding/android/FlutterActivity.java:180` | `onCreate` 挂载引擎 $\rightarrow$ `onStart/onResume` 激活时钟 $\rightarrow$ Surface 就绪时连接 C++ 交换链 $\rightarrow$ `onDestroy` 销毁。 | 极简 Android Native Activity 宿主，直接绑定自定义 JNI。 |
| **EMB-02** | FlutterJNI 跨语言对象指针绑定 | `engine/src/flutter/shell/platform/android/io/flutter/embedding/engine/FlutterJNI.java:80`<br>`platform_view_android_jni_impl.cc:120` | Java `nativeShellHolder` 持有 C++ 堆对象指针；JNI 方法必须保证单向生命周期解耦，严防悬空引用。 | 实现精简版 JNI 注册与 NativePointer 生命周期绑定。 |
| **EMB-03** | StandardMessageCodec 二进制流编解码 | `engine/src/flutter/shell/platform/android/io/flutter/plugin/common/StandardMessageCodec.java:90`<br>`packages/flutter/lib/src/services/message_codecs.dart:400` | 高性能二进制序列化规范：大端序；支持 null, bool, int32, int64, float64, string, list, map 零拷贝解包。 | 1:1 实现 Dart 与 Java/C++ 双端一致的序列化编解码器。 |
| **EMB-04** | VsyncWaiterAndroid 硬件时钟对齐 | `engine/src/flutter/shell/platform/android/io/flutter/view/VsyncWaiter.java:30` | 注册 `Choreographer.FrameCallback`；将 `frameTimeNanos` 经 JNI 传递给 C++ `onVsync` 调度器。 | Android 端硬件刷新率捕获器，实现 60/120Hz 稳定对齐。 |
| **EMB-05** | PlatformViews 原生视图混合渲染拓扑 | `engine/src/flutter/shell/platform/android/io/flutter/plugin/platform/PlatformViewsController.java:150` | 原生 Android View（如原生 Map/WebView）嵌入 Flutter 层级；解构 TextureLayer 与 HybridComposition 原理。 | 仿真外部纹理（External Texture）合成与触摸事件透传。 |
