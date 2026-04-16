#pragma once

#include <map>
#include <string>
#include <vector>

// Forward declaration
class AgentModel;

// ImGui
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

// ImNodes
#include <imnodes.h>

// ============================================================================
// Типы узлов (строковые идентификаторы)
// ============================================================================

enum class NodeType
{
	Input,
	Output,
	Text,
	Triplet,
	Router,
	Concat,
	Logger,
	Gate
};

// Вспомогательные функции для ID атрибутов
static inline int InputAttrId(int nodeId, int portIndex) { return nodeId * 1000 + portIndex; }
static inline int OutputAttrId(int nodeId, int portIndex) { return nodeId * 1000 + portIndex + 100; }

// ============================================================================
// IdentityComponent — идентичность узла
// ============================================================================

struct IdentityComponent
{
	int id = 0;
	NodeType type = NodeType::Input;
	std::string typeName; // строковое имя типа
	std::string displayName; // отображаемое имя (Input, Output, Text...)
	bool fixed = false; // нельзя удалить

	const char* GetTypeName() const { return typeName.c_str(); }
};

// ============================================================================
// FieldsComponent — поля данных узла
// ============================================================================

struct FieldsComponent
{
	std::map<std::string, std::string> fields;

	void Set(const std::string& key, const std::string& value) { fields[key] = value; }
	std::string Get(const std::string& key) const
	{
		auto it = fields.find(key);
		if (it != fields.end()) return it->second;
		return "";
	}
	bool Has(const std::string& key) const { return fields.find(key) != fields.end(); }
};

// ============================================================================
// PortsComponent — порты узла
// ============================================================================

struct PortsComponent
{
	int inputCount = 0;
	int outputCount = 0;

	int GetInputCount() const { return inputCount; }
	int GetOutputCount() const { return outputCount; }
};

// ============================================================================
// UIConfig — конфигурация отрисовки тела узла
// ============================================================================

struct UIConfig
{
	enum class ContentType
	{
		None,
		TextMultiline,
		TextInputs,
		Checkbox
	};

	ContentType contentType = ContentType::None;

	// Для TextMultiline
	bool enableTextMultiline = false;
	std::string textMultilineField;
	float textMultilineSize[2] = { 180.0f, 80.0f };

	// Для TextInputs
	bool enableTextInputs = false;
	std::vector<std::string> textInputFields;
	float textInputWidth = 180.0f;

	// Для Checkbox
	bool enableCheckbox = false;
	std::string checkboxField;
};

// ============================================================================
// Node — единый класс узла (без наследования)
// ============================================================================

class Node
{
public:
	Node();
	~Node() = default;

	// Компоненты
	IdentityComponent& Identity() { return m_identity; }
	const IdentityComponent& Identity() const { return m_identity; }

	FieldsComponent& Fields() { return m_fields; }
	const FieldsComponent& Fields() const { return m_fields; }

	PortsComponent& Ports() { return m_ports; }
	const PortsComponent& Ports() const { return m_ports; }

	UIConfig& GetUI() { return m_uiConfig; }
	const UIConfig& GetUI() const { return m_uiConfig; }

	// Позиция
	float GetX() const { return m_x; }
	float GetY() const { return m_y; }
	void SetPos(float x, float y);

	// Полная отрисовка узла в ImNodes
	void UIDraw(AgentModel* model);

	// Удобные методы
	int GetId() const { return m_identity.id; }
	NodeType GetType() const { return m_identity.type; }
	const char* GetTypeName() const { return m_identity.GetTypeName(); }
	bool IsFixed() const { return m_identity.fixed; }

private:
	// Компоненты
	IdentityComponent m_identity;
	FieldsComponent m_fields;
	PortsComponent m_ports;
	UIConfig m_uiConfig;

	// Позиция на canvas
	float m_x = 0.0f;
	float m_y = 0.0f;

	// Внутренние методы отрисовки
	void DrawTitleBar();
	void DrawContent();
};
