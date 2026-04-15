# AgentMaster API — JSON Спецификация

> **Версия:** 15 апреля 2026  
> **Назначение:** Формат JSON для передачи схемы агента от AgentMaster к AgentClient

---

## 1. Обзор

AgentMaster генерирует JSON-файл, описывающий схему агента в виде графа узлов и связей.  
AgentClient парсит этот JSON и реализует выполнение агента.

---

## 2. Структура JSON

```json
{
  "nodes": [
    {
      "id": 1,
      "type": "input",
      "x": 100.5,
      "y": 200.0,
      "fields": { ... }
    },
    ...
  ],
  "connections": [
    {
      "id": 5,
      "from_node": 1,
      "from_port": 0,
      "to_node": 4,
      "to_port": 0
    },
    ...
  ]
}
```

### Поля корневого объекта

| Поле | Тип | Обязательное | Описание |
|---|---|---|---|
| `nodes` | array | ✅ | Список узлов графа |
| `connections` | array | ✅ | Список связей между узлами |

---

## 3. Узлы (nodes)

### Общая структура узла

```json
{
  "id": 1,
  "type": "input",
  "x": 100.5,
  "y": 200.0,
  "fields": { ... }
}
```

| Поле | Тип | Обязательное | Описание |
|---|---|---|---|---|
| `id` | integer | ✅ | Уникальный ID узла (автоинкремент) |
| `type` | string | ✅ | Тип узла (см. ниже) |
| `x` | float | ✅ | Координата X на canvas (grid space) |
| `y` | float | ✅ | Координата Y на canvas (grid space) |
| `fields` | object | ✅ | Значения полей узла |

### Поддерживаемые типы узлов

| Тип | Описание | Входы | Выходы | Поля fields |
|---|---|---|---|---|
| `input` | Точка входа запроса | 0 | 1 | `name` (string) |
| `output` | Точка выхода ответа | 1 | 1 | `name` (string) |
| `text` | Текстовый блок | 0 | 1 | `name`, `text` (multiline) |
| `triplet` | Блок контекста (3 входа) | 3 | 1 | `name` |
| `router` | Маршрутизация к API | 1 | 1 | `name`, `model`, `url`, `api_key` |
| `concat` | Объединение потоков | 2 | 1 | `name`, `wait` (boolean string) |
| `logger` | Логирование данных | 1 | 1 | `name`, `filename` |
| `gate` | Шлюз с условием | 2 | 1 | `name`, `condition` |
| `summa` | Сумматор | 2 | 1 | `name`, `condition` |

### Примеры узлов

#### Input
```json
{
  "id": 1,
  "type": "input",
  "x": 100.0,
  "y": 200.0,
  "fields": {
    "name": "Входные данные"
  }
}
```

#### Router
```json
{
  "id": 5,
  "type": "router",
  "x": 300.0,
  "y": 150.0,
  "fields": {
    "name": "Мой роутер",
    "model": "qwen/qwen3.6plus",
    "url": "https://api.example.com/v1/chat",
    "api_key": "sk-xxx"
  }
}
```

#### Text (с многострочным полем)
```json
{
  "id": 6,
  "type": "text",
  "x": 50.0,
  "y": 100.0,
  "fields": {
    "name": "Промпт",
    "text": "Вы — полезный ассистент.\nОтвечай кратко."
  }
}
```

#### Concat (с флагом wait)
```json
{
  "id": 4,
  "type": "concat",
  "x": 400.0,
  "y": 300.0,
  "fields": {
    "name": "Объединение",
    "wait": "true"
  }
}
```

> **Примечание:** Все числовые поля в JSON — это строки в `fields` (даже `wait: "true"`).

---

## 4. Связи (connections)

### Структура связи

```json
{
  "id": 5,
  "from_node": 1,
  "from_port": 0,
  "to_node": 4,
  "to_port": 0
}
```

| Поле | Тип | Обязательное | Описание |
|---|---|---|---|---|
| `id` | integer | ✅ | Уникальный ID связи (автоинкремент) |
| `from_node` | integer | ✅ | ID узла-источника |
| `from_port` | integer | ✅ | Индекс выходного порта (0-based) |
| `to_node` | integer | ✅ | ID узла-приёмника |
| `to_port` | integer | ✅ | Индекс входного порта (0-based) |

### Правила соединений

- **Направление:** только от выхода к входу (`from_node` → `to_node`)
- **Один выход → много входов:** разрешено
- **Один вход ← один выход:** разрешено (множественные входы допустимы)

### Примеры связей

```json
{
  "id": 17,
  "from_node": 4,
  "from_port": 0,
  "to_node": 3,
  "to_port": 2
}
```

Это означает:
- Выходной порт 0 узла с ID 4 соединён с
- Входным портом 2 узла с ID 3

---

## 5. Алгоритм выполнения агента (для разработчика клиента)

### 5.1. Парсинг JSON

1. Загрузить JSON-файл
2. Создать словарь `nodesById` для быстрого доступа к узлам
3. Создать граф зависимостей: для каждого узла определить, от каких узлов зависит его ввод

### 5.2. Порядок выполнения

```
1. Найти узел Input (type == "input")
2. Выполнить Input → получить данные
3. Для каждого узла, зависящего от Input:
   a. Собрать входные данные из всех входных портов
   b. Выполнить узел с данными
   c. Результат сохранить для следующих узлов
4. Повторить, пока не достигнем Output
```

### 5.3. Типы обработки узлов

#### Input
- Возвращает входные данные от пользователя

#### Text
- Возвращает значение поля `text` (можно интерполировать переменные)

#### Router
- Выполняет HTTP-запрос к `url` с заголовком `Authorization: Bearer {api_key}`
- В теле запроса отправляет `{"model": "{model}", "messages": [...]}`

#### Triplet
- Собирает данные из 3 входов в структуру `{subject, predicate, object}`

#### Concat
- Если `wait == "true"` — ждёт данные из обоих входов
- Если `wait == "false"` — использует первый полученный поток
- Объединяет данные (конкатенация или merge)

#### Logger
- Записывает данные в файл `filename` (по умолчанию `agent.log`)

#### Gate
- Применяет условие `condition` к входным данным
- Если условие истинно — пропускает данные, иначе блокирует

#### Summa
- Собирает данные из 2 входов
- Применяет условие `condition` (специфично для реализации)

#### Output
- Формирует финальный ответ из полученных данных

### 5.4. Поток данных

```
Input → [данные] → Router → [ответ API] → Output
         ↓
      Triplet ← [контекст]
         ↓
      Concat (wait=true)
         ↓
      Router
         ↓
      Output
```

---

## 6. Пример полного JSON

```json
{
  "nodes": [
    {
      "id": 1,
      "type": "input",
      "x": 100,
      "y": 200,
      "fields": { "name": "User Query" }
    },
    {
      "id": 2,
      "type": "text",
      "x": 100,
      "y": 300,
      "fields": {
        "name": "System Prompt",
        "text": "Вы — полезный ассистент."
      }
    },
    {
      "id": 3,
      "type": "triplet",
      "x": 300,
      "y": 250,
      "fields": { "name": "Context" }
    },
    {
      "id": 4,
      "type": "router",
      "x": 500,
      "y": 200,
      "fields": {
        "name": "LLM",
        "model": "qwen/qwen3.6plus",
        "url": "https://api.example.com/chat",
        "api_key": "sk-xxx"
      }
    },
    {
      "id": 5,
      "type": "output",
      "x": 700,
      "y": 200,
      "fields": { "name": "Response" }
    }
  ],
  "connections": [
    {
      "id": 1,
      "from_node": 1,
      "from_port": 0,
      "to_node": 4,
      "to_port": 0
    },
    {
      "id": 2,
      "from_node": 2,
      "from_port": 0,
      "to_node": 3,
      "to_port": 0
    },
    {
      "id": 3,
      "from_node": 3,
      "from_port": 0,
      "to_node": 4,
      "to_port": 0
    },
    {
      "id": 4,
      "from_node": 4,
      "from_port": 0,
      "to_node": 5,
      "to_port": 0
    }
  ]
}
```

---

## 7. Ошибки и валидация

### Валидация JSON

AgentClient должен проверять:
- ✅ Наличие полей `nodes` и `connections`
- ✅ Уникальность `id` узлов
- ✅ Существование `from_node` и `to_node` для каждой связи
- ✅ Корректность `type` узла (из допустимого списка)
- ✅ Наличие обязательных полей для каждого типа

### Обработка ошибок

| Ошибка | Действие |
|---|---|
| Несуществующий узел в связи | Пропустить связь, логировать |
| Неподдерживаемый тип узла | Пропустить узел, логировать |
| Отсутствует Input/Output | Создать по умолчанию или ошибка |
| Ошибка выполнения узла | Продолжить с пустым результатом или остановить |

---

## 8. Версия формата

| Версия | Дата | Изменения |
|---|---|---|
| 1.0 | 15.04.2026 | Начальная версия с 9 типами узлов |

---

## 9. Примеры реализации

### JavaScript/TypeScript (Node.js)

```ts
interface Node {
  id: number;
  type: string;
  x: number;
  y: number;
  fields: Record<string, string>;
}

interface Connection {
  id: number;
  from_node: number;
  from_port: number;
  to_node: number;
  to_port: number;
}

interface AgentSchema {
  nodes: Node[];
  connections: Connection[];
}

function loadAgent(path: string): AgentSchema {
  const content = fs.readFileSync(path, 'utf8');
  return JSON.parse(content);
}

function executeAgent(schema: AgentSchema, userInput: string): Promise<string> {
  // Реализация выполнения...
}
```

### Python

```python
import json
from typing import Dict, List, Any

class Node:
    def __init__(self, id: int, type: str, x: float, y: float, fields: Dict[str, str]):
        self.id = id
        self.type = type
        self.x = x
        self.y = y
        self.fields = fields

class Connection:
    def __init__(self, id: int, from_node: int, from_port: int, to_node: int, to_port: int):
        self.id = id
        self.from_node = from_node
        self.from_port = from_port
        self.to_node = to_node
        self.to_port = to_port

class AgentSchema:
    def __init__(self, nodes: List[Node], connections: List[Connection]):
        self.nodes = nodes
        self.connections = connections

def load_agent(path: str) -> AgentSchema:
    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    nodes = [Node(**n) for n in data['nodes']]
    connections = [Connection(**c) for c in data['connections']]
    return AgentSchema(nodes, connections)
```

---

## 10. Часто задаваемые вопросы

**Q: Почему поля в `fields` — это всегда строки?**  
A: Для простоты сериализации. Булевы значения (как `wait`) сохраняются как `"true"`/`"false"`.

**Q: Можно ли добавить свои типы узлов?**  
A: Да, но AgentClient должен знать, как их обрабатывать. Используйте уникальные имена типов.

**Q: Как обрабатывать циклы в графе?**  
A: Рекомендуется запретить циклы или использовать топологическую сортировку с проверкой.

**Q: Поддерживается ли потоковая передача данных?**  
A: В текущей версии — нет. Все данные передаются целиком. Для потоков нужна расширенная версия API.
