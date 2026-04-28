#include "AgentModel.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

// ============================================================================
// AgentModel — конструктор/деструктор
// ============================================================================

AgentModel::AgentModel()
{
}

AgentModel::~AgentModel()
{
	ClearAll();
}

// ============================================================================
// Доступ к данным
// ============================================================================

const std::vector<std::unique_ptr<Node>>& AgentModel::GetNodes() const
{
	return m_nodes;
}

Node* AgentModel::GetNodeById(int id) const
{
	for (const auto& node : m_nodes)
	{
		if (node->GetId() == id)
		{
			return node.get();
		}
	}
	return nullptr;
}

const std::vector<Connection>& AgentModel::GetConnections() const
{
	return m_connections;
}

// ============================================================================
// Связи
// ============================================================================

void AgentModel::AddConnection(const Connection& conn)
{
	// Проверка: узлы существуют
	Node* fromNode = GetNodeById(conn.from_node_id);
	Node* toNode = GetNodeById(conn.to_node_id);
	if (!fromNode || !toNode)
		return;

	// Проверка: порты валидны
	if (conn.from_port_index < 0 || conn.from_port_index >= fromNode->Ports().GetOutputCount())
		return;
	if (conn.to_port_index < 0 || conn.to_port_index >= toNode->Ports().GetInputCount())
		return;

	// Проверка: входной порт уже занят?
	for (const auto& existing : m_connections)
	{
		if (existing.to_node_id == conn.to_node_id && existing.to_port_index == conn.to_port_index)
		{
			return; // Один вход — одна связь
		}
	}

	// Проверка на дубликат
	for (const auto& existing : m_connections)
	{
		if (existing == conn)
		{
			return;
		}
	}
	Connection newConn = conn;
	newConn.id = m_next_id++;
	m_connections.push_back(newConn);
}

void AgentModel::RemoveConnection(int from_node_id, int to_node_id)
{
	m_connections.erase(
		std::remove_if(m_connections.begin(), m_connections.end(),
			[from_node_id, to_node_id](const Connection& conn)
			{
				return conn.from_node_id == from_node_id && conn.to_node_id == to_node_id;
			}),
		m_connections.end()
	);
}

void AgentModel::RemoveConnectionById(int conn_id)
{
	m_connections.erase(
		std::remove_if(m_connections.begin(), m_connections.end(),
			[conn_id](const Connection& conn)
			{
				return conn.id == conn_id;
			}),
		m_connections.end()
	);
}

// ============================================================================
// Создание узлов (NodeFactory — строковое имя типа)
// ============================================================================

Node* AgentModel::CreateNode(const std::string& typeName)
{
	auto newNode = NodeFactory::CreateNodeByTypeName(typeName);

	if (!newNode)
	{
		return nullptr;
	}

	// Назначить ID
	newNode->Identity().id = NextId();

	Node* raw = newNode.get();
	m_nodes.push_back(std::move(newNode));
	return raw;
}

// ============================================================================
// Удаление узлов
// ============================================================================

bool AgentModel::DeleteNode(int id)
{
	for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		if ((*it)->GetId() == id)
		{
			if ((*it)->IsFixed())
			{
				return false; // Нельзя удалить Input/Output
			}

			// Удалить все связи с этим узлом
			m_connections.erase(
				std::remove_if(m_connections.begin(), m_connections.end(),
					[id](const Connection& conn)
					{
						return conn.from_node_id == id || conn.to_node_id == id;
					}),
				m_connections.end()
			);

			m_nodes.erase(it);
			return true;
		}
	}
	return false;
}

// ============================================================================
// Очистка
// ============================================================================

void AgentModel::ClearAll()
{
	m_nodes.clear(); // unique_ptr удалит объекты
	m_connections.clear();
	m_next_id = 1;
	InitDefaults();
}
// ============================================================================
// Сериализация — JSON экспорт
// ============================================================================

std::string AgentModel::ToJson() const
{
	json j;
	j["nodes"] = json::array();
	j["connections"] = json::array();

	// Узлы
	for (const auto& node : m_nodes)
	{
		json nodeJson;
		nodeJson["id"] = node->GetId();

		// Тип узла — строковое имя из IdentityComponent
		nodeJson["type"] = node->Identity().typeName;

		nodeJson["x"] = node->GetX();
		nodeJson["y"] = node->GetY();

		// Поля
		json fieldsJson;
		const auto& fields = node->Fields().fields;
		for (const auto& [key, value] : fields)
		{
			fieldsJson[key] = value;
		}
		nodeJson["fields"] = fieldsJson;

		j["nodes"].push_back(nodeJson);
	}

	// Связи
	for (const auto& conn : m_connections)
	{
		json connJson;
		connJson["id"] = conn.id;
		connJson["from_node"] = conn.from_node_id;
		connJson["from_port"] = conn.from_port_index;
		connJson["to_node"] = conn.to_node_id;
		connJson["to_port"] = conn.to_port_index;
		j["connections"].push_back(connJson);
	}

	return j.dump(2);
}

bool AgentModel::FromJson(const std::string& jsonStr)
{
	try
	{
		json j = json::parse(jsonStr);

		// Очистить текущие данные (кроме Input/Output)
		ClearAll();

		// Найти max ID для продолжения нумерации
		m_next_id = 1;

		// Создать узлы
		if (j.contains("nodes"))
		{
			for (const auto& nodeJson : j["nodes"])
			{
				std::string type = nodeJson.value("type", "");
				int id = nodeJson.value("id", 0);
				float x = nodeJson.value("x", 0.0f);
				float y = nodeJson.value("y", 0.0f);

				// Input/Output — использовать существующие фиксированные узлы
				if (type == "input" || type == "output")
				{
					Node* existing = GetNodeById(id);
					if (existing && existing->Identity().typeName == type)
					{
						existing->SetPos(x, y);
						// NOTE: SetNodeScreenSpacePos вызывается в цикле рендера, не здесь

						// Восстановить поля
						if (nodeJson.contains("fields"))
						{
							for (const auto& [key, value] : nodeJson["fields"].items())
							{
								existing->Fields().Set(key, value.get<std::string>());
							}
						}
						if (id >= m_next_id) m_next_id = id + 1;
						continue;
					}
				}

				// Создать узел через фабрику
				auto newNode = NodeFactory::CreateNodeByTypeName(type);
				if (!newNode)
				{
					continue; // Неизвестный тип — пропустить
				}

				// Назначить ID из JSON (сохраняем оригинальные ID)
				newNode->Identity().id = id;
				newNode->SetPos(x, y);
				// NOTE: SetNodeScreenSpacePos вызывается в цикле рендера, не здесь

				// Восстановить поля (перезаписать значения из JSON)
				if (nodeJson.contains("fields"))
				{
					for (const auto& [key, value] : nodeJson["fields"].items())
					{
						newNode->Fields().Set(key, value.get<std::string>());
					}
				}

				m_nodes.push_back(std::move(newNode));

				if (id >= m_next_id)
				{
					m_next_id = id + 1;
				}
			}
		}

		// Создать связи
		if (j.contains("connections"))
		{
			for (const auto& connJson : j["connections"])
			{
				Connection conn;
				conn.id = connJson.value("id", -1);
				conn.from_node_id = connJson.value("from_node", 0);
				conn.from_port_index = connJson.value("from_port", 0);
				conn.to_node_id = connJson.value("to_node", 0);
				conn.to_port_index = connJson.value("to_port", 0);

				// Если ID не сохранён или -1 — назначить новый
				if (conn.id < 0)
				{
					conn.id = m_next_id++;
				}
				else if (conn.id >= m_next_id)
				{
					m_next_id = conn.id + 1;
				}

				m_connections.push_back(conn);
			}
		}

		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool AgentModel::SaveToFile(const std::string& path) const
{
	try
	{
		std::ofstream file(path);
		if (!file.is_open())
		{
			return false;
		}
		file << ToJson();
		file.close();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool AgentModel::LoadFromFile(const std::string& path)
{
	try
	{
		std::ifstream file(path);
		if (!file.is_open())
		{
			return false;
		}
		std::string content((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		file.close();
		return FromJson(content);
	}
	catch (...)
	{
		return false;
	}
}

// ============================================================================
// Инициализация по умолчанию
// ============================================================================

void AgentModel::InitDefaults()
{
	// Создать Input
	float posX = 100.0f;
	float posY = 300.0f;

	auto inputNode = NodeFactory::CreateNodeByTypeName("input");
	if (inputNode)
	{
		inputNode->Identity().id = NextId();
		inputNode->SetPos(posX, posY);
		m_nodes.push_back(std::move(inputNode));
	}

	// Создать Output
	posX = 600.0f;
	posY = 300.0f;

	auto outputNode = NodeFactory::CreateNodeByTypeName("output");
	if (outputNode)
	{
		outputNode->Identity().id = NextId();
		outputNode->SetPos(posX, posY);
		m_nodes.push_back(std::move(outputNode));
	}
}

// ============================================================================
// Удаление связей по ID атрибута (пина)
// ============================================================================

void AgentModel::RemoveConnectionsForPin(int attrId)
{
	bool isInput = (attrId % 1000) < 100;
	int nodeId = attrId / 1000;
	int portIndex = isInput ? (attrId % 1000) : (attrId % 1000) - 100;

	m_connections.erase(
		std::remove_if(m_connections.begin(), m_connections.end(),
			[nodeId, portIndex, isInput](const Connection& conn)
			{
				if (isInput)
				{
					return conn.to_node_id == nodeId && conn.to_port_index == portIndex;
				}
				else
				{
					return conn.from_node_id == nodeId && conn.from_port_index == portIndex;
				}
			}),
		m_connections.end()
	);
}

// ============================================================================
// Внутренние методы
// ============================================================================

int AgentModel::NextId()
{
	return m_next_id++;
}
