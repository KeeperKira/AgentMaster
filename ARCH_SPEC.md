# Архитектура и программные решения — AgentMaster

> **Дата создания:** 13 апреля 2026
> **Дата обновления:** 15 апреля 2026


---

## 1. Зависимости

### 1.1. Управление пакетами

- **Менеджер:** vcpkg
- **Интеграция:** MSBuild (vcxproj)

### 1.2. Список пакетов

```
vcpkg install:
├── imgui:x64-windows          # Dear ImGui (v1.92.7)
├── nlohmann-json:x64-windows  # JSON (v3.12.0)

external/ (вручную):
└── imnodes/                   # ImNodes (git clone из Nelarius/imnodes)

Nodes/ (JSON-шаблоны):
├── input.json                 # Input (fixed)
├── output.json                # Output (fixed)
├── text.json                  # Text
├── triplet.json               # Triplet
├── router.json                # Router
├── concat.json                # Concat
├── logger.json                # Logger
├── gate.json                  # Gate
└── Sum.json                   # Summa
```

### Статус зависимостей

| Зависимость | Источник | Статус |
|---|---|---|
| **ImGui** | vcpkg (`imgui:x64-windows`) | ✅ Установлена |
| **nlohmann-json** | vcpkg (`nlohmann-json:x64-windows`) | ✅ Установлена |
| **ImNodes** | external/ (git clone) | ✅ Добавлена вручную |

---

## 2. Стандарт кодирования

### 2.1. Стиль оформления

- **Стандарт C++:** C++20
- **Отступы:** Табы (`\t`)
- **Фигурные скобки:** С новой строки
- **Условные операторы:** `if` и тело — на разных строках

### 2.2. Именование

| Существо | Стиль | Пример |
|---|---|---|
| Классы | PascalCase | `Node`, `AgentModel` |
| Функции | PascalCase | `GetInputCount()`, `Serialize()` |
| Переменные | camelCase | `nodeId`, `fieldName` |
| Члены класса | `m_` + camelCase | `m_id`, `m_x`, `m_fields` |
| Константы | UPPER_SNAKE_CASE | `MAX_NODES`, `DEFAULT_PORT_COUNT` |
| Перечисления | PascalCase + camelCase значения | `enum class NodeType { input, router }` |

---

## 3. JSON

| Параметр | Значение |
|---|---|
| **Библиотека** | nlohmann/json |
| **Тип** | Header-only (один .hpp) |
| **Использование** | Сериализация/десериализация схемы агента |

### Формат JSON (экспорт)

```json
{
  "nodes": [
    {
      "id": 1,
      "type": "input",
      "x": 100,
      "y": 200,
      "fields": { "name": "Input #1" }
    },
    {
      "id": 2,
      "type": "router",
      "x": 400,
      "y": 200,
      "fields": {
        "name": "Router #2",
        "model": "qwen/qwen3.6plus",
        "url": "https://api.example.com",
        "api_key": "sk-xxx"
      }
    }
  ],
  "connections": [
    {
      "id": 3,
      "from_node": 1,
      "from_port": 0,
      "to_node": 2,
      "to_port": 0
    }
  ]
}
```

> **Примечание:** связи имеют стабильный `id` (автоинкремент),
> который не сбивается при удалении других связей.

---

## 4. Архитектура

### 4.1. Модель данных — Компонентный подход

- **Единый класс `Node`** — вместо наследования, узлы состоят из компонентов
- **Компоненты:** `IdentityComponent`, `FieldsComponent`, `PortsComponent`, `UIConfig`
- **Поля каждого узла** хранятся в `FieldsComponent::fields` — `std::map<std::string, std::string>`
- **NodeFactory** — загружает JSON-шаблоны из `Nodes/`, создаёт узлы динамически
- **Порты** определяются из шаблона (`ports.input`, `ports.output`)
- **Коллекция:** `std::vector<Node*>` в `AgentModel` (сырые указатели, удаление через деструктор)
- **ID:** автоинкремент (1, 2, 3...)
- **Input/Output:** создаются при старте, не в каталоге, нельзя удалить
- **Имена:** автозаполнение при создании (`"Тип #ID"`), редактируются пользователем

### 4.2. Компоненты узла

```cpp
// IdentityComponent — идентичность
struct IdentityComponent {
    int id = 0;
    NodeType type = NodeType::Input;
    std::string typeName;     // "input", "router", "text"...
    std::string displayName;  // "Input", "Router", "Text"...
    bool fixed = false;       // нельзя удалить
};

// FieldsComponent — данные
struct FieldsComponent {
    std::map<std::string, std::string> fields;
    void Set(const std::string& key, const std::string& value);
    std::string Get(const std::string& key) const;
};

// PortsComponent — порты
struct PortsComponent {
    int inputCount = 0;
    int outputCount = 0;
};

// UIConfig — отрисовка тела
struct UIConfig {
    enum class ContentType { None, TextMultiline, TextInputs, Checkbox };
    ContentType contentType = ContentType::None;
    // ... детали для каждого типа контента
};

// Node — единый класс (композиция вместо наследования)
class Node {
    IdentityComponent m_identity;
    FieldsComponent m_fields;
    PortsComponent m_ports;
    UIConfig m_uiConfig;
    float m_x = 0, m_y = 0;
};
```

### 4.3. NodeFactory

```cpp
class NodeFactory {
    static bool Initialize(const std::string& nodesDir);  // загрузка Nodes/*.json
    static Node* CreateNodeByTypeName(const std::string& typeName);
    static std::vector<std::string> GetAllTemplateNames(); // для каталога
    static bool IsTemplateFixed(const std::string& typeName);
    // ...
private:
    struct NodeTemplate { /* тип, порты, поля, UI */ };
    static std::map<std::string, NodeTemplate> s_templates;
};
```

### 4.4. Структура Connection

```cpp
struct Connection {
    int id;             // стабильный ID (автоинкремент)
    int from_node_id;
    int from_port_index;
    int to_node_id;
    int to_port_index;
};
```

### 4.5. Разделение ответственности

| Компонент | Задача |
|---|---|
| **Model (Node, AgentModel)** | Хранение узлов и связей, сериализация JSON |
| **UI (ImGui/ImNodes)** | Отрисовка, обработка ввода |
| **Win32** | Окно, цикл сообщений, инициализация ImGui |

### 4.6. Валидация соединений

- **Только на уровне UI** (ImNodes)
- Model не проверяет правила соединений — просто хранит
- `IsLinkCreated` вызывается **после** `EndNodeEditor()`

---

## 5. Цикл рендера

- Immediate mode — каждый кадр перерисовывает всё
- Встроен в основной цикл Win32
- `ImNodes::Link` вызывается каждый кадр (не условно)
- `IsLinkCreated` — после `EndNodeEditor()`

---

## 6. Специальные правила

### 6.1. Input и Output

- Всегда присутствуют на canvas (по одному экземпляру)
- Нельзя удалить
- Можно перемещать
- При JSON загрузке — обновляются вместо создания дубликатов
- `fixed: true` в JSON-шаблоне

### 6.2. Правила соединений

- Один выход → много входов
- Один вход ← только один выход
- ПКМ на порту → удаляет все связи этого порта
- ПКМ на линии → удаляет конкретную связь

### 6.3. Шрифт

- Загружается `C:\Windows\Fonts\arial.ttf` с `GetGlyphRangesCyrillic()`
- Без этого кириллица отображается как `????`

---

## 7. Чего НЕТ

- [ ] Undo/Redo
- [ ] Контекстное меню (ПКМ на узле)
- [ ] Миникарта
- [ ] Дублирование узлов
- [ ] HTTP-запросы (это делает программа-исполнитель)
- [ ] Тесты (отложены)
- [ ] Валидация соединений на уровне Model
- [ ] Корректное центрирование узла при размещении с учётом зума/пана
  (позиция мыши конвертируется, но смещение может «плыть» при зуме)

---

## 8. План реализации и прогресс

| # | Шаг | Описание | Статус |
|---|---|---|---|
| 1 | vcpkg + зависимости | Подключить ImGui, ImNodes, nlohmann-json | ✅ Готово |
| 2 | ImGui + Win32 | Интегрировать ImGui в Win32 окно | ✅ Готово |
| 3 | ImNodes | Подключить и инициализировать ImNodes | ✅ Готово |
| 4 | Классы Node (+ Concat) | Node + 6 производных | ✅ Готово |
| 5 | AgentModel | Хранение, создание, удаление, сериализация | ✅ Готово |
| 6 | Каркас UI | Две панели: каталог + canvas | ✅ Готово |
| 7 | Каталог | 6 инструментов с размещением узлов | ✅ Готово |
| 8 | Создание узлов | Клик по каталогу → клик на canvas | ✅ Готово |
| 9 | Порты и связи | Отрисовка, создание, удаление | ✅ Готово |
| 10 | Удаление узлов | Красная кнопка X (кроме Input/Output) | ✅ Готово |
| 11 | Zoom/Pan | Колёсико + средняя кнопка мыши | ✅ Работает |
| 12 | JSON экспорт/импорт | Сериализация через nlohmann/json | ✅ Готово |
| 13 | Кириллица | arial.ttf + GetGlyphRangesCyrillic() | ✅ Готово |
| 14 | Автоимена узлов | `"Тип #ID"` при создании | ✅ Готово |
| 15 | Полировка | Удаление связей ПКМ, тихий Save/Load | ✅ Готово |

---

## 9. Реализованные и возможные будущие узлы

### ✅ Реализованы

| Узел | Назначение |
|---|---|
| **Input** | Точка входа (fixed) |
| **Output** | Точка выхода (fixed) |
| **Text** | Текстовый блок |
| **Triplet** | Блок контекста (3 входа) |
| **Router** | Маршрутизация к API |
| **Concat** | Объединение потоков |
| **Logger** | Логирование данных |
| **Gate** | Шлюз с условием |
| **Summa** | Сумматор |

### ⏳ Возможные будущие

| Узел | Назначение |
|---|---|
| **Condition** | Ветвление по правилу (contains, regex, length) |
| **Template** | Шаблон промпта с `{input}` плейсхолдером |
| **Splitter** | Разбивка текста на чанки |
| **Parser** | Извлечение данных из JSON/XML |
| **LLM Prompt** | Упрощённый роутер с预设-хедерами |
| **Delay** | Пауза перед отправкой (rate-limit) |
| **Comment** | Текстовая плашка для пояснений на канвасе |
