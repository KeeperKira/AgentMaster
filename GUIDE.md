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

### ImNodes: `SetNodeScreenSpacePos` vs `SetNodeGridSpacePos`

- **ScreenSpacePos** — экранные координаты (меняются при зуме/панорамировании)
- **GridSpacePos** — координаты сетки canvas (стабильные)

**Правило:** вызывай `SetNodeGridSpacePos` один раз при создании узла.
Каждый кадр синхронизируй позицию обратно через `GetNodeGridSpacePos`.

### ImNodes: узлы должны быть внутри `ImGui::Begin()`/`End()`

`BeginNodeEditor()` **обязательно** вызывается внутри окна ImGui.
Если вызвать без окна — будет access violation (`window->ID` на nullptr).

### Размещение узлов: click-to-place, а не drag-and-drop

ImGui drag-and-drop (`BeginDragDropSource`/`BeginDragDropTarget`) не работает
совместно с `ImNodes::BeginNodeEditor()` — ImNodes перехватывает события мыши.

Реализован альтернативный подход: **клик по каталогу → клик по canvas**.
Работает стабильно. Если вернёшься к drag-and-drop — убедись что
`BeginDragDropTarget` работает внутри `BeginChild` (не всегда).

---

## 3. Как добавлять новые типы узлов

### 3.1. Добавить тип в enum

```cpp
// Model/Node.hpp
enum class NodeType
{
    Input, Output, Text, Triplet, Router,
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
    SetField("my_field", "");  // поля по умолчанию
}
```

### 3.4. Добавить в каталог (AgentMaster.cpp)

```cpp
const char* toolNames[] = { "Text", "Triplet", "Router", "NewType" };
NodeType toolTypes[] = { NodeType::Text, NodeType::Triplet, NodeType::Router, NodeType::NewType };
```

### 3.5. Добавить в switch размещения

```cpp
case NodeType::NewType:
    newNode = new NewTypeNode(g_nextNodeId++);
    break;
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
│   ├── Node.hpp             # Базовый Node + 5 производных
│   └── Node.cpp             # Реализация всех классов
│
├── external/
│   ├── imnodes/             # ImNodes (git clone)
│   └── imgui_backends/      # Win32 + DX11 бэкенды
│
├── backup/                  # ZIP-архивы (в .gitignore)
│
├── UI_SPEC.md               # Спецификация UI
├── ARCH_SPEC.md             # Архитектура, зависимости, план
├── GUIDE.md                 # Этот файл
│
└── .gitignore               # Исключения (x64, .vs, external, backup)
```

---

## 5. Глобальные переменные (AgentMaster.cpp)

| Переменная | Назначение |
|---|---|
| `g_hWnd` | Дескриптор главного окна |
| `g_nodes` | `vector<Node*>` — все узлы на canvas |
| `g_nextNodeId` | Автоинкремент ID для новых узлов |
| `g_nodesPositionSet` | `set<int>` — узлы с уже установленной позицией |
| `g_placingNode` | Флаг режима размещения узла |
| `g_pendingNodeType` | Тип узла, ожидающего размещения |
| `g_placingNodeFrame` | Кадр входа в режим размещения (защита от мгновенного срабатывания) |
| `g_currentFrame` | Счётчик кадров рендера |

**Важно:** при очистке (выход из приложения) все узлы удаляются через `delete`.
InputNode и OutputNode — `IsFixed() = true` — удалять нельзя.

---

## 6. Стандарт кодирования

- **Отступы:** табы (`\t`)
- **Фигурные скобки:** с новой строки
- **Именование классов/функций:** PascalCase
- **Именование переменных:** camelCase
- **Члены класса:** `m_` + camelCase (`m_id`, `m_x`)

См. `ARCH_SPEC.md` §2.

---

## 7. Что ещё нужно реализовать

Актуальный статус — в `ARCH_SPEC.md` §7. Ключевые вещи:

- [ ] **Связи между портами** — ImNodes их поддерживает, но логика соединений
  и хранение в модели не реализованы
- [ ] **Удаление узлов** — красная кнопка-крестик на узле
- [ ] **JSON экспорт/импорт** — nlohmann/json уже подключён
- [ ] **Редактирование полей узлов** — name, url, api_key и т.д.
- [ ] **Drag-and-drop из каталога** — если получится обойти конфликт с ImNodes

---

## 8. Советы

1. **Не трогай vcpkg глобально** — если проект собирается, не обновляй пакеты.
   Версии могут сломать совместимость.
2. **ImNodes — не самая стабильная библиотека.** Если странные краши — проверь
   что все вызовы внутри `BeginNodeEditor`/`EndNodeEditor`.
3. **Бэкапь чаще.** Git-коммиты + ZIP в `backup/` — минимум два способа.
4. **Читай `ARCH_SPEC.md` и `UI_SPEC.md`** перед изменениями — там зафиксированы
   принятые решения и причины.
5. **Если что-то не собирается** — почисти `x64/` и `AgentMaster/` (Debug/Release
   папки) и собери заново.
