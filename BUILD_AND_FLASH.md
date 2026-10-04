# Сборка и прошивка swrelay2x

Команды выполнять из директории репозитория `swrelay2x`. Аппаратная цель: TLSR8258, flash 1 MiB. Полная распиновка задана в [relay_pins.h](relay_pins.h), для контроля питания выбран неподключённый PC5 по указанию пользователя. Сборка разрешена; тесты используют эту же конфигурацию GPIO.

## Проверки и сборка

Нужны Python 3, Clang и Node.js для тестов, GNU Make и TC32 toolchain для прошивки. SDK по умолчанию находится в `../telink_zigbee_sdk/tl_zigbee_sdk`; для другой директории добавить `SDK_DIR=/absolute/path/to/tl_zigbee_sdk`. Использованный SDK commit: `7cf1d6dbac6497505d649bd03342efe81f1a8212`.

```sh
make check
make -j4 combined TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
make -j4 ota TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
```

Если `tc32-elf-gcc`, `tc32-elf-ld`, `tc32-elf-objcopy`, `tc32-elf-size` доступны в PATH, убрать `TOOLCHAIN_PREFIX`. ARM/RISC-V GCC и Clang не создают TC32-прошивку. Режим загрузчика `BOOT_LOADER_MODE=1` обязателен для выбранной цели.

Результаты:

- `build/relay_2ch_factory.bin` — загрузчик + приложение, адрес записи **0x00000**.
- `build/bootloader/bootloader.bin` — только загрузчик, **0x00000**.
- `build/boot1/relay_2ch.bin` — только приложение, **0x10000**, нужен совместимый загрузчик.
- `build/boot1/relay_2ch.raw.bin`, `.elf`, `.map` — вспомогательные артефакты.
- `build/boot1/relay_2ch.zigbee` — OTA-контейнер, не файл для записи BDT.

Приложение занимает слот до 424 KiB. Упаковщик проверяет размеры, добавляет выравнивание FF, заголовок и CRC SDK; объединение проверяет обе прошивки и размещает приложение по 0x10000. Загрузчик использует ту же конфигурацию GPIO и выключает оба реле перед проверкой/переносом OTA. UART update отключён.

## Telink BDT через Burning EVK/SWS

`/path/to/bdt` заменить на установленный BDT для macOS/Linux. Подключение: SWS платы к SWS адаптера, общий GND, питание по схеме платы/адаптера. Команды соответствуют [документации Telink BDT](https://doc.telink-semi.cn/doc/en/software/res/tools/bdt_linux_mac/bdt_linux_mac_en/); на устройстве в этой работе они не выполнялись. При нескольких EVK выбрать адаптер опциями `-b` и `-d`.

Проверить связь и сохранить исходный flash 1 MiB перед первой записью:

```sh
/path/to/bdt 8258 sws
/path/to/bdt 8258 rf 0x0 -s 1048576 -o original_flash.bin
```

После успешного чтения сохранить `original_flash.bin` вне `build`. Первая установка загрузчика и приложения:

```sh
# Старый OTA не должен заменить новое приложение при первом запуске.
/path/to/bdt 8258 wf 0x7A000 00 -s 1 -e
/path/to/bdt 8258 wf 0x0 -i build/relay_2ch_factory.bin

RELAY_IMAGE_SIZE=$(wc -c < build/relay_2ch_factory.bin | tr -d ' ')
/path/to/bdt 8258 rf 0x0 -s "$RELAY_IMAGE_SIZE" -o programmed_readback.bin
cmp build/relay_2ch_factory.bin programmed_readback.bin
```

После успешного выполнения всех команд и `cmp` с кодом 0:

```sh
/path/to/bdt 8258 reset
```

Не стирать всю flash: MAC/calibration и другие заводские данные находятся вне записываемого образа. После первой установки удерживать служебную кнопку 10 секунд для очистки сети/настроек старой прошивки, открыть подключение в Zigbee2MQTT и добавить устройство с новым конвертером.

При последующих аппаратных обновлениях с нашим совместимым загрузчиком можно записывать только приложение:

```sh
/path/to/bdt 8258 wf 0x7A000 00 -s 1 -e
/path/to/bdt 8258 wf 0x10000 -i build/boot1/relay_2ch.bin
RELAY_APP_SIZE=$(wc -c < build/boot1/relay_2ch.bin | tr -d ' ')
/path/to/bdt 8258 rf 0x10000 -s "$RELAY_APP_SIZE" -o app_readback.bin
cmp build/boot1/relay_2ch.bin app_readback.bin
```

Reset также выполнять только после успешного сравнения. Такой путь сохраняет настройки; заводской сброс при обычном обновлении не нужен.

## Что проверено

`make check`: C-поведение с ASan/UBSan, wire-проверки/fuzzing, синтаксис полного набора C приложения и загрузчика по реальным SDK headers, упаковка BIN/OTA и конвертер. Проверка с настоящими herdsman-библиотеками описана в README. **TC32-компиляция/линковка, прошивка, радио и работа на физической плате остаются аппаратными этапами.**
