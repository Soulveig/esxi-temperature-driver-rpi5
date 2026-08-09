# v0.5.0

## English

- Adds automatic Raspberry Pi 5 fan control for ESXi-Arm.
- Reads BCM2712 temperature every 5 seconds.
- Uses a 0 / 30 / 50 / 70 / 100% curve with 5 C downward hysteresis.
- Uses 100% as the fail-safe state.
- Requires [RPI 5 UEFI 0.2.2 [Soulveig Edition]](https://github.com/Soulveig/rpi5-uefi-soulveig-edition/releases/tag/v0.2.2) with `FANC` / `RPI0003`.
- Does not register interrupts or access RP1 Ethernet and SD/MMC controllers.

Install and removal require BootBankInstaller followed by a reboot.

## Русский

- Добавлено автоматическое управление вентилятором Raspberry Pi 5 в ESXi-Arm.
- Температура BCM2712 опрашивается каждые 5 секунд.
- Используется кривая 0 / 30 / 50 / 70 / 100% с гистерезисом 5 C.
- Безопасное состояние при ошибке — 100%.
- Требуется [RPI 5 UEFI 0.2.2 [Soulveig Edition]](https://github.com/Soulveig/rpi5-uefi-soulveig-edition/releases/tag/v0.2.2) с `FANC` / `RPI0003`.
- Драйвер не регистрирует прерывания и не обращается к Ethernet RP1 или SD/MMC.

Установка и удаление выполняются через BootBankInstaller с перезагрузкой.
