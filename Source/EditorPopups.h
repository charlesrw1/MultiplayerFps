#pragma once
#include <functional>
#include <vector>
#include <string>
class EditorPopupManager
{
public:
	static EditorPopupManager* inst;

	void draw_popups();
	// add a callback to draw an imgui popup. return true when you want the popup to close
	void add_popup(const std::string& name, std::function<bool()> callback);
	bool has_popup_open() const;
private:
	void draw_popup_index(int index);

	struct Popup {
		std::string name;
		std::function<bool()> callback;
		int id = -1;
		bool has_opened() const;
	};
	std::vector<Popup> popup_stack;
	int id_gen = 0;
};