## [1.0.6] - 2025-08-25
### Added
- 现在 `ChannelInput` 的任意键执行也会被 `InputTag::IGNORE_ANY_KEY_EXECUTE` 拦截了
- 添加新Oracle `restart` 

### Changed
- 将 `report_after_ota` 改为了 `report_firmware_status`

### Fixed
- 修复了持有 `InputTag::IGNORE_ANY_KEY_EXECUTE` tag的输入并没有忽略任意键执行的问题

## [1.0.5] - 2025-08-22
### Added
- 添加了 `InputTag::IGNORE_ANY_KEY_EXECUTE`, 用于让某些输入无视任意键执行
- `PresetDevice::execute` 中, 现在会在添加 `SOS` 状态时立即上报状态

### Changed
- 现在空调的 `execute` 收到 `"打开"` 或 `"开"` 时, 会固定使用全局空调默认配置
- 优化了动作组被外部中断时的自杀逻辑, 现在不会被 `DeviceType::DELAYER` 阻塞自杀了

### Fixed
- 修复了空调根本不处理 `开` 和 `关` 的问题

## [1.0.4] - 2025-08-15
### Added
- 为mqtt添加了一些新的ORACLE协议

### Fixed
- 修复 `room_state` 相关实现, 每个状态本该携带时间戳

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