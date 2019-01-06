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
	DXREngine* Engine;
	DXRRenderer* Render;
};