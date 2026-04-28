#pragma once

#include "Node.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <memory>
#include <vector>

// ============================================================================
// NodeFactory — фабрика узлов (загрузка из JSON, создание узлов)
// ============================================================================

class NodeFactory
{
public:
	// Инициализация — загрузить все шаблоны из папки Nodes/
	static bool Initialize(const std::string& nodesDir);

	// Очистка
	static void Shutdown();

	// Создать узел по типу
	static std::unique_ptr<Node> CreateNode(NodeType type);

	// Создать узел по строковому имени типа
	static std::unique_ptr<Node> CreateNodeByTypeName(const std::string& typeName);

	// Получить displayName по типу
	static std::string GetDisplayName(NodeType type);
	static std::string GetDisplayNameByTypeName(const std::string& typeName);

	// Получить список всех загруженных типов (для каталога)
	static std::vector<std::string> GetAllTemplateNames();

	// Проверить, существует ли шаблон
	static bool HasTemplate(const std::string& typeName);

	// Получить тип шаблона (fixed, ports и т.д.)
	static bool IsTemplateFixed(const std::string& typeName);
	static NodeType GetTemplateNodeType(const std::string& typeName);

private:
	// Шаблон узла (загружается из JSON)
	struct NodeTemplate
	{
		NodeType type;
		std::string typeName;
		std::string displayName;
		bool fixed;
		int inputPorts;
		int outputPorts;
		std::map<std::string, std::string> defaultFields;
		UIConfig uiConfig;
	};

	// Карта шаблонов
	static std::map<std::string, NodeTemplate> s_templates;

	// Внутренние методы
	static bool LoadTemplate(const std::string& filePath);
	static NodeType ParseNodeType(const std::string& typeName);
	static UIConfig ParseUIConfig(const nlohmann::json& json);
};
