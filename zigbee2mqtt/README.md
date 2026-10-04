# Zigbee2MQTT — swrelay2x

Если настройка кнопки выдаёт `Status 'UNSUPPORTED_CLUSTER' relay2Config`, обновить внешний конвертер файлом из этого проекта и перезапустить Zigbee2MQTT. Эта ошибка может возникать при загрузке конвертера после выполнения start/configure: herdsman ещё не знает описание пользовательского кластера. Конвертер теперь регистрирует его перед каждым чтением и записью настроек. Перепрошивка для этого исправления не требуется.

Добавить `relay_2ch.mjs` как внешний конвертер в Zigbee2MQTT и подключить устройство заново. Fingerprint: модель `swrelay2x`, производитель `Salu Workshop`. Инструкции загрузки внешних конвертеров: [официальная документация](https://www.zigbee2mqtt.io/advanced/more/external_converters.html).

Конвертер предоставляет два независимых переключателя `state_l1`/`state_l2`, поведение после подачи питания `power_on_behavior_l1`/`power_on_behavior_l2`, четыре Boolean-настройки и шесть кнопочных `action`.

MQTT topic `zigbee2mqtt/DEVICE/set`, примеры:

```json
{"state_l1":"ON","state_l2":"OFF"}
```

```json
{"button_1_linked":false,"button_2_linked":true,"button_1_double_click":true,"button_2_double_click":false}
```

Чтение: `zigbee2mqtt/DEVICE/get`:

```json
{"button_1_linked":"","button_2_double_click":""}
```

Настройки сохраняются в flash. После записи конвертер читает атрибут обратно и публикует подтверждённое значение. `true`/`false` должны быть JSON Boolean, не строками и не числами.

События в `zigbee2mqtt/DEVICE`: `action` равен `button_1_single`, `button_1_double`, `button_1_hold`, `button_2_single`, `button_2_double` или `button_2_hold`. Двойные и длинные нажатия не переключают реле. Отключение связи кнопки с реле не отключает события. Для автоматизаций использовать action-события, включая повторные одинаковые действия.

Стандартные On/Off-отчёты настраиваются и привязываются для обоих endpoint расширением `m.onOff`. Настройки `0xFC00` читаются на обоих endpoint. Кнопочные команды прошивка отправляет непосредственно координатору, endpoint 1; привязка для них не требуется.

Проверено с `zigbee-herdsman-converters 26.108.1`: обработчики с подменой транспорта, загрузка/валидация настоящего внешнего конвертера и декодирование событий настоящим herdsman. Координатор и радио в проверках не участвовали.
