#pragma once
#include "DXREngine.h"

class DXREngine;

class DXRScene
{
public:

	DXRScene();

	void Update();
	void Draw();

private:
	std::weak_ptr<DXREngine> Engine;
	std::shared_ptr<DXRRenderer> Render;
};