# 项目长期笔记 — flutter-anatomy

## 参考资料
- **Flutter API 官方文档**：https://api.flutter.dev/index.html
  - 用途：查询 dart:ui / flutter/material / widgets 等库的 API 签名、类继承关系、参数说明
  - 常用子路径形态：`https://api.flutter.dev/flutter/<library>/<Class>-class.html`（例如 `flutter/widgets/StatelessWidget-class.html`）
  - 记录时间：2026-10-31（用户指定，后续任务需以此为准）

## 本地资源
- `flutter_source/flutter/`：Flutter SDK 源码（含 packages/flutter、engine 相关 .dart/.cc/.h）
- 该目录**仅本地保留**，已在 `.gitignore` 中整体排除，不进入版本库、不对外发布

## 仓库
- GitHub（public）：https://github.com/nobody0726/flutter-anatomy
- 本地 `main` 已跟踪 `origin/main`；账号 `nobody0726`（gh CLI 已登录）
- 历史策略：`legacy/sdk-snapshot` 分支保留重写前的旧提交（含 SDK 快照），仅本地存在，未推送
