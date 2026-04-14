#pragma once

#include "Node.hpp"
#include <string>
#include <vector>

// ============================================================================
// Connection — связь между портами
// ============================================================================

struct Connection
{
	int id;
	int from_node_id;
	int from_port_index;
	int to_node_id;
	int to_port_index;

	bool operator==(const Connection& other) const
	{
		return from_node_id == other.from_node_id
			&& from_port_index == other.from_port_index
			&& to_node_id == other.to_node_id
			&& to_port_index == other.to_port_index;
	}
};

// ============================================================================
// AgentModel — управление узлами и связями
// ============================================================================

class AgentModel
{
public:
	AgentModel();
	~AgentModel();

	// Узлы
	const std::vector<Node*>& GetNodes() const;
	Node* GetNodeById(int id) const;

	// Связи
	const std::vector<Connection>& GetConnections() const;
	std::vector<Connection>& GetConnectionsMutable();
	void AddConnection(const Connection& conn);
	void RemoveConnection(int from_node_id, int to_node_id);
	void RemoveConnectionById(int conn_id);

	// Создание узлов
	Node* CreateNode(NodeType type);

	// Удаление узлов (нельзя удалить фиксированные Input/Output)
	bool DeleteNode(int id);

	// Очистка (удаляет все кроме фиксированных)
	void ClearAll();

	// Сериализация
	std::string ToJson() const;
	bool FromJson(const std::string& json);
	bool SaveToFile(const std::string& path) const;
	bool LoadFromFile(const std::string& path);

	// Начальная инициализация (создаёт Input + Output)
	void InitDefaults();

private:
	int NextId();

	std::vector<Node*> m_nodes;
	std::vector<Connection> m_connections;
	int m_next_id = 1;
};
