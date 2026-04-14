#include "Node.hpp"

// ============================================================================
// Node — реализация
// ============================================================================

Node::Node(int id, bool fixed)
	: m_id(id)
	, m_x(0.0f)
	, m_y(0.0f)
	, m_fixed(fixed)
{
}

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

// ============================================================================
// InputNode
// ============================================================================

InputNode::InputNode(int id)
	: Node(id, true)
{
}

int InputNode::GetInputCount() const
{
	return 0;
}

int InputNode::GetOutputCount() const
{
	return 1;
}

NodeType InputNode::GetType() const
{
	return NodeType::Input;
}

const char* InputNode::GetTypeName() const
{
	return "Input";
}

// ============================================================================
// OutputNode
// ============================================================================

OutputNode::OutputNode(int id)
	: Node(id, true)
{
}

int OutputNode::GetInputCount() const
{
	return 1;
}

int OutputNode::GetOutputCount() const
{
	return 1;
}

NodeType OutputNode::GetType() const
{
	return NodeType::Output;
}

const char* OutputNode::GetTypeName() const
{
	return "Output";
}

// ============================================================================
// TextNode
// ============================================================================

TextNode::TextNode(int id)
	: Node(id, false)
{
	SetField("name", "");
	SetField("text", "");
}

int TextNode::GetInputCount() const
{
	return 0;
}

int TextNode::GetOutputCount() const
{
	return 1;
}

NodeType TextNode::GetType() const
{
	return NodeType::Text;
}

const char* TextNode::GetTypeName() const
{
	return "Text";
}

// ============================================================================
// TripletNode
// ============================================================================

TripletNode::TripletNode(int id)
	: Node(id, false)
{
	SetField("name", "");
}

int TripletNode::GetInputCount() const
{
	return 3;
}

int TripletNode::GetOutputCount() const
{
	return 1;
}

NodeType TripletNode::GetType() const
{
	return NodeType::Triplet;
}

const char* TripletNode::GetTypeName() const
{
	return "Triplet";
}

// ============================================================================
// RouterNode
// ============================================================================

RouterNode::RouterNode(int id)
	: Node(id, false)
{
	SetField("name", "");
	SetField("url", "");
	SetField("api_key", "");
}

int RouterNode::GetInputCount() const
{
	return 1;
}

int RouterNode::GetOutputCount() const
{
	return 1;
}

NodeType RouterNode::GetType() const
{
	return NodeType::Router;
}

const char* RouterNode::GetTypeName() const
{
	return "Router";
}

// ============================================================================
// ConcatNode
// ============================================================================

ConcatNode::ConcatNode(int id)
	: Node(id, false)
{
	SetField("name", "");
	SetField("wait", "false");
}

int ConcatNode::GetInputCount() const
{
	return 2;
}

int ConcatNode::GetOutputCount() const
{
	return 1;
}

NodeType ConcatNode::GetType() const
{
	return NodeType::Concat;
}

const char* ConcatNode::GetTypeName() const
{
	return "Concat";
}
