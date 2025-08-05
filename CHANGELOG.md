## [1.0.3] - 2025-08-05
### Fixed
- 修复了空调应该处理 `"关"` 而不是 `"关闭"` 操作的问题

## [1.0.2] - 2025-08-04
### Added
- 添加了新的输入类型语音指令码 `VOICE_CMD`

## [1.0.1] - 2025-08-02
### Changed
- 延长内存监控打印的间隔到1小时

### Fixed
- 真的修复了日志重定向至mqtt时, "OTA_OK"消息无法在网上收到的问题, 而且现在在每次连上mqtt时都会发送

## [1.0.0] - 2025-08-02
### Added
- 为 `SingleRelayDevice` 与 `DryContactOut` 添加了关联设备与排斥设备的解析与处理

### Fixed
- 修复 `InfraredAC::execute` 的 `"温度升高"` 操作无法将温度升至31的问题
- 修复 `LordManager::registerDryContactInput` 时应该查询插拔卡通道物理状态的问题
- 修复日志重定向至mqtt时, "OTA_OK"消息无法在网上收到的问题
- 修复 `handle_mqtt_ndjson` 里将 `param` 写错成 `parameter` 的问题