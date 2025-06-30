#pragma once
#ifdef EDITOR_BUILD
#include <string>
#include <vector>
#include "Framework/Util.h"
#include "GameEnginePublic.h"
#include <stdexcept>
#include "Framework/MulticastDelegate.h"
#include <functional>
class Command
{
public:
	virtual ~Command() {}
	virtual void execute() = 0;
	virtual void undo() = 0;
	virtual std::string to_string() = 0;
	virtual bool is_valid() { return true; }
};

struct SDL_KeyboardEvent;
class UndoRedoSystem
{
public:
	UndoRedoSystem();
	~UndoRedoSystem() {
		clear_all();
		for (auto& c : queued_commands)
			delete c.c;
		queued_commands.clear();
	}

	void on_key_event(const SDL_KeyboardEvent& k);

	void clear_all();
	// returns number of errord commands
	int execute_queued_commands();

	void add_command(Command* c) {
		queued_commands.push_back({ c });
	}
	void add_command_with_execute_callback(Command* c, std::function<void(bool)> callback) {
		queued_commands.push_back({ c, callback });
	}

	void undo();


	MulticastDelegate<> on_command_execute_or_undo;
	struct Queued {
		Command* c = nullptr;
		std::function<void(bool)> func;
	};
	std::vector<Queued> queued_commands;

	const int HIST_SIZE = 128;
	int index = 0;
	std::vector<Command*> hist;
};
#endif