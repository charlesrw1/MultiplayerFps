// Source/IntegrationTests/StateDump.cpp
#define _CRT_SECURE_NO_WARNINGS
#include "StateDump.h"
#include <cstdio>
#include <string>
#include "Framework/Config.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/BaseUpdater.h"
#include "Game/Entity.h"
#include "Framework/ClassTypeInfo.h"

// Stack trace via dbghelp (Windows only)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

void print_stack_trace() {
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);

	void* frames[64];
	WORD count = CaptureStackBackTrace(1, 64, frames, NULL);

	SYMBOL_INFO* sym = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256, 1);
	sym->MaxNameLen = 255;
	sym->SizeOfStruct = sizeof(SYMBOL_INFO);

	IMAGEHLP_LINE64 line = {};
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

	fprintf(stderr, "\n=== STACK TRACE ===\n");
	for (WORD i = 0; i < count; ++i) {
		DWORD64 addr = (DWORD64)frames[i];
		SymFromAddr(process, addr, 0, sym);
		DWORD disp = 0;
		if (SymGetLineFromAddr64(process, addr, &disp, &line))
			fprintf(stderr, "  #%-2d %s  (%s:%lu)\n", i, sym->Name, line.FileName, line.LineNumber);
		else
			fprintf(stderr, "  #%-2d %s  (0x%llx)\n", i, sym->Name, (unsigned long long)addr);
	}
	fprintf(stderr, "===================\n\n");
	free(sym);
}

void print_engine_state(const char* label) {
	fprintf(stderr, "\n=== ENGINE STATE [%s] ===\n", label);

	// Config vars
	fprintf(stderr, "-- CONFIG VARS --\n");
	VarMan::get()->enumerate_vars(
		[](const ConfigVarDataPublic& v, void* /*unused*/) {
			fprintf(stderr, "  %-40s = %s\n", v.name, v.value);
		},
		nullptr);

	fprintf(stderr, "=========================\n\n");
}
