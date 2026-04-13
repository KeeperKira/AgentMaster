#pragma once

#include <map>
#include <string>

// ============================================================================
// Типы узлов
// ============================================================================

enum class NodeType
{
	Input,
	Output,
	Text,
	Triplet,
	Router
};

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
};
