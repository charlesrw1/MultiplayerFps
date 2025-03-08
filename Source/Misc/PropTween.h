#pragma once

#include <memory>

class TweenProp
{
public:
	void stop();
	void start();
};

class TweenObject
{
public:
	TweenProp* create();
};

class TweenManager
{
public:
	TweenObject* create();
};


extern TweenManager g_tweenMgr;