#include "DXREngine.h"

static std::shared_ptr<DXREngine> GEngine = DXREngine::GetEngine().lock();

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	GEngine->Init(hInstance, nShowCmd, 1280, 720);
	GEngine->Run();
	GEngine->Exit();
}