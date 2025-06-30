#pragma once
#include <vector>
#include <optional>
template<typename T>
using opt = std::optional<T>;
#include <string>
using std::string;
using std::vector;
#include "Util.h"

struct NodeMenuItem;
class NodeMenu
{
public:
	~NodeMenu();
	vector<NodeMenuItem> menus;
	NodeMenu& add(string s, opt<Color32> color = std::nullopt);
	NodeMenu& add_submenu(string name, NodeMenu& m);
	opt<int> find_item(string name) const;
};
struct NodeMenuItem {
	string name;
	opt<NodeMenu> menu;
	opt<Color32> color;
};
inline NodeMenu& NodeMenu::add(string s, opt<Color32> color) {
	menus.push_back({ s, std::nullopt, color });
	return *this;
}
inline NodeMenu& NodeMenu::add_submenu(string name, NodeMenu& m) {
	menus.push_back({ name,std::move(m) });
	return *this;
}
inline opt<int> NodeMenu::find_item(string name) const {
	for (int i = 0; i < menus.size(); i++)
		if (menus[i].name == name)
			return i;
	return std::nullopt;
}
inline NodeMenu::~NodeMenu() {}
