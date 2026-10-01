# ResumableDownloader

基于 Qt 6 / C++ 实现的 Windows 桌面文件下载器，支持 HTTP/HTTPS 下载、实时进度与速度统计，以及基于 HTTP Range 的暂停与断点续传。

## 功能

- HTTP / HTTPS 文件下载
- 自定义保存目录与同名文件覆盖确认
- 实时下载进度、速度和预计剩余时间
- 暂停、继续和取消下载
- 使用 `Range` 请求与 `206 Partial Content` 实现断点续传
- 检测服务器是否支持续传，并处理网络失败状态
- 使用流式写盘避免将整个文件一次性加载到内存

## 技术实现

- `QNetworkAccessManager` / `QNetworkReply`：异步网络请求与数据接收
- `QFile`：分块写入下载内容
- `QElapsedTimer`：计算实时速度与剩余时间
- HTTP `Range`：从指定字节偏移继续请求
- Qt Signals & Slots：处理下载进度、响应完成和界面交互
- 枚举状态机：维护空闲、下载中和暂停状态

## 开发环境

- C++17
- Qt 6.5+
- Qt Widgets / Qt Network
- CMake
- MSVC 2022
- Windows

## 构建

使用 Qt Creator 打开项目根目录中的 `CMakeLists.txt`，选择 Qt 6 MSVC Kit 后配置并构建项目即可。
