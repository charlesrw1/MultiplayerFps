#pragma once
#include "GameEnginePublic.h"
class fpsApp : public Application {
public:
	CLASS_BODY(fpsApp);
	void start() override;
	void update() override;
};