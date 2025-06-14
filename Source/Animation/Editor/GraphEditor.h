#pragma once
#include <vector>
#include <unordered_set>
#include "Framework/Hashmap.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/Util.h"
#include "Framework/ClassBase.h"

using std::vector;
using std::string;

class GraphObjectBase;
class GraphEditor
{
public:
	hash_map<GraphObjectBase*> nodes;
};
class GraphObjectBase : public ClassBase
{
public:
	int id = -1;

};
class GraphObjectComment : public GraphObjectBase
{
public:
	string comment = "";
	Color32 color = COLOR_WHITE;
};
class GraphNodeBase : public GraphObjectBase
{
public:
};