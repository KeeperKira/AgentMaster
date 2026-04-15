#include "NodeFactory.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

using json = nlohmann::json;

// Статическая карта шаблонов
std::map<std::string, NodeFactory::NodeTemplate> NodeFactory::s_templates;

// ============================================================================
// Initialize — загрузить все JSON шаблоны из папки Nodes/
// ============================================================================

bool NodeFactory::Initialize(const std::string& nodesDir)
{
	namespace fs = std::filesystem;

	if (!fs::exists(nodesDir))
	{
		return false;
	}

	for (const auto& entry : fs::directory_iterator(nodesDir))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".json")
		{
			LoadTemplate(entry.path().string());
		}
	}

	return !s_templates.empty();
}

// ============================================================================
// Shutdown — очистить шаблоны
// ============================================================================

void NodeFactory::Shutdown()
{
	s_templates.clear();
}

// ============================================================================
// CreateNode — создать узел по типу
// ============================================================================

Node* NodeFactory::CreateNode(NodeType type)
{
	// Найти шаблон по типу
	std::string typeName;
	switch (type)
	{
	case NodeType::Input:    typeName = "input"; break;
	case NodeType::Output:   typeName = "output"; break;
	case NodeType::Text:     typeName = "text"; break;
	case NodeType::Triplet:  typeName = "triplet"; break;
	case NodeType::Router:   typeName = "router"; break;
	case NodeType::Concat:   typeName = "concat"; break;
	case NodeType::Logger:   typeName = "logger"; break;
	case NodeType::Gate:     typeName = "gate"; break;
	default: return nullptr;
	}

	return CreateNodeByTypeName(typeName);
}

// ============================================================================
// CreateNodeByTypeName — создать узел по строковому имени
// ============================================================================

Node* NodeFactory::CreateNodeByTypeName(const std::string& typeName)
{
	auto it = s_templates.find(typeName);
	if (it == s_templates.end())
	{
		return nullptr;
	}

	const auto& tmpl = it->second;

	Node* node = new Node();

	// IdentityComponent
	node->Identity().id = 0; // будет установлен caller'ом
	node->Identity().type = tmpl.type;
	node->Identity().typeName = tmpl.typeName;
	node->Identity().displayName = tmpl.displayName;
	node->Identity().fixed = tmpl.fixed;

	// PortsComponent
	node->Ports().inputCount = tmpl.inputPorts;
	node->Ports().outputCount = tmpl.outputPorts;

	// FieldsComponent (копия default fields)
	for (const auto& [key, value] : tmpl.defaultFields)
	{
		node->Fields().Set(key, value);
	}

	// UIConfig
	node->GetUI() = tmpl.uiConfig;

	return node;
}

// ============================================================================
// GetDisplayName — получить отображаемое имя
// ============================================================================

std::string NodeFactory::GetDisplayName(NodeType type)
{
	return CreateNode(type) ? CreateNode(type)->GetTypeName() : "Unknown";
}

std::string NodeFactory::GetDisplayNameByTypeName(const std::string& typeName)
{
	auto it = s_templates.find(typeName);
	if (it != s_templates.end())
	{
		return it->second.displayName;
	}
	return "Unknown";
}

// ============================================================================
// GetAllTemplateNames — список всех загруженных типов
// ============================================================================

std::vector<std::string> NodeFactory::GetAllTemplateNames()
{
	std::vector<std::string> names;
	for (const auto& [key, tmpl] : s_templates)
	{
		names.push_back(key);
	}
	return names;
}

// ============================================================================
// HasTemplate — проверить наличие шаблона
// ============================================================================

bool NodeFactory::HasTemplate(const std::string& typeName)
{
	return s_templates.find(typeName) != s_templates.end();
}

// ============================================================================
// IsTemplateFixed — фиксированный ли шаблон
// ============================================================================

bool NodeFactory::IsTemplateFixed(const std::string& typeName)
{
	auto it = s_templates.find(typeName);
	if (it != s_templates.end())
	{
		return it->second.fixed;
	}
	return false;
}

// ============================================================================
// GetTemplateNodeType — получить NodeType по имени шаблона
// ============================================================================

NodeType NodeFactory::GetTemplateNodeType(const std::string& typeName)
{
	auto it = s_templates.find(typeName);
	if (it != s_templates.end())
	{
		return it->second.type;
	}
	return NodeType::Input; // default
}

// ============================================================================
// LoadTemplate — загрузить один шаблон из JSON файла
// ============================================================================

bool NodeFactory::LoadTemplate(const std::string& filePath)
{
	try
	{
		std::ifstream file(filePath);
		if (!file.is_open())
		{
			return false;
		}

		json j;
		file >> j;
		file.close();

		NodeTemplate tmpl;

		// Тип
		std::string typeStr = j.value("type", "");
		tmpl.type = ParseNodeType(typeStr);
		tmpl.typeName = typeStr;

		// Отображаемое имя
		tmpl.displayName = j.value("name", typeStr);

		// Фиксированный
		tmpl.fixed = j.value("fixed", false);

		// Порты
		if (j.contains("ports"))
		{
			tmpl.inputPorts = j["ports"].value("input", 0);
			tmpl.outputPorts = j["ports"].value("output", 0);
		}

		// Поля (значения по умолчанию)
		if (j.contains("fields"))
		{
			for (const auto& [key, value] : j["fields"].items())
			{
				tmpl.defaultFields[key] = value.get<std::string>();
			}
		}

		// UI конфигурация
		if (j.contains("ui"))
		{
			tmpl.uiConfig = ParseUIConfig(j["ui"]);
		}

		s_templates[typeStr] = tmpl;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

// ============================================================================
// ParseNodeType — строковое имя → NodeType
// ============================================================================

NodeType NodeFactory::ParseNodeType(const std::string& typeName)
{
	if (typeName == "input")    return NodeType::Input;
	if (typeName == "output")   return NodeType::Output;
	if (typeName == "text")     return NodeType::Text;
	if (typeName == "triplet")  return NodeType::Triplet;
	if (typeName == "router")   return NodeType::Router;
	if (typeName == "concat")   return NodeType::Concat;
	if (typeName == "logger")   return NodeType::Logger;
	if (typeName == "gate")     return NodeType::Gate;
	return NodeType::Input; // default
}

// ============================================================================
// ParseUIConfig — парсинг UI конфигурации из JSON
// ============================================================================

UIConfig NodeFactory::ParseUIConfig(const nlohmann::json& json)
{
	UIConfig config;

	// Multiline text
	if (json.contains("text_multiline") && json["text_multiline"].is_boolean())
	{
		if (json["text_multiline"].get<bool>())
		{
			config.contentType = UIConfig::ContentType::TextMultiline;
			config.textMultilineField = "text"; // default

			// Если есть fields, взять первое не-"name" поле
			if (json.contains("text_field"))
			{
				config.textMultilineField = json["text_field"].get<std::string>();
			}

			if (json.contains("text_size") && json["text_size"].is_array() && json["text_size"].size() == 2)
			{
				config.textMultilineSize[0] = json["text_size"][0].get<float>();
				config.textMultilineSize[1] = json["text_size"][1].get<float>();
			}
		}
	}

	// Text inputs
	if (json.contains("text_inputs") && json["text_inputs"].is_array())
	{
		config.contentType = UIConfig::ContentType::TextInputs;
		config.textInputWidth = json.value("input_width", 180.0f);

		for (const auto& field : json["text_inputs"])
		{
			config.textInputFields.push_back(field.get<std::string>());
		}
	}

	// Checkbox
	if (json.contains("checkbox") && json["checkbox"].is_string())
	{
		config.contentType = UIConfig::ContentType::Checkbox;
		config.checkboxField = json["checkbox"].get<std::string>();
	}

	return config;
}
