# 整合版热升级回归（2026-11-26）

实机反馈：专项 MCU 07724ce 热升级通过，整合 MCU 4c7856c 又卡住；ESP32 414e67。恢复后的日志 session 3351070418、MD5 5749295d6ff6793f3aad336795ef42fb、normal、UART 错误 0，不能证明卡住的位置。实际比较两个 SHA：board.c、guard_service.c、maintenance.c blob 相同，新增代码集中于原生相册/媒体/UI。不是复位代码被覆盖。

确定的互斥漏洞：end_ota 后 state=0、staged=true，旧 media_busy 只判断 state，允许相册 restore/JPEG 解码、媒体命令和 socket，以及资源轮询在 A1 激活等待期重新运行。修复：定义 maintenance_media_blocked(state||staged)，仅原始 UART 所有权仍以 state 为准，Guard INSTALL 不受新媒体门禁影响；暂存等待期间拒绝媒体命令（busy，不消费序列），停止 socket 和 JPEG 恢复、资源轮询，保持 OTA UI 和已提交时模块抑制。暂存超时/取消后恢复。保持 07724ce 原厂复位、NOR 校验和 WDT 路径不变。已有故障记录增加 15=OTA_CHECK、16=OTA_COMMITTED、17=OTA_QUIESCE、18=OTA_RESET，只有原有故障/WDT 保留条件成立时可回读，不保证掉电保留。主机测试覆盖暂存期间与过期后的互斥判据；不是板级复位或媒体并发实测。本次并不证明互斥漏洞就是实机卡死根因，必须在相同 ESP32 下测试停播与播放两种热升级路径，记录卡死前 status/recovery，不改 Bootloader、不绕过 NOR 门禁。
