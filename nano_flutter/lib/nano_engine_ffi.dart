import 'dart:ffi' as ffi;
import 'dart:io' show Platform, File;
import 'package:ffi/ffi.dart';

// 定义 C 回调类型: void (*NanoVsyncCallback)(int64_t frame_time_nanos)
typedef NanoVsyncCallbackNative = ffi.Void Function(ffi.Int64 frameTimeNanos);
typedef NanoVsyncCallback = void Function(int frameTimeNanos);

// 定义 C 函数原型
typedef NanoEngineInitNative = ffi.Bool Function();
typedef NanoEngineInitDart = bool Function();

typedef NanoEngineRegisterVsyncCallbackNative = ffi.Void Function(
    ffi.Pointer<ffi.NativeFunction<NanoVsyncCallbackNative>> callback);
typedef NanoEngineRegisterVsyncCallbackDart = void Function(
    ffi.Pointer<ffi.NativeFunction<NanoVsyncCallbackNative>> callback);

typedef NanoEngineRequestVsyncNative = ffi.Void Function();
typedef NanoEngineRequestVsyncDart = void Function();

typedef NanoEngineGetTimeNanosNative = ffi.Int64 Function();
typedef NanoEngineGetTimeNanosDart = int Function();

typedef NanoEngineShutdownNative = ffi.Void Function();
typedef NanoEngineShutdownDart = void Function();

/// NanoEngine C++ 宿主桥接器 (通过 Dart FFI 直连)
class NanoEngineBridge {
  static late final ffi.DynamicLibrary _dylib;
  static bool _initialized = false;

  static late final NanoEngineInitDart _init;
  static late final NanoEngineRegisterVsyncCallbackDart _registerVsync;
  static late final NanoEngineRequestVsyncDart _requestVsync;
  static late final NanoEngineGetTimeNanosDart _getTimeNanos;
  static late final NanoEngineShutdownDart _shutdown;

  /// 加载 C++ 动态库并绑定符号
  static void initialize({String? dylibPath}) {
    if (_initialized) return;

    final path = dylibPath ??
        (Platform.isMacOS
            ? 'lib/libnano_engine.dylib'
            : 'lib/libnano_engine.so');

    _dylib = ffi.DynamicLibrary.open(path);

    _init = _dylib
        .lookup<ffi.NativeFunction<NanoEngineInitNative>>('nano_engine_init')
        .asFunction();

    _registerVsync = _dylib
        .lookup<ffi.NativeFunction<NanoEngineRegisterVsyncCallbackNative>>(
            'nano_engine_register_vsync_callback')
        .asFunction();

    _requestVsync = _dylib
        .lookup<ffi.NativeFunction<NanoEngineRequestVsyncNative>>(
            'nano_engine_request_vsync')
        .asFunction();

    _getTimeNanos = _dylib
        .lookup<ffi.NativeFunction<NanoEngineGetTimeNanosNative>>(
            'nano_engine_get_time_nanos')
        .asFunction();

    _shutdown = _dylib
        .lookup<ffi.NativeFunction<NanoEngineShutdownNative>>(
            'nano_engine_shutdown')
        .asFunction();

    _init();
    _initialized = true;
  }

  /// 注册 Vsync 回调 (Pointer 到 NativeFunction)
  static void registerVsyncCallback(
      ffi.Pointer<ffi.NativeFunction<NanoVsyncCallbackNative>> callback) {
    _registerVsync(callback);
  }

  /// 触发下一帧 Vsync 请求
  static void requestVsync() {
    _requestVsync();
  }

  /// 获取引擎纳秒时间戳
  static int getTimeNanos() {
    return _getTimeNanos();
  }

  /// 释放引擎资源
  static void shutdown() {
    if (!_initialized) return;
    _shutdown();
    _initialized = false;
  }
}
