#pragma once
#include <Windows.h>
#include <memory>

#include "DXRRenderer.h"
#include "DXRScene.h"

class DXRScene;

class DXREngine
{
public:
	DXREngine() = default;

	static std::weak_ptr<DXREngine> GetEngine()
	{
		static std::shared_ptr<DXREngine> Engine = nullptr;
		if (Engine == nullptr)
		{
			Engine = std::make_shared<DXREngine>();
		}

		return Engine;
	}

	void Init(HINSTANCE hInstance, int nCmdShow, int Width, int Height);
	void Run();
	void Exit();
	
	void SetupWindow(HINSTANCE hInstance, int nCmdShow);

	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	std::weak_ptr<DXRScene> GetScene();
	std::weak_ptr<DXRRenderer> GetRender();

private:
	int ScreenWidth;
	int ScreenHeight;

	HWND WinHandle;

	std::shared_ptr<DXRRenderer> Renderer;
	std::shared_ptr<DXRScene> CurrentScene;
};