# Загрузчик, объединение BIN и прошивка TLSR8258

Для текущей конфигурации нужны **TLSR8258 с flash 1 MiB**, TC32 toolchain,
GNU make и Python 3. GPIO реле, LED, кнопки и измерителя в
`board_smart_plug_8258.h` обновлены по прозвонке пользователя. Перед заливкой
PC5 по согласованию оставлен для первого стендового запуска: он не выведен
на ZT2S, но внутренние соединения из даташита неизвестны. Проверить ADC
питания и работу NV на стенде; калибровка измерителя также остаётся необходимой.
Прошивать с отключённой сетью 230 В, от подходящего низковольтного питания.
Не подключать USB-программатор к плате под сетью.

## Сборка без IDE

Обновление уже установленного приложения по Zigbee описано в
[OTA_UPDATE.md](OTA_UPDATE.md): цель `ota` создаёт `.zigbee` для Z2M.

Из корня `telink-plug`, заменив путь на свой TC32-префикс:

```sh
make -C smart_plug -j4 combined TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
```

Эта команда собирает загрузчик, приложение и объединяет их. Отдельно:

```sh
make -C smart_plug -j4 TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
make -C smart_plug -j4 bootloader TOOLCHAIN_PREFIX=/path/to/bin/tc32-elf-
```

Если `tc32-elf-gcc/ld/objcopy/size` доступны в PATH, аргумент
`TOOLCHAIN_PREFIX` можно убрать. ARM/RISC-V компилятор не подходит.
Официальный контейнер TC32: [telink-dev-tc32](https://github.com/orgs/telink-semi/packages/container/package/telink-dev-tc32).

| Файл | Назначение | Смещение во flash |
|---|---|---|
| `smart_plug/build/bootloader/bootloader.bin` | Загрузчик | `0x00000` |
| `smart_plug/build/boot1/smart_plug.bin` | Приложение | `0x10000` |
| `smart_plug/build/smart_plug_factory.bin` | Оба образа | `0x00000` |

Рядом с отдельными BIN будут ELF, map и raw.bin. Заливать подготовленные
`.bin`, а не `.raw.bin`. Проверить size и map обоих ELF, запас RAM/stack.
На момент подготовки инструкции TC32 здесь отсутствует: настоящая линковка
и ELF/BIN не проверены. Host-проверка её не заменяет.

## Конфигурация загрузчика

Используются SDK `apps/bootLoader`, локальная конфигурация
`smart_plug/bootloader` и общий board-файл розетки. Реле при GPIO-инициализации
получает неактивный уровень. UART-обновление загрузчика отключено: пины заняты
измерителем (BL0937 по умолчанию, HLW8032 выбирается в `meter_config.h`). Загрузчик сразу проверяет OTA и запускает приложение.

Локальный `bootloader/bootloader.c` — копия SDK с исправлением границ размера
OTA: исключены вычитание из размера меньше 4 и стирание вне OTA при повреждённом
размере. Остальные механизмы взяты из SDK; это не полный аудит загрузчика.
При обновлении SDK нужно отдельно переносить и проверять исправление.

## Объединение готовых BIN

```sh
python3 smart_plug/tools/merge_firmware.py \
  smart_plug/build/bootloader/bootloader.bin \
  smart_plug/build/boot1/smart_plug.bin \
  smart_plug/build/smart_plug_factory.bin
```

Проверяются размеры, заголовки и CRC обоих файлов, а также ограничение
RAM code приложения резервом загрузчика 16 KiB. Промежуток до `0x10000`
заполняется `FF`. Простой `cat` неверен. Каждый образ сохраняет свой CRC;
объединённый файл повторно через `prepare_firmware.py` не пропускать.

Это **не полный дамп flash**: файл заканчивается после приложения,
до OTA `0x7A000..0xE3FFF`. NV/MAC/calibration в него не включаются.
Предел приложения с padding/CRC — `0x6A000` (424 KiB), загрузчика —
`0x10000` (64 KiB). Merge не подтверждает linker offset: использовать
выходы соответствующих целей Makefile.

## Из имеющегося оборудования: CH340 через SWire

Для первого стендового запуска можно использовать CH340 с выведенным RTS
и **логическими уровнями 3.3 В**. Перемычка питания 3.3 В на USB-UART
не всегда переключает уровни TX/RTS — проверить их отдельно.
Официальный Telink Burning EVK + BDT предпочтительнее, если такой адаптер есть.

Подключение к **площадкам модуля ZT2S**, а не номерам ножек микросхемы:

| Источник | ZT2S |
|---|---|
| CH340 TX через резистор 1–1.8 кОм | SWS, площадка 12 (PA7) |
| CH340 RX напрямую | Та же SWS, площадка 12 |
| CH340 RTS | RST, площадка 10 |
| CH340 GND и земля питания | GND, площадка 3 |
| Внешнее стабилизированное питание 3.3 В | VCC, площадка 1 |

Не соединять TX и RX напрямую: между TX и общей линией SWS/RX нужен
резистор. UART-контакты модуля и HLW8032 в этой схеме не используются.
Питание должно выдерживать потребление всей подключённой низковольтной
части платы; RTS предназначен для сброса, не для питания.
Автор утилиты исключает FTDI и адаптеры со светодиодной нагрузкой на RX.
Если нет RTS или уровни 5 В, эту схему напрямую не применять.

Источник схемы и программы:
[pvvx/TlsrComSwireWriter](https://github.com/pvvx/TlsrComSwireWriter),
[схема подключения](https://github.com/pvvx/TlsrComSwireWriter/blob/master/schematicc.gif).
Ниже команды именно для `TLSR825xComFlasher-macos.py` из этого репозитория:
в других утилитах значения `wf`/`we` могут быть обратными.
На нашей плате этот способ ещё не проверен.

Выполнять из корня проекта. Подготовка:

```sh
git clone https://github.com/pvvx/TlsrComSwireWriter.git /tmp/plug-swire
python3 -m venv /tmp/plug-swire-venv
/tmp/plug-swire-venv/bin/python -m pip install pyserial
/tmp/plug-swire-venv/bin/python -m serial.tools.list_ports

PLUG_PY=/tmp/plug-swire-venv/bin/python
PLUG_FLASHER=/tmp/plug-swire/TLSR825xComFlasher-macos.py
PLUG_PORT=/dev/cu.wchusbserialXXXX
mkdir -p flash_backup
```

Заменить `PLUG_PORT` на порт своего адаптера. Сначала дважды прочитать
1 MiB исходного flash, убедиться в отсутствии ошибок и сравнить дампы:

```sh
"$PLUG_PY" "$PLUG_FLASHER" -p "$PLUG_PORT" -t 70 rf 0 0x100000 flash_backup/original.bin
"$PLUG_PY" "$PLUG_FLASHER" -p "$PLUG_PORT" -t 70 rf 0 0x100000 flash_backup/original_second.bin
wc -c flash_backup/original.bin flash_backup/original_second.bin
cmp flash_backup/original.bin flash_backup/original_second.bin
```

Каждый файл должен быть ровно 1048576 байт, `cmp` должен завершиться
с кодом 0 без вывода. При ошибке чтения или различии дампов **не переходить
к записи**. Сохранить исходный дамп отдельно от проекта. COM-SWire может
читать медленно; дождаться полного завершения.

После успешной сборки и сохранения дампа, по одной команде, с остановкой
при любой ошибке:

```sh
# Только первый сектор старого OTA; не весь flash.
"$PLUG_PY" "$PLUG_FLASHER" -p "$PLUG_PORT" -t 70 es 0x7A000 0x1000

# wf в этой программе стирает необходимые секторы и записывает файл.
"$PLUG_PY" "$PLUG_FLASHER" -p "$PLUG_PORT" -t 70 wf 0 smart_plug/build/smart_plug_factory.bin

PLUG_IMAGE_SIZE=$(wc -c < smart_plug/build/smart_plug_factory.bin | tr -d ' ')
"$PLUG_PY" "$PLUG_FLASHER" -p "$PLUG_PORT" -t 70 rf 0 "$PLUG_IMAGE_SIZE" flash_backup/programmed_readback.bin
cmp smart_plug/build/smart_plug_factory.bin flash_backup/programmed_readback.bin
```

После успешного сравнения отключить программатор и перезапустить питание
3.3 В. Проверить реле, кнопку, LED и удержание кнопки 10 секунд для
factory reset/commissioning. Измеритель и ADC питания PC5 требуют проверки
на реальной плате. Низковольтный запуск не проверяет измерение сети.
Команду `ea` (erase all) не использовать.

Pico и Blue Pill требуют отдельной прошивки программатора. Не переносить
инструкции для TLSR8232 на TLSR8258: адреса и протоколы могут различаться.

## OpenOCD

Проверенный локальный OpenOCD из `~/.Telink_Tools` не содержит TC32 в
`target types` и конфигурации TLSR8258/SWire в scripts. Поэтому через
**установленную здесь сборку OpenOCD путь прошивки не подготовлен**.
`target/riscv.cfg` и `target/cortex_m.cfg` для TC32 не подходят.

Если имеется специальная сборка с TC32/SWire, нужны её версия, совместимый
адаптер и board/target cfg. Ниже только шаблон для такой подтверждённой
конфигурации, **не рабочая команда для установленного OpenOCD**:

```sh
openocd -f /path/to/verified-tlsr8258-board.cfg \
  -c 'init; reset halt; flash banks; shutdown'
```

Только если cfg определяет правильный адаптер, flash 1 MiB с базой
`0x000000`, bank 0 и поддерживает erase/write/verify:

```sh
openocd -f /path/to/verified-tlsr8258-board.cfg \
  -c 'init; reset halt; flash read_bank 0 original_flash.bin 0 0x100000; shutdown'

openocd -f /path/to/verified-tlsr8258-board.cfg \
  -c 'init; reset halt; flash write_image erase smart_plug/build/smart_plug_factory.bin 0x000000 bin; flash verify_image smart_plug/build/smart_plug_factory.bin 0x000000 bin; shutdown'
```

Перед первым запуском также инвалидировать старый OTA header по `0x7A000`
средствами подтверждённого драйвера: объединённый файл его не изменяет.
Иначе загрузчик может установить старый OTA вместо нового приложения.
Reset/run — только после успешной записи/verify и очистки старого OTA.
Синтаксис общих команд: [OpenOCD Flash Commands](https://openocd.org/doc/html/Flash-Commands.html).

## Путь производителя: Telink BDT + Burning EVK

8258 программируется через **SWS/SWire**: SWS адаптера к SWS платы, GND к GND;
питание по схеме конкретной платы/адаптера. UART RX/TX HLW8032 здесь не нужны.
Не подавать 5 В на GPIO. Ниже `/path/to/bdt` заменить на установленный
CLI BDT для Linux/macOS. Команды подтверждены документацией, но BDT и плата
здесь не запускались.

Проверить связь и сохранить исходный flash **до записи**:

```sh
/path/to/bdt 8258 sws
/path/to/bdt 8258 rf 0x0 -s 1048576 -o original_flash.bin
```

Размер 1048576 использовать после подтверждения flash 1 MiB. Убедиться,
что чтение успешно и размер верен; сохранить дамп вне build.

Первичная установка нашего загрузчика и приложения:

```sh
# Инвалидировать старый OTA: стереть только его первый сектор и записать 00.
/path/to/bdt 8258 wf 0x7A000 00 -s 1 -e

# Режим загрузки файла BDT выполняет необходимое стирание автоматически.
/path/to/bdt 8258 wf 0x0 -i smart_plug/build/smart_plug_factory.bin

PLUG_IMAGE_SIZE=$(wc -c < smart_plug/build/smart_plug_factory.bin | tr -d ' ')
/path/to/bdt 8258 rf 0x0 -s "$PLUG_IMAGE_SIZE" -o programmed_readback.bin
cmp smart_plug/build/smart_plug_factory.bin programmed_readback.bin
```

Перезапуск **только после успешного чтения и `cmp` (код 0)**:

```sh
/path/to/bdt 8258 reset
```

Не применять полное стирание flash: оно уничтожит MAC/calibration/NV.
Первичная установка не мигрирует NV чужой прошивки. После успешного запуска
выполнить локальный factory reset кнопкой для чистого commissioning;
поведение с данными прежней прошивки требует проверки платы.

В последующих обновлениях с **уже проверенным нашим загрузчиком** можно
записывать только приложение по `0x10000`, также исключив старый OTA и
сравнив прочитанный диапазон с BIN перед reset.

Источник: [Telink BDT Linux & MAC](https://doc.telink-semi.cn/doc/en/software/res/tools/bdt_linux_mac/bdt_linux_mac_en/).
Проверить `bdt help` своей версии CLI/EVK.

## Проверки

```sh
make -C smart_plug check
```

Проверяются приложение, отдельная конфигурация загрузчика, повреждённые
размеры OTA, CRC и объединение BIN. TC32-сборка и аппаратные испытания
остаются отдельными обязательными этапами.
