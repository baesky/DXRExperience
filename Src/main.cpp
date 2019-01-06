#include "DXREngine.h"

static DXREngine* GEngine = DXREngine::GetEngine();

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	GEngine->Init(hInstance, nShowCmd, 1280, 720);
	GEngine->Run();
	GEngine->Exit();
}