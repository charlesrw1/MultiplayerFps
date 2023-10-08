#pragma once

class GlobalVar
{
public:
	const static int NAME_LEN = 32;
	char name[NAME_LEN +1];
	union {
		int ival;
		float fval;
	};
	enum Type { Int, Float };
	Type type;
	bool changed = false;

	GlobalVar* next = nullptr;
};

class ConfigMgr
{
public:
	int* MakeI(const char* name, int default_val);
	float* MakeF(const char* name, float default_val);

	int GetI(const char* name);
	float GetF(const char* name);
	void SetI(const char* name, int val);
	void SetF(const char* name, float val);


	void LoadFromDisk(const char* path);
	void WriteToDisk(const char* path);

private:
	GlobalVar* FindInList(const char* name);
	GlobalVar* InitNewVar(const char* name);
	GlobalVar* head = nullptr;
	GlobalVar* tail = nullptr;
};

extern ConfigMgr cfg;