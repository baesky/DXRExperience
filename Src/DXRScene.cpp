#include "DXRScene.h"

DXRScene::DXRScene()
{
	Engine = DXREngine::GetEngine();
	Render = Engine.lock()->GetRender().lock();
}

void DXRScene::Update()
{

}

void DXRScene::Draw()
{
	Render->OnFrameBegin();

	const float clearColor[4] = { 0.4f, 0.6f, 0.2f, 1.0f };
	//Render->ResourceBarrier(mpCmdList, mFrameObjects[rtvIndex].pSwapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	//CmdList->ClearRenderTargetView(mFrameObjects[rtvIndex].rtvHandle, clearColor, 0, nullptr);

	Render->OnFrameEnd();
}