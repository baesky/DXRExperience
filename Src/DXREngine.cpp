#include "DXREngine.h"
using namespace std;

LRESULT CALLBACK DXREngine::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
		/*if (pSample)
		{
			pSample->OnUpdate();
			pSample->OnRender();
		}*/
		GetEngine().lock()->GetRender().lock()->Draw();
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
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

	Renderer = std::make_shared<DXRRenderer>();
	Renderer->Init(WinHandle, ScreenWidth, ScreenHeight);
}

void DXREngine::Run()
{
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Process any messages in the queue.
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void DXREngine::Exit()
{

}

weak_ptr<DXRScene> DXREngine::GetScene()
{
	return CurrentScene;
}
weak_ptr<DXRRenderer> DXREngine::GetRender()
{
	return Renderer;
}