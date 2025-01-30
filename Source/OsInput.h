#pragma once

class OsInput
{
public:
	OsInput() {
		memset(keys, 0, sizeof(keys));
		memset(keychanges, 0, sizeof(keychanges));
	}

	bool keys[SDL_NUM_SCANCODES];
	bool keychanges[SDL_NUM_SCANCODES];
	int mousekeys = 0;
};