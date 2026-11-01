import 'dart:async';
import 'dart:ffi' as ffi;
import 'package:test/test.dart';
import '../../nano_flutter/lib/nano_engine_ffi.dart';

void main() {
  setUpAll(() {
    NanoEngineBridge.initialize(
        dylibPath: 'nano_flutter/lib/libnano_engine.dylib');
  });

  tearDownAll(() {
    NanoEngineBridge.shutdown();
  });

  test('NanoEngine C++ Bridge Initialization & Nanosecond Clock', () {
    final t1 = NanoEngineBridge.getTimeNanos();
    expect(t1, greaterThan(0));

    // 短暂耗时操作验证单调递增
    var acc = 0;
    for (var i = 0; i < 10000; i++) {
      acc += i;
    }
    final t2 = NanoEngineBridge.getTimeNanos();
    expect(t2, greaterThanOrEqualTo(t1));
  });

  test('Vsync Heartbeat Dispatch from C++ TaskRunner to Dart', () async {
    final completer = Completer<int>();

    // 使用 Dart FFI NativeCallable 监听 C++ 的纳秒帧时钟
    final callback =
        ffi.NativeCallable<NanoVsyncCallbackNative>.listener((int frameNanos) {
      if (!completer.isCompleted) {
        completer.complete(frameNanos);
      }
    });

    NanoEngineBridge.registerVsyncCallback(callback.nativeFunction);

    final beforeRequest = NanoEngineBridge.getTimeNanos();
    NanoEngineBridge.requestVsync();

    // 等待 C++ 线程池派发 Vsync 脉冲回 Dart
    final frameTime = await completer.future.timeout(
      const Duration(milliseconds: 500),
      onTimeout: () => throw TimeoutException('Vsync did not fire within 500ms'),
    );

    expect(frameTime, greaterThan(0));
    expect(frameTime, greaterThanOrEqualTo(beforeRequest));

    callback.close();
  });
}
