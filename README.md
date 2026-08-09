# Raspberry Pi 5 temperature and fan controller for ESXi-Arm

[English](#english) | [Русский](#русская-версия) | [Releases](https://github.com/Soulveig/esxi-driver-temperature-rpi5/releases)

## English

`rpitherm` is a VMware ESXi-Arm VMkernel module for Raspberry Pi 5. It reads
the BCM2712 temperature through the VideoCore property mailbox and controls the
three-wire fan on Waveshare PoE HAT (F) Rev1.2 through RP1 PWM1 channel 3.

## Features

- temperature polling every 5 seconds, with a periodic log record every minute;
- automatic fan curve: 0 / 30 / 50 / 70 / 100% at below 50 / 50 / 60 / 67.5 / 75 C;
- 5 C downward hysteresis;
- 100% fail-safe on startup, temperature-read failure, attach failure, and stop;
- no interrupt registration, including no use of shared RP1 interrupt 261;
- no access to RP1 Ethernet or SD/MMC controller registers.

## Compatibility

- Raspberry Pi 5;
- VMware ESXi-Arm 8.0U3c build 24449057;
- [RPI 5 UEFI 0.2.2 [Soulveig Edition]](https://github.com/Soulveig/rpi5-uefi-soulveig-edition/releases/tag/v0.2.2);
- ACPI device `FANC` / `RPI0003` supplied by that UEFI;
- Secure Boot disabled and `CommunitySupported` acceptance level.

## Installation

Use the offline bundle and perform a dry run first:

```console
esxcli software vib update -d /tmp/rpitherm-0.5.0-3-offline-bundle.zip --dry-run --no-sig-check --force
esxcli software vib update -d /tmp/rpitherm-0.5.0-3-offline-bundle.zip --no-sig-check --force
reboot
```

The dry run must select only `BootBankInstaller`, replace only `rpitherm`, and
require a reboot. Live installation and live removal are disabled.

## Verification

```console
esxcli software vib list | grep rpitherm
vmkload_mod -l | grep rpitherm
dmesg | grep rpitherm
```

A successful boot contains a numeric temperature and fan transitions such as
`fan changed old=70% new=50%`. Verify network and storage separately.

## Safety

This is an unsigned driver for one specific ESXi-Arm build. Preserve a known-good
UEFI image and bootbank rollback. The module keeps the fan at 100% when it cannot
read temperature or complete initialization.

## Files

- `rpitherm-0.5.0-3-offline-bundle.zip` — recommended offline depot;
- `rpitherm-0.5.0-3-community.vib` — standalone unsigned VIB;
- `rpitherm.c` — source of the hardware-tested module;
- `SHA256SUMS` — release checksums.

---

## Русская версия

`rpitherm` — модуль VMkernel для Raspberry Pi 5 под VMware ESXi-Arm. Он читает
температуру BCM2712 через VideoCore property mailbox и автоматически управляет
трёхпроводным вентилятором Waveshare PoE HAT (F) Rev1.2 через RP1 PWM1 channel 3.

### Возможности

- опрос температуры каждые 5 секунд и запись значения в журнал раз в минуту;
- кривая 0 / 30 / 50 / 70 / 100% на порогах ниже 50 / 50 / 60 / 67,5 / 75 C;
- гистерезис 5 C при охлаждении;
- безопасные 100% при старте, ошибке температуры, attach или остановке;
- отсутствие регистрации прерываний, включая общий RP1 IRQ 261;
- отсутствие обращений к регистрам Ethernet RP1 и SD/MMC.

### Совместимость

- Raspberry Pi 5;
- VMware ESXi-Arm 8.0U3c build 24449057;
- [RPI 5 UEFI 0.2.2 [Soulveig Edition]](https://github.com/Soulveig/rpi5-uefi-soulveig-edition/releases/tag/v0.2.2);
- ACPI-устройство `FANC` / `RPI0003` из этого UEFI;
- отключённый Secure Boot и уровень `CommunitySupported`.

### Установка

```console
esxcli software vib update -d /tmp/rpitherm-0.5.0-3-offline-bundle.zip --dry-run --no-sig-check --force
esxcli software vib update -d /tmp/rpitherm-0.5.0-3-offline-bundle.zip --no-sig-check --force
reboot
```

Dry-run должен использовать только `BootBankInstaller`, менять только
`rpitherm` и требовать перезагрузку. Live-установка и live-удаление отключены.

### Проверка

```console
esxcli software vib list | grep rpitherm
vmkload_mod -l | grep rpitherm
dmesg | grep rpitherm
```

При успешной загрузке журнал содержит численную температуру и смену ступеней
вентилятора. Сеть и накопители проверяйте отдельно. Сохраните рабочий образ UEFI
и путь отката bootbank.

## License

BSD 2-Clause License. See [LICENSE](LICENSE).
