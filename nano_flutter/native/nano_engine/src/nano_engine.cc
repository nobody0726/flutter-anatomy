#include "nano_engine.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <atomic>

namespace nano {

// 仿 FML TaskRunner: 单线程事件循环与任务队列
class TaskRunner {
 public:
  TaskRunner() : running_(true), worker_(&TaskRunner::Run, this) {}

  ~TaskRunner() {
    Stop();
  }

  void PostTask(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!running_) return;
      queue_.push_back(std::move(task));
    }
    cv_.notify_one();
  }

  void Stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!running_) return;
      running_ = false;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
      worker_.join();
    }
  }

 private:
  void Run() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });
        if (!running_ && queue_.empty()) {
          break;
        }
        task = std::move(queue_.front());
        queue_.pop_front();
      }
      if (task) {
        task();
      }
    }
  }

  std::atomic<bool> running_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::function<void()>> queue_;
  std::thread worker_;
};

// 仿 vsync_waiter: 负责产生对齐刷新率的时钟中断
class VsyncWaiter {
 public:
  explicit VsyncWaiter(TaskRunner* ui_task_runner)
      : ui_task_runner_(ui_task_runner),
        vsync_requested_(false),
        running_(true),
        ticker_thread_(&VsyncWaiter::TickerLoop, this) {}

  ~VsyncWaiter() {
    running_ = false;
    cv_.notify_all();
    if (ticker_thread_.joinable()) {
      ticker_thread_.join();
    }
  }

  void SetCallback(NanoVsyncCallback callback) {
    callback_ = callback;
  }

  void RequestVsync() {
    bool expected = false;
    if (vsync_requested_.compare_exchange_strong(expected, true)) {
      cv_.notify_one();
    }
  }

 private:
  void TickerLoop() {
    const std::chrono::milliseconds kFrameInterval(16); // 模拟 60Hz 帧率 (~16.6ms)

    while (running_) {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return vsync_requested_.load() || !running_; });
      if (!running_) break;

      // 模拟硬件刷新周期对齐
      std::this_thread::sleep_for(std::chrono::milliseconds(2));

      vsync_requested_.store(false);
      auto now_nanos = nano_engine_get_time_nanos();
      auto cb = callback_;

      if (cb && ui_task_runner_) {
        // 将 Vsync 脉冲派发到 UI 线程执行
        ui_task_runner_->PostTask([cb, now_nanos]() {
          cb(now_nanos);
        });
      }
    }
  }

  TaskRunner* ui_task_runner_;
  std::atomic<bool> vsync_requested_;
  std::atomic<bool> running_;
  std::mutex mutex_;
  std::condition_variable cv_;
  NanoVsyncCallback callback_ = nullptr;
  std::thread ticker_thread_;
};

} // namespace nano

// 全局静态单例与 C 接口导出
static std::unique_ptr<nano::TaskRunner> g_ui_task_runner;
static std::unique_ptr<nano::VsyncWaiter> g_vsync_waiter;

extern "C" {

bool nano_engine_init(void) {
  if (!g_ui_task_runner) {
    g_ui_task_runner = std::make_unique<nano::TaskRunner>();
    g_vsync_waiter = std::make_unique<nano::VsyncWaiter>(g_ui_task_runner.get());
  }
  return true;
}

void nano_engine_register_vsync_callback(NanoVsyncCallback callback) {
  if (g_vsync_waiter) {
    g_vsync_waiter->SetCallback(callback);
  }
}

void nano_engine_request_vsync(void) {
  if (g_vsync_waiter) {
    g_vsync_waiter->RequestVsync();
  }
}

void nano_engine_post_ui_task(NanoTask task, void* user_data) {
  if (g_ui_task_runner && task) {
    g_ui_task_runner->PostTask([task, user_data]() {
      task(user_data);
    });
  }
}

int64_t nano_engine_get_time_nanos(void) {
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
}

void nano_engine_shutdown(void) {
  g_vsync_waiter.reset();
  g_ui_task_runner.reset();
}

}
