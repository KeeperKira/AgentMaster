# AgentMaster

Визуальный редактор агентов для нейронных сетей. Создавайте маршруты обработки запросов с помощью drag-and-drop интерфейса.

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![C++](https://img.shields.io/badge/C++-20-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## Возможности

- **Визуальный граф-редактор** — создавайте агентов из узлов и связей
- **9 типов узлов** (шаблоны в `Nodes/`):
  - **Input** — точка входа запроса от пользователя
  - **Output** — точка выхода ответа
  - **Text** — текстовый блок (многострочное поле)
  - **Router** — маршрутизация к внешнему API (модель, URL, API ключ)
  - **Triplet** — блок контекста (3 входа, 1 выход)
  - **Concat** — объединение двух потоков данных (флаг Wait)
  - **Logger** — логирование проходящих данных в файл
  - **Gate** — шлюз с условием (2 входа, 1 выход)
  - **Summa** — сумматор (2 входа, 1 выход, поле condition)
- **Экспорт/импорт JSON** — сохранение и загрузка схем агентов
- **Кириллица** — полная поддержка русского языка

## Сборка

### Зависимости

```bash
# vcpkg
vcpkg install imgui:x64-windows
vcpkg install nlohmann-json:x64-windows

# ImNodes (вручную)
git clone https://github.com/Nelarius/imnodes.git external/imnodes
```

### Шаблоны узлов

Узлы загруются из JSON-шаблонов в папке `Nodes/`. Каждый шаблон определяет:
- Тип узла, порты, поля по умолчанию
- UI-конфигурацию (многострочное поле, текстовые поля, чекбокс)

### Требования

- **C++20**
- **Visual Studio 2022** (v143 toolset) или MSBuild
- **vcpkg** (интеграция через MSBuild)

### Команда сборки

```bash
msbuild AgentMaster.slnx -p:Configuration=Release -p:Platform=x64
```

## Использование

1. Запустите `AgentMaster.exe`
2. Перетащите инструменты из каталога (левая панель) на рабочее поле
3. Соединяйте порты узлов (вход ↔ выход)
4. Сохраните схему: **File → Save JSON...**

## Примеры

### Простой чат с ИИ

```
Input → Router → Output
```

### Чат с сохранением контекста

```
Input → Triplet → Router → Output
                   ↖______↙
```

## Архитектура

| Компонент | Описание |
|---|---|
| **Model** | `Node` (компонентный подход), `AgentModel`, `NodeFactory` — хранение, фабрикация, сериализация |
| **UI** | ImGui + ImNodes — отрисовка, обработка ввода |
| **Win32** | Окно, цикл сообщений, инициализация DirectX 11 |
| **Nodes/** | JSON-шаблоны для динамического создания узлов |

## Зависимости

| Библиотека | Назначение |
|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate mode GUI |
| [ImNodes](https://github.com/Nelarius/imnodes) | Node editor для ImGui |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON сериализация |

## Лицензия

MIT
