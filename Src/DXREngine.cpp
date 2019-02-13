#include "DXREngine.h"
using namespace std;

LRESULT CALLBACK DXREngine::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
		GetEngine()->bRequestExit = true;
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	
}


void DXREngine::SetupWindow(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASSEX WindowClass = { 0 };
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = hInstance;
	WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WindowClass.lpszClassName = L"DXRExprienceWindowClass";
	RegisterClassEx(&WindowClass);

	RECT WindowRect = { 0, 0, static_cast<LONG>(ScreenWidth), static_cast<LONG>(ScreenHeight) };
	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

	WinHandle = CreateWindowEx(0, WindowClass.lpszClassName, L"DXR Experience", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, ScreenWidth, ScreenHeight, nullptr, nullptr, hInstance, nullptr);

	ShowWindow(WinHandle, nCmdShow);
}

void DXREngine::Init(HINSTANCE hInstance, int nCmdShow, int Width, int Height)
{


	ScreenWidth = Width;
	ScreenHeight = Height;
	SetupWindow(hInstance, nCmdShow);

	RECT r;
	GetClientRect(WinHandle, &r);
	ScreenWidth = r.right - r.left;
	ScreenHeight = r.bottom - r.top;

	Renderer = std::make_shared<DXRRenderer>();
	Renderer->Init(WinHandle, ScreenWidth, ScreenHeight);

	bRequestExit = false;
}

void DXREngine::Run()
{
	while (!bRequestExit)
	{
		Tick();
	}
	
}

void DXREngine::Tick()
{

	PumpWinMsg();

	RenderScene();
}

void DXREngine::RenderScene()
{
	GetRender()->Draw();
}

void DXREngine::PumpWinMsg()
{
	MSG msg = {};
	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void DXREngine::Exit()
{
	GetRender()->Exit();
}

DXRScene* DXREngine::GetScene()
{
	return CurrentScene.get();
}
DXRRenderer* DXREngine::GetRender()
{
	return Renderer.get();
}
