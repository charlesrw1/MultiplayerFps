#pragma once
#ifdef EDITOR_BUILD

class DiagnosticsWindow
{
public:
	static DiagnosticsWindow& get();

	void imgui_draw();
	void open() { is_open = true; }

	bool is_open = false;

private:
	void draw_asset_tab();
	void draw_map_tab();

	char search_filter[256] = {};
	bool show_error = true;
	bool show_warning = true;
	bool show_transitive = true;
	bool show_info = false;

	int selected_tab = 0;
};

#endif
