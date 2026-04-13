// AgentMaster.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "AgentMaster.h"

// Model
#include "Node.hpp"

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
#include <stdio.h>
#include <vector>
#include <set>

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна
HWND    g_hWnd = nullptr;                       // дескриптор главного окна

// Модель: узлы и связи
static std::vector<Node*> g_nodes;

// Следующий ID для новых узлов
static int g_nextNodeId = 1;

// Отслеживание узлов, чья позиция уже установлена в ImNodes
static std::set<int> g_nodesPositionSet;

// Режим размещения нового узла (клик по каталогу → ожидание клика на canvas)
static bool g_placingNode = false;
static NodeType g_pendingNodeType = NodeType::Input;
static int g_placingNodeFrame = 0;
static int g_currentFrame = 0;

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

	// Создать фиксированные узлы Input и Output
	InputNode* inputNode = new InputNode(g_nextNodeId++);
	inputNode->SetPos(100.0f, 300.0f);
	g_nodes.push_back(inputNode);

	OutputNode* outputNode = new OutputNode(g_nextNodeId++);
	outputNode->SetPos(600.0f, 300.0f);
	g_nodes.push_back(outputNode);

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
			const char* toolNames[] = { "Text", "Triplet", "Router" };
			NodeType toolTypes[] = { NodeType::Text, NodeType::Triplet, NodeType::Router };

			for (int i = 0; i < 3; i++)
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

					Node* newNode = nullptr;
					switch (g_pendingNodeType)
					{
					case NodeType::Text:
						newNode = new TextNode(g_nextNodeId++);
						break;
					case NodeType::Triplet:
						newNode = new TripletNode(g_nextNodeId++);
						break;
					case NodeType::Router:
						newNode = new RouterNode(g_nextNodeId++);
						break;
					default:
						break;
					}

					if (newNode != nullptr)
					{
						newNode->SetPos(nodeX, nodeY);
						g_nodes.push_back(newNode);
						g_placingNode = false;
					}
				}

				// Визуальная подсказка — текст рядом с курсором
				ImGui::SetNextWindowBgAlpha(0.8f);
				ImGui::BeginTooltip();
				ImGui::Text("Place %s node", g_pendingNodeType == NodeType::Text ? "Text" :
					g_pendingNodeType == NodeType::Triplet ? "Triplet" : "Router");
				ImGui::EndTooltip();
			}

			ImNodes::BeginNodeEditor();

			// Отрисовка всех узлов
			for (Node* node : g_nodes)
			{
				// При первом рендере — установить начальную позицию
				if (g_nodesPositionSet.find(node->GetId()) == g_nodesPositionSet.end())
				{
					ImNodes::SetNodeGridSpacePos(node->GetId(), ImVec2(node->GetX(), node->GetY()));
					g_nodesPositionSet.insert(node->GetId());
				}

				ImNodes::BeginNode(node->GetId());
				ImNodes::BeginNodeTitleBar();
				ImGui::TextUnformatted(node->GetTypeName());
				ImNodes::EndNodeTitleBar();

				// Входы
				for (int i = 0; i < node->GetInputCount(); i++)
				{
					ImNodes::BeginInputAttribute(node->GetId() * 1000 + i);
					ImGui::Text("In %d", i);
					ImNodes::EndInputAttribute();
				}

				// Выходы
				for (int i = 0; i < node->GetOutputCount(); i++)
				{
					ImNodes::BeginOutputAttribute(node->GetId() * 1000 + i + 100);
					ImGui::Text("Out %d", i);
					ImNodes::EndOutputAttribute();
				}

				ImNodes::EndNode();

				// Синхронизировать позицию из ImNodes в модель
				ImVec2 gridPos = ImNodes::GetNodeGridSpacePos(node->GetId());
				node->SetPos(gridPos.x, gridPos.y);
			}

			ImNodes::EndNodeEditor();
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

	// Очистка узлов
	for (Node* node : g_nodes)
	{
		delete node;
	}
	g_nodes.clear();

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
