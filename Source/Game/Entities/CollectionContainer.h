#pragma once
#include "Game/Entity.h"
#include <unordered_set>
NEWCLASS(EditorCollectionContainer,Entity)
public:
	struct Data {
		std::unordered_set<uint64_t> handles_in_collection;
		std::string collection_name;
		Color32 color=COLOR_WHITE;
	};
	REFLECT(type="EditorCollectionData");
	Data data;
};