# Native Raspberry Pi 5 temperature monitor for ESXi-Arm

`rpitherm` is a read-only VMware ESXi-Arm VMkernel module that reads the
Raspberry Pi 5 SoC temperature through the standard Raspberry Pi firmware
property mailbox.

## Contents

- [Features](#features)
- [Compatibility](#compatibility)
- [Firmware requirement](#firmware-requirement)
- [Installation](#installation)
- [Verification](#verification)
- [Update and removal](#update-and-removal)
- [Safety and limitations](#safety-and-limitations)
- [Files](#files)
- [Русская версия](#русская-версия)

## Features

- Reads the BCM2712 SoC temperature in millidegrees Celsius.
- Performs one read during device attachment and one periodic read every 60 seconds.
- Uses the ACPI `RPIQ` / `BCM2849` mailbox device exposed by the required UEFI.
- Does not bind to the RP1 Ethernet or SD/MMC controllers.
- Does not access PWM, GPIO, or fan-control registers.
- Does not register interrupts.

## Compatibility

- Raspberry Pi 5.
- VMware ESXi-Arm 8.0U3c build 24449057.
- Soulveig Raspberry Pi 5 UEFI 0.2.1 with ACPI `RPIQ` / `BCM2849`.
- Secure Boot disabled.
- `CommunitySupported` acceptance level.

## Firmware requirement

The module requires the modified Raspberry Pi 5 UEFI from
[Soulveig/rpi5-uefi-soulveig-edition](https://github.com/Soulveig/rpi5-uefi-soulveig-edition).
The firmware exposes the Raspberry Pi property mailbox as an ACPI device with a
single MMIO resource. Stock UEFI images without this ACPI device are not
supported.

## Installation

Copy the offline bundle to the ESXi host and run a dry-run first:

```console
esxcli software vib update -d /tmp/rpitherm-0.4.4-1-offline-bundle.zip --dry-run --no-sig-check --force
```

The dry-run must install only `rpitherm`, use `BootBankInstaller`, and report
that a reboot is required. Then install and reboot:

```console
esxcli software vib update -d /tmp/rpitherm-0.4.4-1-offline-bundle.zip --no-sig-check --force
reboot
```

Keep the host in maintenance mode and preserve a tested altbootbank rollback.

## Verification

After reboot:

```console
esxcli software vib get -n rpitherm
vmkload_mod -l | grep rpitherm
grep -o 'rpitherm: temperature=[0-9]* mC ([0-9.]* C)' /var/log/vmkernel.log
grep -o 'rpitherm: periodic temperature=[0-9]* mC ([0-9.]* C) read=[0-9]*' /var/log/vmkernel.log
```

A successful boot contains a numeric temperature, for example `41127 mC
(41.127 C)`. Also verify the host network and storage devices independently.

## Update and removal

Live installation and live removal are disabled. Install, update, or remove the
VIB only through `BootBankInstaller` followed by a reboot. Do not repeatedly
load and unload the module on a running host.

## Safety and limitations

- This is an unsigned `CommunitySupported` VIB for a specific ESXi-Arm build.
- Temperature is written to `vmkernel.log`; no Host Client sensor integration is provided.
- The module does not control the fan. UEFI fan behavior after `ExitBootServices`
  is separate from this driver.
- Keep local-console access and a tested rollback path available.

## Files

- `rpitherm-0.4.4-1-offline-bundle.zip` — recommended offline depot.
- `rpitherm-0.4.4-1-community.vib` — standalone unsigned VIB.
- `rpitherm.c` — VMkernel module source.
- `SHA256SUMS` — release checksums.

---

## Русская версия

`rpitherm` — модуль VMkernel для VMware ESXi-Arm, который только читает
температуру SoC Raspberry Pi 5 через стандартный property mailbox прошивки
Raspberry Pi.

### Содержание

- [Возможности](#возможности)
- [Совместимость](#совместимость)
- [Требование к UEFI](#требование-к-uefi)
- [Установка](#установка)
- [Проверка](#проверка)
- [Обновление и удаление](#обновление-и-удаление)
- [Безопасность и ограничения](#безопасность-и-ограничения)
- [Файлы](#файлы)

### Возможности

- Чтение температуры BCM2712 в тысячных долях градуса Цельсия.
- Одно чтение при подключении устройства и далее одно чтение каждые 60 секунд.
- Работа через ACPI-устройство `RPIQ` / `BCM2849`, предоставленное требуемым UEFI.
- Модуль не связывается с контроллерами Ethernet RP1 и SD/MMC.
- Модуль не обращается к PWM, GPIO или регистрам управления вентилятором.
- Модуль не регистрирует прерывания.

### Совместимость

- Raspberry Pi 5.
- VMware ESXi-Arm 8.0U3c build 24449057.
- Soulveig Raspberry Pi 5 UEFI 0.2.1 с ACPI `RPIQ` / `BCM2849`.
- Secure Boot отключён.
- Уровень приёмки `CommunitySupported`.

### Требование к UEFI

Требуется модифицированный UEFI из репозитория
[Soulveig/rpi5-uefi-soulveig-edition](https://github.com/Soulveig/rpi5-uefi-soulveig-edition).
Он предоставляет property mailbox Raspberry Pi системе ESXi как ACPI-устройство
с одним MMIO-ресурсом. Стандартные образы UEFI без этого ACPI-устройства не
поддерживаются.

### Установка

Скопируйте offline bundle на хост ESXi и сначала выполните dry-run:

```console
esxcli software vib update -d /tmp/rpitherm-0.4.4-1-offline-bundle.zip --dry-run --no-sig-check --force
```

Dry-run должен менять только `rpitherm`, использовать `BootBankInstaller` и
требовать перезагрузку. После этого установите пакет и перезагрузите хост:

```console
esxcli software vib update -d /tmp/rpitherm-0.4.4-1-offline-bundle.zip --no-sig-check --force
reboot
```

Хост должен находиться в maintenance mode. Сохраните проверенный rollback через
altbootbank.

### Проверка

После перезагрузки:

```console
esxcli software vib get -n rpitherm
vmkload_mod -l | grep rpitherm
grep -o 'rpitherm: temperature=[0-9]* mC ([0-9.]* C)' /var/log/vmkernel.log
grep -o 'rpitherm: periodic temperature=[0-9]* mC ([0-9.]* C) read=[0-9]*' /var/log/vmkernel.log
```

В журнале должна присутствовать численная температура, например `41127 mC
(41.127 C)`. Отдельно проверьте сеть и накопители хоста.

### Обновление и удаление

Live-установка и live-удаление отключены. Устанавливайте, обновляйте и удаляйте
VIB только через `BootBankInstaller` с последующей перезагрузкой. Не выполняйте
многократную загрузку и выгрузку модуля на работающем хосте.

### Безопасность и ограничения

- Это неподписанный VIB уровня `CommunitySupported` для конкретной сборки ESXi-Arm.
- Температура записывается в `vmkernel.log`; интеграции с датчиками Host Client нет.
- Модуль не управляет вентилятором. Поведение вентилятора UEFI после
  `ExitBootServices` не относится к этому драйверу.
- Сохраните доступ к локальной консоли и заранее проверенный путь отката.

### Файлы

- `rpitherm-0.4.4-1-offline-bundle.zip` — рекомендуемый offline depot.
- `rpitherm-0.4.4-1-community.vib` — отдельный неподписанный VIB.
- `rpitherm.c` — исходный код модуля VMkernel.
- `SHA256SUMS` — контрольные суммы файлов релиза.

## License

BSD 2-Clause License. See [LICENSE](LICENSE).
