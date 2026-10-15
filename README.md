# DeskPet 🐾

Настольный робот-компаньон на **LILYGO T-Display-S3 (ESP32-S3, LCD 320 × 170)**. Цель — живой персонаж с локальной анимацией, а позднее голосом, AI-провайдерами (OpenAI / Gemini / Ollama), интеграцией с Mac, напоминаниями и умным домом.

## Статус

**Firmware 0.1 — первый прототип.** Работает без Wi-Fi, API-ключей, дополнительных плат и датчиков. Показывает анимированные глаза, моргает, меняет направление взгляда и реагирует на кнопки. Сборка и работа на физической плате требуют локальной проверки.

> Этот профиль рассчитан на **обычную T-Display-S3 с TFT LCD**, не Touch, AMOLED или Pro. Для других ревизий потребуется отдельный display profile.

## Прошивка через терминал (macOS)

Для macOS с Homebrew рекомендуем **PlatformIO CLI через Homebrew**. Не используйте `pip install --user` с Homebrew Python: в современных версиях он может завершиться `externally-managed-environment` (PEP 668).

```bash
brew install platformio
pio --version
```

Клонировать ветку первого прототипа и собрать:

```bash
git clone https://github.com/KopievDev/DeskPet.git
cd DeskPet
git switch feat/firmware-animated-face
pio run -d firmware -e tdisplay_s3
```

Если репозиторий уже клонирован, достаточно начать с `cd DeskPet` (если вы не в нём) и `pio run -d firmware -e tdisplay_s3`.

Подключить плату USB-C (кабель для передачи данных), затем:

```bash
pio device list
pio run -d firmware -e tdisplay_s3 -t upload
pio device monitor -d firmware -b 115200
```

При нескольких портах добавить `--upload-port /dev/cu.usbmodemXXXX` для `run`, а в monitor — `-p /dev/cu.usbmodemXXXX`. Если загрузка не начинается: удерживать BOOT, кратко нажать RST, отпустить BOOT и повторить upload.

**Если Homebrew недоступен**, можно установить PlatformIO в отдельное Python-окружение:

```bash
python3 -m venv ~/.venvs/platformio
~/.venvs/platformio/bin/python -m pip install -U platformio
~/.venvs/platformio/bin/pio run -d firmware -e tdisplay_s3
```

## Управление

| Действие | Результат |
|---|---|
| **GPIO14 / USER** — короткое нажатие | Следующая эмоция |
| **GPIO0 / BOOT** — короткое нажатие после загрузки | Сон / пробуждение |
| USB Serial, 115200 бод | Команды `neutral`, `happy`, `curious`, `surprised`, `sleepy`, `sleep`, `wake`, `next`, `help` |

**Примечание:** BOOT (GPIO0) — strapping pin. Не удерживать его при перезагрузке, кроме входа в режим прошивки.

## Устройство проекта

- `firmware/` — PlatformIO + Arduino framework + официальный display profile LILYGO (LovyanGFX), персонаж и управление.
- `docs/ARCHITECTURE.md` — границы ответственности устройства, приложений и AI Gateway.
- `docs/ROADMAP.md` — поэтапная разработка.

Первые версии принципиально не нуждаются в интернете. API-ключи AI не будут храниться в прошивке ESP32. После аппаратной проверки добавим Wi-Fi, локальное сопряжение и macOS-клиент.

Источники по плате: [официальная документация](https://wiki.lilygo.cc/products/t-display-series/t-display-s3/) и [LILYGO display library](https://github.com/Xinyuan-LILYGO/LilyGo-display-library).

## Электрическая схема прототипа

- [Схема подключения T-Display-S3, микрофона, усилителя, датчиков, PCA9685 и сервоприводов](docs/hardware/WIRING.md) (проектная, ещё не проверена на макете).
