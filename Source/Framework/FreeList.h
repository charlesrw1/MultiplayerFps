#pragma once
#include <vector>
#include <cassert>

#ifdef _DEBUG 
#define FREELIST_CHECK
#endif // _DEBUG 

template<typename T>
class Free_List
{
public:
	using handle_type = int;

	bool check_handle(handle_type handle) {
		return (handle >= 0 && handle < handle_to_obj.size()) &&
			(handle_to_obj[handle] >= 0 && handle_to_obj[handle] < objects.size());
	}

	T& get(handle_type handle) {
#ifdef FREELIST_CHECK
		if (!check_handle(handle)) {
			printf("FREELIST invalid ptr\n");
			std::abort();
		}
#endif
		return objects[handle_to_obj[handle]].type_;
	}
	handle_type make_new() {
		handle_type h = 0;
		if (free_handles.empty()) {
			h = first_free++;
			handle_to_obj.push_back(objects.size());
		}
		else {
			h = free_handles.back();
			free_handles.pop_back();
			handle_to_obj[h] = objects.size();
		}
		objects.push_back({ h, T() });

		return h;
	}
	void free(handle_type handle) {
#ifdef FREELIST_CHECK
		if (!check_handle(handle)) {
			printf("FREELIST invalid ptr\n");
			std::abort();
		}
#endif
		int obj_index = handle_to_obj[handle];
		handle_to_obj[handle] = -1;
		objects[obj_index] = objects.back();
		handle_to_obj[objects[obj_index].handle] = obj_index;
		objects.pop_back();
		free_handles.push_back(handle);
	}

	// NOT INTERNALLY SAFE
	// ONLY USE IN MAIN THREAD AND SYNC PERIOD
	handle_type first_free = 0;
	std::vector<handle_type> free_handles;

	std::vector<int> handle_to_obj;
	struct pair {
		handle_type handle = -1;
		T type_;
	};
	std::vector<pair> objects;
};