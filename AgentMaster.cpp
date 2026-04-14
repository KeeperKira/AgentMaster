// AgentMaster.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "AgentMaster.h"

// Model
#include "AgentModel.hpp"

// ImGui
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

// Объявление функции обработки сообщений (в ImGui 1.92+ она закомментирована в заголовке)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ImNodes
#include <imnodes.h>

// DirectX 11 (необходим для ImGui backend)
#include <d3d11.h>
#include <dxgi.h>
#include <commdlg.h>
#include <stdio.h>
#include <set>
#include <algorithm>

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна
HWND    g_hWnd = nullptr;                       // дескриптор главного окна

// Модель
static AgentModel g_model;

// Отслеживание узлов, чья позиция уже установлена в ImNodes
static std::set<int> g_nodesPositionSet;

// Режим размещения нового узла (клик по каталогу → ожидание клика на canvas)
static bool g_placingNode = false;
static NodeType g_pendingNodeType = NodeType::Input;
static int g_placingNodeFrame = 0;
static int g_currentFrame = 0;

// Вспомогательная: получить ImNodes ID атрибута для порта
static inline int InputAttrId(int nodeId, int portIndex) { return nodeId * 1000 + portIndex; }
static inline int OutputAttrId(int nodeId, int portIndex) { return nodeId * 1000 + portIndex + 100; }

// Вспомогательные: обёртки над Win32 диалогами
static BOOL GetSaveFileDialogA(OPENFILENAMEA* ofn)
{
	return GetSaveFileNameA(ofn);
}

static BOOL GetOpenFileDialogA(OPENFILENAMEA* ofn)
{
	return GetOpenFileNameA(ofn);
}

// DirectX 11
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

bool                CreateDeviceD3D(HWND hWnd);
void                CleanupDeviceD3D();
void                CreateRenderTarget();
void                CleanupRenderTarget();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Инициализация глобальных строк
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_AGENTMASTER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Выполнить инициализацию приложения:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	// Создать DirectX 11 устройство
	if (!CreateDeviceD3D(g_hWnd))
	{
		CleanupDeviceD3D();
		return FALSE;
	}

	// Инициализировать ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImNodes::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	// Стилизация ImGui
	ImGui::StyleColorsDark();
	ImNodes::StyleColorsDark();

	// Инициализировать платформы ImGui
	ImGui_ImplWin32_Init(g_hWnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	// Инициализировать модель (создаёт Input + Output)
	g_model.InitDefaults();

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AGENTMASTER));

	MSG msg = {};
	ZeroMemory(&msg, sizeof(msg));

	// Цикл основного сообщения
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			// Новый кадр ImGui
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			g_currentFrame++;

			// ESC — отмена размещения
			if (g_placingNode && ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				g_placingNode = false;
			}

			// Главное окно на весь экран
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);

			ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
				| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
				| ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Main", nullptr, flags);
			ImGui::PopStyleVar(3);

			// Меню
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Save JSON..."))
					{
						OPENFILENAMEA ofn = {};
						char szFile[MAX_PATH] = "agent.json";
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner = g_hWnd;
						ofn.lpstrFile = szFile;
						ofn.nMaxFile = MAX_PATH;
						ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
						ofn.nFilterIndex = 1;
						ofn.lpstrFileTitle = nullptr;
						ofn.nMaxFileTitle = 0;
						ofn.lpstrInitialDir = nullptr;
						ofn.Flags = OFN_OVERWRITEPROMPT;
						if (GetSaveFileDialogA(&ofn))
						{
							if (g_model.SaveToFile(szFile))
							{
								MessageBoxA(g_hWnd, "Saved successfully.", "Info", MB_OK);
							}
							else
							{
								MessageBoxA(g_hWnd, "Failed to save file.", "Error", MB_ICONERROR);
							}
						}
					}
					if (ImGui::MenuItem("Load JSON..."))
					{
						OPENFILENAMEA ofn = {};
						char szFile[MAX_PATH] = "";
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner = g_hWnd;
						ofn.lpstrFile = szFile;
						ofn.nMaxFile = MAX_PATH;
						ofn.lpstrFilter = "JSON Files\0*.json\0All Files\0*.*\0";
						ofn.nFilterIndex = 1;
						ofn.lpstrFileTitle = nullptr;
						ofn.nMaxFileTitle = 0;
						ofn.lpstrInitialDir = nullptr;
						ofn.Flags = OFN_FILEMUSTEXIST;
						if (GetOpenFileDialogA(&ofn))
						{
							if (g_model.LoadFromFile(szFile))
							{
								g_nodesPositionSet.clear();
								MessageBoxA(g_hWnd, "Loaded successfully.", "Info", MB_OK);
							}
							else
							{
								MessageBoxA(g_hWnd, "Failed to load file.", "Error", MB_ICONERROR);
							}
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Exit")) { DestroyWindow(g_hWnd); }
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Help"))
				{
					if (ImGui::MenuItem("About")) { /* TODO */ }
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			// Layout: две панели
			static float catalogWidth = 200.0f;
			float availableWidth = ImGui::GetContentRegionAvail().x;
			float canvasWidth = availableWidth - catalogWidth;

			// Левая панель — каталог
			ImGui::BeginChild("Catalog", ImVec2(catalogWidth, 0.0f), true);

			if (g_placingNode)
			{
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Click on canvas to place");
				if (ImGui::Button("Cancel"))
				{
					g_placingNode = false;
				}
				ImGui::Separator();
			}
			else
			{
				ImGui::Text("Tools");
			}
			ImGui::Separator();

			// Элементы каталога (Input/Output всегда на canvas — не показываем)
			const char* toolNames[] = { "Text", "Triplet", "Router", "Concat" };
			NodeType toolTypes[] = { NodeType::Text, NodeType::Triplet, NodeType::Router, NodeType::Concat };

			for (int i = 0; i < 4; i++)
			{
				if (ImGui::Selectable(toolNames[i], false))
				{
					// Клик — войти в режим размещения (начиная со следующего кадра)
					g_placingNode = true;
					g_pendingNodeType = toolTypes[i];
					g_placingNodeFrame = g_currentFrame + 1;
				}
			}

			ImGui::EndChild();

			// Разделитель
			ImGui::SameLine();

			// Правая панель — canvas
			ImGui::BeginChild("Canvas", ImVec2(canvasWidth, 0.0f), true);

			// Если в режиме размещения — проверить клик по canvas
			// (только со следующего кадра после активации — защита от мгновенного срабатывания)
			if (g_placingNode && g_currentFrame > g_placingNodeFrame)
			{
				// Проверяем, не навели ли на узел
				int hoveredNodeId = -1;
				ImNodes::IsNodeHovered(&hoveredNodeId);

				// Правый клик или ESC — отмена
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					g_placingNode = false;
				}

				// Клик по пустому месту на canvas — создать узел
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoveredNodeId == -1)
				{
					ImVec2 mousePos = ImGui::GetMousePos();
					ImVec2 canvasStart = ImGui::GetCursorScreenPos();
					float nodeX = mousePos.x - canvasStart.x;
					float nodeY = mousePos.y - canvasStart.y;

					Node* newNode = g_model.CreateNode(g_pendingNodeType);

					if (newNode != nullptr)
					{
						newNode->SetPos(nodeX, nodeY);
						g_placingNode = false;
					}
				}

				// Визуальная подсказка — текст рядом с курсором
				ImGui::SetNextWindowBgAlpha(0.8f);
				ImGui::BeginTooltip();
				const char* typeName = g_pendingNodeType == NodeType::Text ? "Text" :
					g_pendingNodeType == NodeType::Triplet ? "Triplet" :
					g_pendingNodeType == NodeType::Router ? "Router" : "Concat";
				ImGui::Text("Place %s node", typeName);
				ImGui::EndTooltip();
			}

			ImNodes::BeginNodeEditor();

			// Отрисовка всех узлов
			const auto& nodes = g_model.GetNodes();
			for (Node* node : nodes)
			{
				// При первом рендере — установить начальную позицию
				if (g_nodesPositionSet.find(node->GetId()) == g_nodesPositionSet.end())
				{
					ImNodes::SetNodeGridSpacePos(node->GetId(), ImVec2(node->GetX(), node->GetY()));
					g_nodesPositionSet.insert(node->GetId());
				}

				ImNodes::BeginNode(node->GetId());

				// Заголовок узла: тип + имя
				ImNodes::BeginNodeTitleBar();

				// Кнопка удаления (кроме фиксированных узлов)
				if (!node->IsFixed())
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.1f, 0.1f, 0.5f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.15f, 0.15f, 0.8f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
					ImGui::PushID(node->GetId());
					if (ImGui::SmallButton("X"))
					{
						g_model.DeleteNode(node->GetId());
						ImNodes::EndNode();
						continue;
					}
					ImGui::PopID();
					ImGui::PopStyleColor(4);
					ImGui::SameLine();
				}

				ImGui::TextUnformatted(node->GetTypeName());

				// Редактируемое поле имени (под заголовком, в строке title)
				if (node->HasField("name"))
				{
					ImGui::SameLine();
					char nameBuf[256];
					strncpy_s(nameBuf, node->GetField("name").c_str(), sizeof(nameBuf) - 1);
					nameBuf[sizeof(nameBuf) - 1] = '\0';
					ImGui::PushItemWidth(120.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 0));
					ImGui::PushID(node->GetId());
					if (ImGui::InputText("##nodename", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
					{
						node->SetField("name", nameBuf);
					}
					// Сохранить при потере фокуса
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						node->SetField("name", nameBuf);
					}
					ImGui::PopID();
					ImGui::PopStyleVar();
					ImGui::PopItemWidth();
				}

				ImNodes::EndNodeTitleBar();

				// Тело узла — поля
				// TextNode: поле text
				if (node->GetType() == NodeType::Text)
				{
					ImGui::PushID(node->GetId());
					char textBuf[512];
					strncpy_s(textBuf, node->GetField("text").c_str(), sizeof(textBuf) - 1);
					textBuf[sizeof(textBuf) - 1] = '\0';
					ImGui::PushItemWidth(180.0f);
					if (ImGui::InputTextMultiline("##text", textBuf, sizeof(textBuf), ImVec2(180, 80)))
					{
						node->SetField("text", textBuf);
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						node->SetField("text", textBuf);
					}
					ImGui::PopItemWidth();
					ImGui::PopID();
				}

				// RouterNode: поля url и api_key
				if (node->GetType() == NodeType::Router)
				{
					ImGui::PushID(node->GetId());
					char urlBuf[256];
					strncpy_s(urlBuf, node->GetField("url").c_str(), sizeof(urlBuf) - 1);
					urlBuf[sizeof(urlBuf) - 1] = '\0';
					ImGui::PushItemWidth(180.0f);
					if (ImGui::InputText("URL", urlBuf, sizeof(urlBuf)))
					{
						node->SetField("url", urlBuf);
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						node->SetField("url", urlBuf);
					}

					char keyBuf[256];
					strncpy_s(keyBuf, node->GetField("api_key").c_str(), sizeof(keyBuf) - 1);
					keyBuf[sizeof(keyBuf) - 1] = '\0';
					if (ImGui::InputText("API Key", keyBuf, sizeof(keyBuf)))
					{
						node->SetField("api_key", keyBuf);
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						node->SetField("api_key", keyBuf);
					}
					ImGui::PopItemWidth();
					ImGui::PopID();
				}

				// ConcatNode: чекбокс wait
				if (node->GetType() == NodeType::Concat)
				{
					ImGui::PushID(node->GetId());

					bool wait = (node->GetField("wait") == "true");
					if (ImGui::Checkbox("Wait", &wait))
					{
						node->SetField("wait", wait ? "true" : "false");
					}

					ImGui::PopID();
				}

				// Входы
				for (int i = 0; i < node->GetInputCount(); i++)
				{
					ImNodes::BeginInputAttribute(InputAttrId(node->GetId(), i), ImNodesPinShape_CircleFilled);
					ImGui::Text("In %d", i);
					ImNodes::EndInputAttribute();
				}

				// Выходы
				for (int i = 0; i < node->GetOutputCount(); i++)
				{
					ImNodes::BeginOutputAttribute(OutputAttrId(node->GetId(), i), ImNodesPinShape_CircleFilled);
					ImGui::Text("Out %d", i);
					ImNodes::EndOutputAttribute();
				}

				ImNodes::EndNode();

				// Синхронизировать позицию из ImNodes в модель
				ImVec2 gridPos = ImNodes::GetNodeGridSpacePos(node->GetId());
				node->SetPos(gridPos.x, gridPos.y);
			}

			// Отрисовка связей (каждый кадр)
			{
				const auto& connections = g_model.GetConnections();
				for (int i = 0; i < (int)connections.size(); i++)
				{
					const auto& conn = connections[i];
					int startAttr = OutputAttrId(conn.from_node_id, conn.from_port_index);
					int endAttr = InputAttrId(conn.to_node_id, conn.to_port_index);
					ImNodes::Link(i, startAttr, endAttr);
				}
			}

			ImNodes::EndNodeEditor();

			// Удаление связей по ПКМ на порту (pin)
			{
				int hoveredAttrId = -1;
				if (ImNodes::IsPinHovered(&hoveredAttrId) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					// Удалить все связи где участвует этот атрибут
					auto& conns = g_model.GetConnectionsMutable();
					conns.erase(
						std::remove_if(conns.begin(), conns.end(),
							[hoveredAttrId](const Connection& conn)
							{
								int startAttr = OutputAttrId(conn.from_node_id, conn.from_port_index);
								int endAttr = InputAttrId(conn.to_node_id, conn.to_port_index);
								return startAttr == hoveredAttrId || endAttr == hoveredAttrId;
							}),
						conns.end()
					);
				}
			}

			// Удаление связей по ПКМ на самой линии (link)
			{
				int hoveredLinkId = -1;
				if (ImNodes::IsLinkHovered(&hoveredLinkId) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					g_model.RemoveConnectionByIndex(hoveredLinkId);
				}
			}

			// Проверить новые связи — ПОСЛЕ EndNodeEditor
			{
				int startAttr = -1, endAttr = -1;
				if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
				{
					// Определяем направление: Output -> Input
					// Выход: остаток от /1000 >= 100, Вход: < 100
					auto isOutputAttr = [](int attrId) { return (attrId % 1000) >= 100; };
					auto getInputPort = [](int attrId) { return attrId % 1000; };
					auto getOutputPort = [](int attrId) { return (attrId % 1000) - 100; };
					auto getNodeId = [](int attrId) { return attrId / 1000; };

					bool startIsOutput = isOutputAttr(startAttr);
					bool endIsOutput = isOutputAttr(endAttr);

					if (startIsOutput && !endIsOutput)
					{
						Connection conn;
						conn.from_node_id = getNodeId(startAttr);
						conn.from_port_index = getOutputPort(startAttr);
						conn.to_node_id = getNodeId(endAttr);
						conn.to_port_index = getInputPort(endAttr);
						g_model.AddConnection(conn);
					}
					else if (!startIsOutput && endIsOutput)
					{
						Connection conn;
						conn.from_node_id = getNodeId(endAttr);
						conn.from_port_index = getOutputPort(endAttr);
						conn.to_node_id = getNodeId(startAttr);
						conn.to_port_index = getInputPort(startAttr);
						g_model.AddConnection(conn);
					}
				}
			}

			ImGui::EndChild();

			ImGui::End(); // Main

			// Рендер ImGui
			ImGui::Render();
			g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			// Представить кадр
			HRESULT hr = g_pSwapChain->Present(1, 0);
			if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
			{
				// Устройство потеряно — пересоздать
				CleanupDeviceD3D();
				CreateDeviceD3D(g_hWnd);
			}
		}
	}

	// Очистка
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImNodes::DestroyContext();
	ImGui::DestroyContext();

	CleanupDeviceD3D();

	return (int)msg.wParam;
}

//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AGENTMASTER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = nullptr; // Фон не нужен — рендерим через DirectX
	wcex.lpszMenuName = nullptr;  // Меню управляется через ImGui
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;

	RECT rc = { 0, 0, 1280, 800 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	g_hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
		nullptr, nullptr, hInstance, nullptr);

	if (!g_hWnd)
	{
		return FALSE;
	}

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	return TRUE;
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Передать сообщения в ImGui Win32 backend
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			switch (wmId)
			{
			case IDM_ABOUT:
				DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
				break;
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;
	case WM_SIZE:
		if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Создание DirectX 11 устройства
bool CreateDeviceD3D(HWND hWnd)
{
	// Настройка структуры swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[1] = { D3D_FEATURE_LEVEL_11_0 };

	// Попытка 1: аппаратный рендерер
	HRESULT res = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		featureLevelArray,
		1,
		D3D11_SDK_VERSION,
		&sd,
		&g_pSwapChain,
		&g_pd3dDevice,
		&featureLevel,
		&g_pd3dDeviceContext
	);

	if (FAILED(res))
	{
		// Попытка 2: WARP (программный рендерер)
		res = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0,
			featureLevelArray,
			1,
			D3D11_SDK_VERSION,
			&sd,
			&g_pSwapChain,
			&g_pd3dDevice,
			&featureLevel,
			&g_pd3dDeviceContext
		);

		if (FAILED(res))
		{
			// Попытка 3: Reference (очень медленный, но должен работать)
			res = D3D11CreateDeviceAndSwapChain(
				nullptr,
				D3D_DRIVER_TYPE_REFERENCE,
				nullptr,
				0,
				featureLevelArray,
				1,
				D3D11_SDK_VERSION,
				&sd,
				&g_pSwapChain,
				&g_pd3dDevice,
				&featureLevel,
				&g_pd3dDeviceContext
			);

			if (FAILED(res))
			{
				char msg[256];
				snprintf(msg, sizeof(msg), "D3D11CreateDeviceAndSwapChain failed. HRESULT = 0x%08X", (unsigned int)res);
				MessageBoxA(nullptr, msg, "Error", MB_ICONERROR);
				return false;
			}
		}
	}

	CreateRenderTarget();
	return true;
}

// Очистка DirectX 11
void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

// Создание render target
void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer = nullptr;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (pBackBuffer)
	{
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}
}

// Очистка render target
void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
