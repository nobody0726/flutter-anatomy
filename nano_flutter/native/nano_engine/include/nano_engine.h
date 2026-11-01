#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define NANO_EXPORT __declspec(dllexport)
#else
#define NANO_EXPORT __attribute__((visibility("default")))
#endif

/// 帧同步脉冲回调函数指针类型
/// @param frame_time_nanos 纳秒级高精度时钟戳 (仿照 Choreographer timestamp)
typedef void (*NanoVsyncCallback)(int64_t frame_time_nanos);

/// 初始化 NanoEngine C++ 宿主运行时
NANO_EXPORT bool nano_engine_init(void);

/// 注册 Dart 端的 Vsync 帧回调
NANO_EXPORT void nano_engine_register_vsync_callback(NanoVsyncCallback callback);

/// 向引擎请求下一帧 Vsync 脉冲 (1:1 模拟 Flutter Window.scheduleFrame)
NANO_EXPORT void nano_engine_request_vsync(void);

/// 向 UI 线程投递异步任务
typedef void (*NanoTask)(void* user_data);
NANO_EXPORT void nano_engine_post_ui_task(NanoTask task, void* user_data);

/// 获取当前高精度纳秒时间戳
NANO_EXPORT int64_t nano_engine_get_time_nanos(void);

/// 销毁并释放 NanoEngine 运行时
NANO_EXPORT void nano_engine_shutdown(void);

#ifdef __cplusplus
}
#endif
