#pragma once

class ClassBase;
class ScriptManagerPublic
{
public:
	virtual void init() = 0;
	virtual void push_global(ClassBase* class_, const char* str) = 0;
	virtual void remove_global(const char* str) = 0;
};
extern ScriptManagerPublic* g_scriptMgr;// defined in ScriptLocal