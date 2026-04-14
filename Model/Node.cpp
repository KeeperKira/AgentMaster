#include "Node.hpp"
#include "AgentModel.hpp"

// ============================================================================
// Node — реализация
// ============================================================================

Node::Node(int id, bool fixed)
	: m_id(id)
	, m_x(0.0f)
	, m_y(0.0f)
	, m_fixed(fixed) {}

int Node::GetId() const
{
	return m_id;
}

float Node::GetX() const
{
	return m_x;
}

float Node::GetY() const
{
	return m_y;
}

void Node::SetPos(float x, float y)
{
	m_x = x;
	m_y = y;
}

const std::map<std::string, std::string>& Node::GetFields() const
{
	return m_fields;
}

void Node::SetField(const std::string& key, const std::string& value)
{
	m_fields[key] = value;
}

std::string Node::GetField(const std::string& key) const
{
	auto it = m_fields.find(key);
	if (it != m_fields.end())
	{
		return it->second;
	}
	return "";
}

bool Node::HasField(const std::string& key) const
{
	return m_fields.find(key) != m_fields.end();
}

bool Node::IsFixed() const
{
	return m_fixed;
}

// Базовая отрисовка: title bar -> content -> порты
void Node::UIDraw(AgentModel* model)
{
	// Заголовок: кнопка удаления + имя
	ImNodes::BeginNodeTitleBar();
	DrawTitleBar();
	ImNodes::EndNodeTitleBar();

	// Тело узла
	DrawContent();

	// Входы
	for (int i = 0; i < GetInputCount(); i++)
	{
		ImNodes::BeginInputAttribute(InputAttrId(m_id, i), ImNodesPinShape_CircleFilled);
		ImGui::Text("In %d", i);
		ImNodes::EndInputAttribute();
	}

	// Выходы
	for (int i = 0; i < GetOutputCount(); i++)
	{
		ImNodes::BeginOutputAttribute(OutputAttrId(m_id, i), ImNodesPinShape_CircleFilled);
		ImGui::Text("Out %d", i);
		ImNodes::EndOutputAttribute();
	}
}

// Отрисовка title bar (кнопка X + type name + editable name)
void Node::DrawTitleBar()
{


	ImGui::TextUnformatted(GetTypeName());

	// Редактируемое поле имени
	if (HasField("name"))
	{
		ImGui::SameLine();
		char nameBuf[256];
		strncpy_s(nameBuf, GetField("name").c_str(), sizeof(nameBuf) - 1);
		nameBuf[sizeof(nameBuf) - 1] = '\0';
		ImGui::PushItemWidth(120.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 0));
		ImGui::PushID(m_id);
		if (ImGui::InputText("##nodename", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			SetField("name", nameBuf);
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			SetField("name", nameBuf);
		}
		ImGui::PopID();
		ImGui::PopStyleVar();
		ImGui::PopItemWidth();
		ImGui::SameLine();
	}

	// Кнопка удаления (кроме фиксированных узлов)
	if (!m_fixed)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.1f, 0.1f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.15f, 0.15f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		ImGui::PushID(m_id);

		if (ImGui::SmallButton("X"))
		{
			// Узел будет удалён caller'ом (после EndNode)
			// Помечаем через поле — AgentMaster.cpp проверит
			SetField("__delete_requested", "true");
		}
		ImGui::PopID();
		ImGui::PopStyleColor(4);
		ImGui::SameLine();
	}
}

// Пустая реализация по умолчанию
void Node::DrawContent() {}

// ============================================================================
// InputNode
// ============================================================================

InputNode::InputNode(int id)
	: Node(id, true)
{
	std::string name = "Input #" + std::to_string(id);
	SetField("name", name);
}

int InputNode::GetInputCount() const { return 0; }
int InputNode::GetOutputCount() const { return 1; }
NodeType InputNode::GetType() const { return NodeType::Input; }
const char* InputNode::GetTypeName() const { return "Input"; }
void InputNode::DrawContent() {}

// ============================================================================
// OutputNode
// ============================================================================

OutputNode::OutputNode(int id)
	: Node(id, true)
{
	std::string name = "Output #" + std::to_string(id);
	SetField("name", name);
}

int OutputNode::GetInputCount() const { return 1; }
int OutputNode::GetOutputCount() const { return 1; }
NodeType OutputNode::GetType() const { return NodeType::Output; }
const char* OutputNode::GetTypeName() const { return "Output"; }
void OutputNode::DrawContent() {}

// ============================================================================
// TextNode
// ============================================================================

TextNode::TextNode(int id)
	: Node(id, false)
{
	std::string name = "Text #" + std::to_string(id);
	SetField("name", name);
	SetField("text", "");
}

int TextNode::GetInputCount() const { return 0; }
int TextNode::GetOutputCount() const { return 1; }
NodeType TextNode::GetType() const { return NodeType::Text; }
const char* TextNode::GetTypeName() const { return "Text"; }

void TextNode::DrawContent()
{
	ImGui::PushID(m_id);
	char textBuf[512];
	strncpy_s(textBuf, GetField("text").c_str(), sizeof(textBuf) - 1);
	textBuf[sizeof(textBuf) - 1] = '\0';
	ImGui::PushItemWidth(180.0f);
	if (ImGui::InputTextMultiline("##text", textBuf, sizeof(textBuf), ImVec2(180, 80)))
	{
		SetField("text", textBuf);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SetField("text", textBuf);
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
}

// ============================================================================
// TripletNode
// ============================================================================

TripletNode::TripletNode(int id)
	: Node(id, false)
{
	std::string name = "Triplet #" + std::to_string(id);
	SetField("name", name);
}

int TripletNode::GetInputCount() const { return 3; }
int TripletNode::GetOutputCount() const { return 1; }
NodeType TripletNode::GetType() const { return NodeType::Triplet; }
const char* TripletNode::GetTypeName() const { return "Triplet"; }
void TripletNode::DrawContent() {}

// ============================================================================
// RouterNode
// ============================================================================

RouterNode::RouterNode(int id)
	: Node(id, false)
{
	std::string name = "Router #" + std::to_string(id);
	SetField("name", name);
	SetField("model", "");
	SetField("url", "");
	SetField("api_key", "");
}

int RouterNode::GetInputCount() const { return 1; }
int RouterNode::GetOutputCount() const { return 1; }
NodeType RouterNode::GetType() const { return NodeType::Router; }
const char* RouterNode::GetTypeName() const { return "Router"; }

void RouterNode::DrawContent()
{
	ImGui::PushID(m_id);

	char modelBuf[256];
	strncpy_s(modelBuf, GetField("model").c_str(), sizeof(modelBuf) - 1);
	modelBuf[sizeof(modelBuf) - 1] = '\0';
	ImGui::PushItemWidth(180.0f);
	if (ImGui::InputText("Model", modelBuf, sizeof(modelBuf)))
	{
		SetField("model", modelBuf);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SetField("model", modelBuf);
	}

	char urlBuf[256];
	strncpy_s(urlBuf, GetField("url").c_str(), sizeof(urlBuf) - 1);
	urlBuf[sizeof(urlBuf) - 1] = '\0';
	if (ImGui::InputText("URL", urlBuf, sizeof(urlBuf)))
	{
		SetField("url", urlBuf);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SetField("url", urlBuf);
	}

	char keyBuf[256];
	strncpy_s(keyBuf, GetField("api_key").c_str(), sizeof(keyBuf) - 1);
	keyBuf[sizeof(keyBuf) - 1] = '\0';
	if (ImGui::InputText("API Key", keyBuf, sizeof(keyBuf)))
	{
		SetField("api_key", keyBuf);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SetField("api_key", keyBuf);
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
}

// ============================================================================
// ConcatNode
// ============================================================================

ConcatNode::ConcatNode(int id)
	: Node(id, false)
{
	std::string name = "Concat #" + std::to_string(id);
	SetField("name", name);
	SetField("wait", "false");
}

int ConcatNode::GetInputCount() const { return 2; }
int ConcatNode::GetOutputCount() const { return 1; }
NodeType ConcatNode::GetType() const { return NodeType::Concat; }
const char* ConcatNode::GetTypeName() const { return "Concat"; }

void ConcatNode::DrawContent()
{
	ImGui::PushID(m_id);
	bool wait = (GetField("wait") == "true");
	if (ImGui::Checkbox("Wait", &wait))
	{
		SetField("wait", wait ? "true" : "false");
	}
	ImGui::PopID();
}

// ============================================================================
// LoggerNode
// ============================================================================

LoggerNode::LoggerNode(int id)
	: Node(id, false)
{
	std::string name = "Logger #" + std::to_string(id);
	SetField("name", name);
	SetField("filename", "agent.log");
}

int LoggerNode::GetInputCount() const { return 1; }
int LoggerNode::GetOutputCount() const { return 1; }
NodeType LoggerNode::GetType() const { return NodeType::Logger; }
const char* LoggerNode::GetTypeName() const { return "Logger"; }

void LoggerNode::DrawContent()
{
	ImGui::PushID(m_id);
	char fileBuf[256];
	strncpy_s(fileBuf, GetField("filename").c_str(), sizeof(fileBuf) - 1);
	fileBuf[sizeof(fileBuf) - 1] = '\0';
	ImGui::PushItemWidth(180.0f);
	if (ImGui::InputText("File name", fileBuf, sizeof(fileBuf)))
	{
		SetField("filename", fileBuf);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SetField("filename", fileBuf);
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
}

// ============================================================================
// GateNode
// ============================================================================

GateNode::GateNode(int id)
	: Node(id, false)
{
	std::string name = "Gate #" + std::to_string(id);
	SetField("name", name);
	SetField("condition", "");
}

int GateNode::GetInputCount() const { return 2; }
int GateNode::GetOutputCount() const { return 1; }
NodeType GateNode::GetType() const { return NodeType::Gate; }
const char* GateNode::GetTypeName() const { return "Gate"; }

void GateNode::DrawContent()
{
	ImGui::PushID(m_id);
	char condBuf[256];
	strncpy_s(condBuf, GetField("condition").c_str(), sizeof(condBuf) - 1);
	condBuf[sizeof(condBuf) - 1] = '\0';
	ImGui::PushItemWidth(180.0f);
	if (ImGui::InputText("Condition", condBuf, sizeof(condBuf)))
	{
		SetField("condition", condBuf);
	}
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		SetField("condition", condBuf);
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
}
