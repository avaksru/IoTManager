# 🔥 IoTManager - OpenTherm Controller

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32%20%7C%20ESP8266-green.svg)
![Version](https://img.shields.io/badge/Version-2025.03-blue.svg)

> OpenTherm контроллер отопительного котла.  
> Мониторинг состояния котла и управление термостатированием.

---

## 🚀 Возможности

### 🖥️ Web-интерфейс контроллера

![Web-интерфейс](https://live-control.com/wiki/opentherm/img/w1.jpg)

Встроенный веб-интерфейс позволяет:
- 📊 **Мониторинг** — отслеживание всех параметров котла в реальном времени
- 🎛️ **Управление** — изменение температуры, режимов работы
- 📈 **Графики** — постоение графиков по любым параметрам
- 🔍 **Диагностика** — чтение любых команд протокола OpenTherm

### 📱 Мобильное приложение

![Мобильное приложение](https://live-control.com/wiki/opentherm/img/m2.jpg)

Управление через мобильное приложение IoT Manager (iOS/Android).

### 🏠 Интеграция с Home Assistant

![Home Assistant](https://live-control.com/wiki/opentherm/img/ha.jpg)

Полная поддержка MQTT для интеграции с Home Assistant.

---

## 🌡️ Встроенный термостат

Реализованы **4 варианта термостатирования**:

| Режим | Описание |
|-------|----------|
| 🔄 **PID** | Управление температурой в контуре отопления |
| 📊 **Гистерезис** | Включение при отклонении температуры от заданной |
| 📉 **Эквитермические кривые** | Регулировка по температуре на улице |
| 🌡️ **Эквитермические + помещение** | То же + учёт температуры в помещении |

---

## 📋 Требования

| Компонент | Описание |
|-----------|----------|
| **Адаптер OpenTherm** | С ESP32-С6 (рекомендуется) или ESP32 |
| **MQTT брокер** | Локальный (Mosquitto) или облачный |
| **Wi-Fi роутер** | Для локального управления |

---

## 🖥️ OTPanel — Панель управления

![Панель управления](https://live-control.com/wiki/otpanel/img/panel.jpg)

Дистанционная панель для управления контроллером OpenTherm. Не требует проводов!

### Поддерживаемые контроллеры

- ✅ SmartTherm
- ✅ OTGateway
- ✅ Live-Control
- ✅ SmartKot Wi-Fi
- ✅ SmartKot ZigBee (zigbee2mqtt, HOMEd)

### Возможности панели

- 🎯 Изменение целевой температуры термостата
- 🔌 Включение/выключение термостата
- 🚿 Управление контуром отопления и ГВС

### Элементы панели

![Элементы](https://live-control.com/wiki/otpanel/img/elements.jpg)

### Где купить

Панель **ESP32-4848S040C** (ESP32 LVGL) доступна на AliExpress:
- [Без блока питания](https://aliexpress.com/item/ESP32-4848S040C) — крепится на липучки, питание 5V
- [С блоком питания и реле](https://aliexpress.com/item/ESP32-4848S040C-power) — в квадратный подрозетник, 3 реле 10A



## 📚 Документация и поддержка

| Ресурс | Ссылка |
|--------|--------|
| 💬 **Telegram канал** | [@live_control](https://t.me/live_control) |
| 🌐 **Wiki** | [live-control.com](https://live-control.com/wiki/) |