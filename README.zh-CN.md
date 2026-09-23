# Surface Pro 7 Linux 摄像头

本仓库公开 **Surface Pro 7（2019，IPU4P PCI ID `8086:8a19`）** 前后摄像头的实验性源码、补丁和复现资料，包括内核、libcamera、GNOME Snapshot 与运行配置。不适用于 Surface Pro 7+ 或其他机型。

截至 2026-09-23，Fedora 42 上的 P1 内核已在**一台** Surface Pro 7 上启动两次。前后摄均可采集 RAW 和 libcamera 视频帧；Snapshot 完成前→后→前拍照、两路可解码视频录制与重复打开；第二次开机自动加载驱动。P1 尚未验证休眠唤醒和主观视频质量，也没有第二台设备的独立测试。详见[验证状态](docs/STATUS.md)。

目前公开的是**源码仓库，并非可直接安装的发行版**。现有 Fedora 42 RPM 未签名，启动脚本和相关配置尚未打包；在安装与恢复流程完善前，不建议仅凭这些文件替换启动内核。

- [内核源码组合和构建边界](docs/BUILDING.md)
- [从微软官方 MSI 自行获取固件](docs/FIRMWARE.md)：本仓库不分发固件
- [验证结果、产物校验值和限制](docs/STATUS.md)
- [作者、许可与上游协作](COPYING.md)

项目使用过 AI 编程辅助。正式向 Linux、libcamera、GNOME 上游提交补丁前，需要人工审阅、真实作者署名和符合 DCO 的签署。
