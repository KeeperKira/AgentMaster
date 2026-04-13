# Архитектура и программные решения — AgentMaster

> **Дата создания:** 13 апреля 2026
> **Статус:** Утверждено
> **Ответственный:** Bakharev Yuri

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

### 2.2. Пример

```cpp
int Foo(int a, int b)
{
	if (a > b)
	{
		return a;
	}
	else
	{
		return b;
	}
}
```

### 2.3. Именование

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
      "fields": {}
    },
    {
      "id": 2,
      "type": "router",
      "x": 400,
      "y": 200,
      "fields": {
        "name": "Мой роутер",
        "url": "https://api.example.com",
        "api_key": "sk-xxx"
      }
    }
  ],
  "connections": [
    {
      "from_node": 1,
      "from_port": 0,
      "to_node": 2,
      "to_port": 0
    }
  ]
}
```

---

## 3. Архитектура

### 3.1. Модель данных — Наследование

- **Базовый класс `Node`** — общий интерфейс для всех типов
- **Производные классы** — `InputNode`, `OutputNode`, `TextNode`, `TripletNode`, `RouterNode`
- **Поля каждого узла** хранятся в `std::map<std::string, std::string>` (гибко, но без type-safety)
- **Порты** определяются через виртуальные методы `getInputCount()` / `getOutputCount()`
- **Коллекция:** `std::vector<Node*>` (сырые указатели, ручное управление памятью)
- **ID:** автоинкремент (1, 2, 3...)
- **Input/Output:** создаются при старте, не отображаются в каталоге, нельзя удалить, только перемещать

### 3.2. Иерархия классов

```cpp
class Node {
public:
    virtual ~Node() = default;
    int getId() const;
    float getX() const, getY() const;
    void setPos(float x, float y);

    virtual int getInputCount() const = 0;
    virtual int getOutputCount() const = 0;
    virtual const char* getTypeName() const = 0;

    // Поля
    const std::map<std::string, std::string>& getFields() const;
    void setField(const std::string& key, const std::string& value);
    std::string getField(const std::string& key) const;

    bool isFixed() const;  // Input/Output = true

protected:
    Node(int id, bool fixed);
    int m_id;
    float m_x, m_y;
    bool m_fixed;
    std::map<std::string, std::string> m_fields;
};

class InputNode  : public Node { /* getInputCount=0, getOutputCount=1 */ };
class OutputNode : public Node { /* getInputCount=1, getOutputCount=1 */ };
class TextNode   : public Node { /* getInputCount=0, getOutputCount=1 */ };
class TripletNode: public Node { /* getInputCount=3, getOutputCount=1 */ };
class RouterNode : public Node { /* getInputCount=1, getOutputCount=1 */ };
```

### 3.3. Структура Connection

```cpp
struct Connection {
    int from_node_id;
    int from_port_index;
    int to_node_id;
    int to_port_index;
};
```

### 3.4. Хранение

```cpp
class AgentModel {
    std::vector<Node*> m_nodes;
    std::vector<Connection> m_connections;
    int m_next_id = 1;

    Node* createNode(NodeType type);
    void deleteNode(int id);          // Проверка isFixed() перед удалением
    void addConnection(...);
    void serialize(const std::string& path);
    void deserialize(const std::string& path);
};
```

### 3.5. Разделение ответственности

| Компонент | Задача |
|---|---|
| **Model (Node, AgentModel)** | Хранение узлов и связей, сериализация JSON |
| **UI (ImGui/ImNodes)** | Отрисовка, обработка ввода, drag-and-drop |
| **Win32** | Окно, цикл сообщений, инициализация ImGui |

### 3.6. Валидация соединений

- **Только на уровне UI** (ImNodes)
- Model не проверяет правила соединений — просто хранит то, что создал UI
- ImNodes не даст соединить несовместимые порты визуально

---

## 4. Цикл рендера

- Immediate mode — каждый кадр перерисовывает всё
- Встроен в основной цикл Win32
- ImGui обрабатывает события через `ImGui_ImplWin32_WndProcHandler`

---

## 5. Специальные правила

### 5.1. Input и Output

- Всегда присутствуют на canvas (по одному экземпляру)
- Нельзя удалить
- Можно перемещать

### 5.2. Правила соединений

- Один выход → много входов
- Один вход ← только один выход

---

## 6. Чего НЕТ на первом этапе

- [ ] Undo/Redo
- [ ] Контекстное меню (ПКМ)
- [ ] Миникарта
- [ ] Дублирование узлов
- [ ] HTTP-запросы (это делает программа-исполнитель)
- [ ] Тесты (отложены)
- [ ] Валидация соединений на уровне Model

---

## 7. План реализации и прогресс

| # | Шаг | Описание | Статус |
|---|---|---|---|
| 1 | vcpkg + зависимости | Подключить ImGui, ImNodes, nlohmann-json | ✅ Готово |
| 2 | ImGui + Win32 | Интегрировать ImGui в Win32 окно | ✅ Готово |
| 3 | ImNodes | Подключить и инициализировать ImNodes | ✅ Готово |
| 4 | Классы Node и производные | Node, InputNode, OutputNode, TextNode, TripletNode, RouterNode | ✅ Готово |
| 5 | AgentModel | Хранение, создание, удаление, сериализация | 🔄 В работе |
| 6 | Каркас UI | Две панели: каталог + canvas | ✅ Готово |
| 7 | Каталог | Список инструментов с размещением узлов | ✅ Готово (click-to-place) |
| 8 | Создание узлов | Клик по каталогу → клик на canvas | ✅ Готово |
| 9 | Порты и связи | Отрисовка через ImNodes | ⏳ Частично (порты есть, связи — нет) |
| 10 | Удаление | Красный крестик на узле (блокировка для Input/Output) | ❌ Не начато |
| 11 | Zoom/Pan | Колёсико + средняя кнопка мыши | ⏳ Работает из коробки в ImNodes |
| 12 | JSON экспорт | Сериализация через nlohmann/json | ❌ Не начато |
| 13 | Полировка | Финальные штрихи | ❌ Не начато |
