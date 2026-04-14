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

### 3.1. Добавить тип в enum

```cpp
// Model/Node.hpp
enum class NodeType
{
    Input, Output, Text, Triplet, Router, Concat,
    NewType  // <--- добавить
};
```

### 3.2. Создать класс

```cpp
// Model/Node.hpp
class NewTypeNode : public Node
{
public:
    NewTypeNode(int id);
    int GetInputCount() const override;
    int GetOutputCount() const override;
    NodeType GetType() const override;
    const char* GetTypeName() const override;
};
```

### 3.3. Реализовать

```cpp
// Model/Node.cpp
NewTypeNode::NewTypeNode(int id) : Node(id, false)
{
    std::string name = "NewType #" + std::to_string(id);
    SetField("name", name);
    SetField("my_field", "");  // поля по умолчанию
}
```

### 3.4. Добавить в каталог (AgentMaster.cpp)

```cpp
const char* toolNames[] = { "Text", "Triplet", "Router", "Concat", "NewType" };
NodeType toolTypes[] = { NodeType::Text, NodeType::Triplet, NodeType::Router, NodeType::Concat, NodeType::NewType };
```

### 3.5. Добавить в `AgentModel::CreateNode` (Model/AgentModel.cpp)

```cpp
case NodeType::NewType:
    newNode = new NewTypeNode(NextId());
    break;
```

### 3.6. Добавить в `ToJson` и `FromJson` (Model/AgentModel.cpp)

```cpp
// ToJson
case NodeType::NewType:
    nodeJson["type"] = "newtype";
    break;

// FromJson
else if (type == "newtype")
    newNode = new NewTypeNode(id);
```

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
│   ├── Node.hpp             # Базовый Node + 6 производных
│   ├── Node.cpp             # Реализация всех классов
│   ├── AgentModel.hpp       # AgentModel + Connection
│   └── AgentModel.cpp       # Управление узлами/связями, JSON
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
- 6 типов узлов (Input, Output, Text, Triplet, Router, Concat)
- Создание узлов через click-to-place
- Перемещение узлов
- Связи между портами (создание, удаление по ПКМ на порту или линии)
- Удаление узлов (красная кнопка X)
- Редактирование полей узлов (name, text, url, model, api_key, wait)
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
