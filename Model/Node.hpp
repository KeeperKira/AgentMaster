#pragma once

#include <map>
#include <string>

// Forward declaration (цикл с AgentModel.hpp)
class AgentModel;

// ImGui
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

// ImNodes
#include <imnodes.h>


// ============================================================================
// Типы узлов
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

// Вспомогательные
static inline int InputAttrId(int nodeId, int portIndex) { return nodeId * 1000 + portIndex; }
static inline int OutputAttrId(int nodeId, int portIndex) { return nodeId * 1000 + portIndex + 100; }

// ============================================================================
// Базовый класс Node
// ============================================================================

class Node
{
public:
	virtual ~Node() = default;

	// Идентификатор
	int GetId() const;
	// Позиция на canvas
	float GetX() const;
	float GetY() const;
	void SetPos(float x, float y);

	// Полная отрисовка узла в ImNodes (заголовок, поля, порты)
	virtual void UIDraw(AgentModel* model);

	// Порты (определяются производными классами)
	virtual int GetInputCount() const = 0;
	virtual int GetOutputCount() const = 0;

	// Тип
	virtual NodeType GetType() const = 0;
	virtual const char* GetTypeName() const = 0;

	// Поля
	const std::map<std::string, std::string>& GetFields() const;
	void SetField(const std::string& key, const std::string& value);
	std::string GetField(const std::string& key) const;
	bool HasField(const std::string& key) const;
	// Фиксированные узлы (Input/Output) — нельзя удалить
	bool IsFixed() const;

protected:
	Node(int id, bool fixed);

	// Отрисовка тела узла (переопределяется в потомках)
	virtual void DrawContent();
	// Отрисовка кнопки удаления + заголовка + имени
	void DrawTitleBar();

	int m_id;
	float m_x;
	float m_y;
	bool m_fixed;
	std::map<std::string, std::string> m_fields;
};

// ============================================================================
// InputNode — точка входа запроса
// ============================================================================

class InputNode : public Node
{
public:
	InputNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};

// ============================================================================
// OutputNode — точка выхода ответа
// ============================================================================

class OutputNode : public Node
{
public:
	OutputNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};

// ============================================================================
// TextNode — текстовый блок
// ============================================================================

class TextNode : public Node
{
public:
	TextNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};

// ============================================================================
// TripletNode — блок контекста (3 входа, 1 выход)
// ============================================================================

class TripletNode : public Node
{
public:
	TripletNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};

// ============================================================================
// RouterNode — роутер к внешнему API
// ============================================================================

class RouterNode : public Node
{
public:
	RouterNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};

// ============================================================================
// ConcatNode — объединение двух входов в один выход
// ============================================================================

class ConcatNode : public Node
{
public:
	ConcatNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};

// ============================================================================
// LoggerNode — логирование проходящих данных
// ============================================================================

class LoggerNode : public Node
{
public:
	LoggerNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};

// ============================================================================
// GateNode — шлюз с условием (2 входа: Data / Condition, 1 выход)
// ============================================================================

class GateNode : public Node
{
public:
	GateNode(int id);

	int GetInputCount() const override;
	int GetOutputCount() const override;
	NodeType GetType() const override;
	const char* GetTypeName() const override;

protected:
	void DrawContent() override;
};
