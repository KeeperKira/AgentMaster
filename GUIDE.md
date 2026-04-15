# Напутствие — AgentMaster

> Этот файл — для тех, кто будет продолжать проект после предыдущих авторов.
> Здесь собраны неочевидные вещи, подводные камни и практические советы.

---

## 1. Быстрый старт

### Сборка

```
msbuild AgentMaster.slnx -p:Configuration=Debug -p:Platform=x64
```

Зависимости подключены через **vcpkg** (`C:\vcpkg`). Глобальная интеграция уже настроена —
MSBuild сам находит пакеты. Если vcpkg отсутствует — переустанови:

```
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg.exe integrate install
C:\vcpkg\vcpkg.exe install imgui:x64-windows nlohmann-json:x64-windows
```

### Toolset

Проект настроен на **v143** (Visual Studio 2022 BuildTools).
Исходный проект был на v145 (VS 2026), но на этой машине его нет.
Если установлен другой toolset — поменяй в `AgentMaster.vcxproj`.

### Зависимости

| Пакет | Источник | Примечание |
|---|---|---|
| ImGui 1.92.7 | vcpkg (`imgui:x64-windows`) | Стандартная версия, без docking |
| ImNodes | `external/imnodes/` (git clone) | Нет в vcpkg, подключен вручную |
| nlohmann-json 3.12.0 | vcpkg | Header-only, только JSON |
| ImGui backends (Win32, DX11) | `external/imgui_backends/` | Скопированы из исходников vcpkg |

### Шаблоны узлов (Nodes/)

Узлы больше не захардкожены — они загружаются из JSON-шаблонов в папке `Nodes/`.
Каждый `.json` файл описывает один тип узла:

```json
{
  "type": "router",
  "name": "Router",
  "fixed": false,
  "ports": { "input": 1, "output": 1 },
  "fields": { "name": "Router", "model": "", "url": "", "api_key": "" },
  "ui": { "text_inputs": ["model", "url", "api_key"], "input_width": 180 }
}
```

**Поля шаблона:**

| Поле | Тип | Описание |
|---|---|---|
| `type` | string | Уникальный идентификатор типа (`"input"`, `"router"`, ...) |
| `name` | string | Отображаемое имя в каталоге |
| `fixed` | bool | Если `true` — узел нельзя удалить (Input/Output) |
| `ports.input` | int | Количество входных портов |
| `ports.output` | int | Количество выходных портов |
| `fields` | object | Поля по умолчанию (ключ → значение) |
| `ui.text_multiline` | bool | Многострочное поле (для Text) |
| `ui.text_inputs` | array | Список текстовых полей (для Router, Logger...) |
| `ui.checkbox` | string | Имя поля-чекбокса (для Concat) |

**Текущие шаблоны:**

| Файл | Тип | Входы | Выходы | Описание |
|---|---|---|---|---|
| `input.json` | input | 0 | 1 | Точка входа (fixed) |
| `output.json` | output | 1 | 1 | Точка выхода (fixed) |
| `text.json` | text | 0 | 1 | Текстовый блок |
| `triplet.json` | triplet | 3 | 1 | Блок контекста |
| `router.json` | router | 1 | 1 | Маршрутизация к API |
| `concat.json` | concat | 2 | 1 | Объединение потоков |
| `logger.json` | logger | 1 | 1 | Логирование |
| `gate.json` | gate | 2 | 1 | Шлюз с условием |
| `Sum.json` | Summa | 2 | 1 | Сумматор |

---

## 2. Известные проблемы и workaround'и

### DirectX 11: D3D11CreateDeviceAndSwapChain не работает

На машине может не быть аппаратного GPU или драйверов. Код уже имеет 3 fallback'а:
1. `D3D_DRIVER_TYPE_HARDWARE` (аппаратный)
2. `D3D_DRIVER_TYPE_WARP` (программный)
3. `D3D_DRIVER_TYPE_REFERENCE` (очень медленный, но работает)

### ImGui 1.92+: `ImGui_ImplWin32_WndProcHandler` не объявлен

В новых версиях ImGui эта функция закомментирована в `#if 0`.
Решение — ручное объявление в `AgentMaster.cpp`:

```cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
```

### `ImGuiConfigFlags_DockingEnable` недоступен

Стандартный vcpkg-пакет ImGui собран **без docking**. Если нужен docking —
собирай ImGui из ветки `docking` вручную или подключай docking-experimental.

### Кириллица в ImGui

Стандартный шрифт ImGui (`Proggy Clean`) не поддерживает кириллицу.
Решение — загрузка `arial.ttf` с `GetGlyphRangesCyrillic()` (уже в коде).

### Размещение узлов: click-to-place, а не drag-and-drop

ImGui drag-and-drop (`BeginDragDropSource`/`BeginDragDropTarget`) не работает
совместимо с `ImNodes::BeginNodeEditor()` — ImNodes перехватывает события мыши.

Реализован альтернативный подход: **клик по каталогу → клик по canvas**.
Работает стабильно.

---

## 3. Как добавлять новые типы узлов

Теперь узлы добавляются **без изменения кода** — просто создайте JSON-шаблон в папке `Nodes/`.

### 3.1. Создать JSON-файл

```json
// Nodes/mytype.json
{
  "type": "mytype",
  "name": "MyType",
  "fixed": false,
  "ports": { "input": 1, "output": 1 },
  "fields": { "name": "MyType", "my_field": "" },
  "ui": { "text_inputs": ["my_field"], "input_width": 180 }
}
```

### 3.2. Перезапустить приложение

NodeFactory автоматически загрузит новый шаблон при старте.

**Поддерживаемые UI-элементы:**

| UI-тип | JSON-поле | Описание |
|---|---|---|
| Многострочный текст | `"ui": { "text_multiline": true, "text_size": [180, 80] }` | Поле `text` в fields |
| Текстовые поля | `"ui": { "text_inputs": ["field1", "field2"], "input_width": 180 }` | Список полей |
| Чекбокс | `"ui": { "checkbox": "wait" }` | Имя поля в fields |

---

## 4. Структура проекта

```
AgentMaster/
├── AgentMaster.cpp          # Точка входа, UI-цикл, инициализация
├── AgentMaster.h            # extern HWND g_hWnd
├── AgentMaster.vcxproj      # MSBuild проект
├── AgentMaster.rc           # Ресурсы Windows (иконки, диалоги)
│
├── Model/
│   ├── Node.hpp             # Единый класс Node (компоненты: Identity, Fields, Ports, UIConfig)
│   ├── Node.cpp             # Реализация Node
│   ├── NodeFactory.hpp      # Фабрика узлов (загрузка из Nodes/)
│   ├── NodeFactory.cpp      # Реализация NodeFactory
│   ├── AgentModel.hpp       # AgentModel + Connection
│   └── AgentModel.cpp       # Управление узлами/связями, JSON
│
├── Nodes/                   # JSON-шаблоны узлов
│   ├── input.json           # Input (fixed)
│   ├── output.json          # Output (fixed)
│   ├── text.json            # Text
│   ├── triplet.json         # Triplet
│   ├── router.json          # Router
│   ├── concat.json          # Concat
│   ├── logger.json          # Logger
│   ├── gate.json            # Gate
│   └── Sum.json             # Summa
│
├── external/
│   ├── imnodes/             # ImNodes (git clone)
│   └── imgui_backends/      # Win32 + DX11 бэкенды
│
├── backup/                  # ZIP-архивы (в .gitignore)
│
├── example/                 # Примеры JSON-схем
│
├── UI_SPEC.md               # Спецификация UI
├── ARCH_SPEC.md             # Архитектура, зависимости, план
├── GUIDE.md                 # Этот файл
├── USER_GUIDE.md            # Руководство пользователя (RU)
│
└── .gitignore               # Исключения (x64, .vs, external, backup)
```

---

## 5. Глобальные переменные (AgentMaster.cpp)

| Переменная | Назначение |
|---|---|
| `g_hWnd` | Дескриптор главного окна |
| `g_model` | `AgentModel` — все узлы и связи |
| `g_nodesPositionSet` | `set<int>` — узлы с уже установленной позицией |
| `g_placingNode` | Флаг режима размещения узла |
| `g_pendingNodeType` | Тип узла, ожидающего размещения |
| `g_placingNodeFrame` | Кадр входа в режим размещения (защита) |
| `g_currentFrame` | Счётчик кадров рендера |
| `g_pendingMouseX/Y` | Позиция мыши для размещения нового узла |

**Важно:** при очистке (выход из приложения) деструктор `AgentModel`
удаляет все узлы. InputNode и OutputNode — `IsFixed() = true` —
удалять нельзя.

---

## 6. Стандарт кодирования

- **Отступы:** табы (`\t`)
- **Фигурные скобки:** с новой строки
- **Именование классов/функций:** PascalCase
- **Именование переменных:** camelCase
- **Члены класса:** `m_` + camelCase (`m_id`, `m_x`)

См. `ARCH_SPEC.md` §2.

---

## 7. Что реализовано и что осталось

**✅ Готово:**
- 9 типов узлов через JSON-шаблоны (Nodes/)
- NodeFactory — динамическая загрузка шаблонов
- Компонентный подход к узлам (Identity, Fields, Ports, UIConfig)
- Создание узлов через click-to-place
- Перемещение узлов
- Связи между портами (создание, удаление по ПКМ на порту или линии)
- Удаление узлов (красная кнопка X)
- Редактирование полей узлов (name, text, url, model, api_key, wait, condition, filename)
- Кириллический шрифт (arial.ttf)
- JSON экспорт/импорт (Save/Load JSON)
- Zoom/Pan через ImNodes
- Стабильные ID связей (не сбиваются при удалении)

**⏳ Можно улучшить:**
- Удаление связей по ПКМ на линии — работает, но нужно убедиться
  что `IsLinkHovered` корректно возвращает `conn.id`
- Позиция размещения узлов при панорамировании/зуме — 
  требует доработки (сейчас `g_pendingMouseX/Y` конвертируется 
  через `editorPos` внутри `BeginNodeEditor`)

**❌ Не реализовано (отложено):**
- Undo/Redo
- Контекстное меню (ПКМ на узле)
- Миникарта
- Дублирование узлов
- Валидация соединений на уровне Model
- Drag-and-drop из каталога

---

## 8. Советы

1. **Не трогай vcpkg глобально** — если проект собирается, не обновляй пакеты.
2. **ImNodes — не самая стабильная библиотека.** Если странные краши — проверь
   что все вызовы внутри `BeginNodeEditor`/`EndNodeEditor`.
3. **Бэкапь чаще.** Git-коммиты + ZIP в `backup/` — минимум два способа.
4. **Читай `ARCH_SPEC.md` и `UI_SPEC.md`** перед изменениями.
5. **Если что-то не собирается** — почисти `x64/` и `AgentMaster/` (Debug/Release)
   и собери заново.
6. **При добавлении нового узла** не забудь обновить `ToJson`/`FromJson`
   в `AgentModel.cpp`.
