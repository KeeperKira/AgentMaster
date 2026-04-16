#include "Node.hpp"
#include "AgentModel.hpp"

// ============================================================================
// Node — конструктор
// ============================================================================

Node::Node()
	: m_x(0.0f)
	, m_y(0.0f)
{
}

void Node::SetPos(float x, float y)
{
	m_x = x;
	m_y = y;
}

// ============================================================================
// UIDraw — полная отрисовка узла
// ============================================================================

void Node::UIDraw(AgentModel* model)
{
	// Заголовок: кнопка удаления + имя
	ImNodes::BeginNodeTitleBar();
	DrawTitleBar();
	ImNodes::EndNodeTitleBar();

	// Тело узла
	DrawContent();

	// Входы
	for (int i = 0; i < m_ports.GetInputCount(); i++)
	{
		ImNodes::BeginInputAttribute(InputAttrId(m_identity.id, i), ImNodesPinShape_CircleFilled);
		ImGui::Text("In %d", i);
		ImNodes::EndInputAttribute();
	}

	// Выходы
	for (int i = 0; i < m_ports.GetOutputCount(); i++)
	{
		ImNodes::BeginOutputAttribute(OutputAttrId(m_identity.id, i), ImNodesPinShape_CircleFilled);
		ImGui::Text("Out %d", i);
		ImNodes::EndOutputAttribute();
	}
}

// ============================================================================
// DrawTitleBar — заголовок узла
// ============================================================================

void Node::DrawTitleBar()
{
	ImGui::TextUnformatted(m_identity.GetTypeName());

	// Редактируемое поле имени
	if (m_fields.Has("name"))
	{
		ImGui::SameLine();
		char nameBuf[256];
		strncpy_s(nameBuf, m_fields.Get("name").c_str(), sizeof(nameBuf) - 1);
		nameBuf[sizeof(nameBuf) - 1] = '\0';
		ImGui::PushItemWidth(120.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 0));
		ImGui::PushID(m_identity.id);
		if (ImGui::InputText("##nodename", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_fields.Set("name", nameBuf);
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			m_fields.Set("name", nameBuf);
		}
		ImGui::PopID();
		ImGui::PopStyleVar();
		ImGui::PopItemWidth();
		ImGui::SameLine();
	}

	// Кнопка удаления (кроме фиксированных узлов)
	if (!m_identity.fixed)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.1f, 0.1f, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.15f, 0.15f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		ImGui::PushID(m_identity.id);

		if (ImGui::SmallButton("X"))
		{
			m_fields.Set("__delete_requested", "true");
		}
		ImGui::PopID();
		ImGui::PopStyleColor(4);
		ImGui::SameLine();
	}
}

// ============================================================================
// DrawContent — тело узла (поддержка комбинаций типов контента)
// ============================================================================

void Node::DrawContent()
{
	ImGui::PushID(m_identity.id);

	// TextMultiline (одно поле)
	if (m_uiConfig.enableTextMultiline)
	{
		char textBuf[512];
		strncpy_s(textBuf, m_fields.Get(m_uiConfig.textMultilineField).c_str(), sizeof(textBuf) - 1);
		textBuf[sizeof(textBuf) - 1] = '\0';
		ImGui::PushItemWidth(m_uiConfig.textMultilineSize[0]);
		if (ImGui::InputTextMultiline(("##text_" + m_uiConfig.textMultilineField).c_str(), textBuf, sizeof(textBuf), ImVec2(m_uiConfig.textMultilineSize[0], m_uiConfig.textMultilineSize[1])))
		{
			m_fields.Set(m_uiConfig.textMultilineField, textBuf);
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			m_fields.Set(m_uiConfig.textMultilineField, textBuf);
		}
		ImGui::PopItemWidth();
	}

	// TextInputs (несколько полей)
	if (m_uiConfig.enableTextInputs)
	{
		for (const auto& fieldName : m_uiConfig.textInputFields)
		{
			char buf[256];
			strncpy_s(buf, m_fields.Get(fieldName).c_str(), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			ImGui::PushItemWidth(m_uiConfig.textInputWidth);

			// Capitalize field label
			std::string label = fieldName;
			if (!label.empty()) label[0] = toupper(label[0]);

			if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
			{
				m_fields.Set(fieldName, buf);
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				m_fields.Set(fieldName, buf);
			}
			ImGui::PopItemWidth();
		}
	}

	// Checkbox
	if (m_uiConfig.enableCheckbox)
	{
		bool checked = (m_fields.Get(m_uiConfig.checkboxField) == "true");
		if (ImGui::Checkbox(m_uiConfig.checkboxField.c_str(), &checked))
		{
			m_fields.Set(m_uiConfig.checkboxField, checked ? "true" : "false");
		}
	}

	ImGui::PopID();
}
