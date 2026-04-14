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

const std::vector<Node*>& AgentModel::GetNodes() const
{
	return m_nodes;
}

Node* AgentModel::GetNodeById(int id) const
{
	for (auto node : m_nodes)
	{
		if (node->GetId() == id)
		{
			return node;
		}
	}
	return nullptr;
}

const std::vector<Connection>& AgentModel::GetConnections() const
{
	return m_connections;
}

std::vector<Connection>& AgentModel::GetConnectionsMutable()
{
	return m_connections;
}

// ============================================================================
// Связи
// ============================================================================

void AgentModel::AddConnection(const Connection& conn)
{
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
// Создание узлов
// ============================================================================

Node* AgentModel::CreateNode(NodeType type)
{
	Node* newNode = nullptr;

	switch (type)
	{
	case NodeType::Text:
		newNode = new TextNode(NextId());
		break;
	case NodeType::Triplet:
		newNode = new TripletNode(NextId());
		break;
	case NodeType::Router:
		newNode = new RouterNode(NextId());
		break;
	case NodeType::Concat:
		newNode = new ConcatNode(NextId());
		break;
	case NodeType::Logger:
		newNode = new LoggerNode(NextId());
		break;
	case NodeType::Gate:
		newNode = new GateNode(NextId());
		break;
	default:
		return nullptr;
	}

	m_nodes.push_back(newNode);
	return newNode;
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

			delete *it;
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
	// Удалить все кроме фиксированных
	for (auto node : m_nodes)
	{
		if (!node->IsFixed())
		{
			delete node;
		}
	}
	m_nodes.erase(
		std::remove_if(m_nodes.begin(), m_nodes.end(),
			[](Node* node) { return !node->IsFixed(); }),
		m_nodes.end()
	);
	m_connections.clear();
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

		// Тип узла — строковое имя для JSON
		switch (node->GetType())
		{
		case NodeType::Input:
			nodeJson["type"] = "input";
			break;
		case NodeType::Output:
			nodeJson["type"] = "output";
			break;
		case NodeType::Text:
			nodeJson["type"] = "text";
			break;
		case NodeType::Triplet:
			nodeJson["type"] = "triplet";
			break;
		case NodeType::Router:
			nodeJson["type"] = "router";
			break;
		case NodeType::Concat:
			nodeJson["type"] = "concat";
			break;
		case NodeType::Logger:
			nodeJson["type"] = "logger";
			break;
		case NodeType::Gate:
			nodeJson["type"] = "gate";
			break;
		}

		nodeJson["x"] = node->GetX();
		nodeJson["y"] = node->GetY();

		// Поля
		json fieldsJson;
		const auto& fields = node->GetFields();
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

				Node* newNode = nullptr;

				if (type == "input")
				{
					// Найти существующий Input или создать новый
					Node* existing = GetNodeById(id);
					if (existing && existing->GetType() == NodeType::Input)
					{
						existing->SetPos(x, y);
						ImNodes::SetNodeScreenSpacePos(existing->GetId(), ImVec2(x, y));
						// Восстановить поля
						if (nodeJson.contains("fields"))
						{
							for (const auto& [key, value] : nodeJson["fields"].items())
							{
								existing->SetField(key, value.get<std::string>());
							}
						}
						if (id >= m_next_id) m_next_id = id + 1;
						continue;
					}
					newNode = new InputNode(id);
				}
				else if (type == "output")
				{
					// Найти существующий Output или создать новый
					Node* existing = GetNodeById(id);
					if (existing && existing->GetType() == NodeType::Output)
					{
						existing->SetPos(x, y);
						ImNodes::SetNodeScreenSpacePos(existing->GetId(), ImVec2(x, y));
						// Восстановить поля
						if (nodeJson.contains("fields"))
						{
							for (const auto& [key, value] : nodeJson["fields"].items())
							{
								existing->SetField(key, value.get<std::string>());
							}
						}
						if (id >= m_next_id) m_next_id = id + 1;
						continue;
					}
					newNode = new OutputNode(id);
				}
				else if (type == "text")
				{
					newNode = new TextNode(id);
				}
				else if (type == "triplet")
				{
					newNode = new TripletNode(id);
				}
				else if (type == "router")
				{
					newNode = new RouterNode(id);
				}
				else if (type == "concat")
				{
					newNode = new ConcatNode(id);
				}
				else if (type == "logger")
				{
					newNode = new LoggerNode(id);
				}
				else if (type == "gate")
				{
					newNode = new GateNode(id);
				}
				else
				{
					continue; // Неизвестный тип — пропустить
				}

				newNode->SetPos(x, y);
				ImNodes::SetNodeScreenSpacePos(newNode->GetId(), ImVec2(x, y));

				// Восстановить поля
				if (nodeJson.contains("fields"))
				{
					for (const auto& [key, value] : nodeJson["fields"].items())
					{
						newNode->SetField(key, value.get<std::string>());
					}
				}

				m_nodes.push_back(newNode);

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
	// Создать Input и Output
	InputNode* inputNode = new InputNode(NextId());
	inputNode->SetPos(100.0f, 300.0f);
	ImNodes::SetNodeScreenSpacePos(inputNode->GetId(), ImVec2(100.0f, 300.0f));
	m_nodes.push_back(inputNode);

	OutputNode* outputNode = new OutputNode(NextId());
	outputNode->SetPos(600.0f, 300.0f);
	ImNodes::SetNodeScreenSpacePos(outputNode->GetId(), ImVec2(600.0f, 300.0f));
	m_nodes.push_back(outputNode);
}

// ============================================================================
// Внутренние методы
// ============================================================================

int AgentModel::NextId()
{
	return m_next_id++;
}
