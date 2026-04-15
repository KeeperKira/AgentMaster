# Архитектура и программные решения — AgentMaster

> **Дата создания:** 13 апреля 2026
> **Дата обновления:** 14 апреля 2026


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

### 4.1. Модель данных — Наследование

- **Базовый класс `Node`** — общий интерфейс для всех типов
- **Производные классы** — `InputNode`, `OutputNode`, `TextNode`, `TripletNode`, `RouterNode`, `ConcatNode`
- **Поля каждого узла** хранятся в `std::map<std::string, std::string>` (гибко, но без type-safety)
- **Порты** определяются через виртуальные методы `GetInputCount()` / `GetOutputCount()`
- **Коллекция:** `std::vector<Node*>` в `AgentModel` (сырые указатели, удаление через деструктор)
- **ID:** автоинкремент (1, 2, 3...)
- **Input/Output:** создаются при старте, не в каталоге, нельзя удалить
- **Имена:** автозаполнение при создании (`"Тип #ID"`), редактируются пользователем

### 4.2. Иерархия классов

```cpp
class Node {
    virtual int GetInputCount() const = 0;
    virtual int GetOutputCount() const = 0;
    virtual NodeType GetType() const = 0;
    virtual const char* GetTypeName() const = 0;

    bool IsFixed() const;  // Input/Output = true
    // Поля: m_fields — std::map<std::string, std::string>
};

class InputNode   : public Node { 0 in,  1 out, name };
class OutputNode  : public Node { 1 in,  1 out, name };
class TextNode    : public Node { 0 in,  1 out, name, text };
class TripletNode : public Node { 3 in,  1 out, name };
class RouterNode  : public Node { 1 in,  1 out, name, model, url, api_key };
class ConcatNode  : public Node { 2 in,  1 out, name, wait };
```

### 4.3. Структура Connection

```cpp
struct Connection {
    int id;             // стабильный ID (автоинкремент)
    int from_node_id;
    int from_port_index;
    int to_node_id;
    int to_port_index;
};
```

### 4.4. Разделение ответственности

| Компонент | Задача |
|---|---|
| **Model (Node, AgentModel)** | Хранение узлов и связей, сериализация JSON |
| **UI (ImGui/ImNodes)** | Отрисовка, обработка ввода |
| **Win32** | Окно, цикл сообщений, инициализация ImGui |

### 4.5. Валидация соединений

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

## 9. Возможные будущие узлы

| Узел | Назначение |
|---|---|
| **Condition** | Ветвление по правилу (contains, regex, length) |
| **Template** | Шаблон промпта с `{input}` плейсхолдером |
| **Splitter** | Разбивка текста на чанки |
| **Parser** | Извлечение данных из JSON/XML |
| **LLM Prompt** | Упрощённый роутер с预设-хедерами |
| **Logger** | Логирование проходящих данных |
| **Delay** | Пауза перед отправкой (rate-limit) |
| **Comment** | Текстовая плашка для пояснений на канвасе |
