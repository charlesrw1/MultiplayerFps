#pragma once
#include "Percentage.h"
#include "Framework/StringName.h"
#include "Framework/InlineVec.h"

// sync marker based
// concept not implmented yet
// esoterica engine ftw!

class SyncTime
{
public:
	int index = 0;		// event index
	Percentage time;	// percentage between [index,index+1]
};

class SyncTimeRange
{
public:
	SyncTime start;
	SyncTime end;
};

class SyncTrack
{
public:

	struct EventMarker
	{
		StringName id;
		Percentage start;
	};

	struct Event
	{
		StringName id;
		Percentage start;
		Percentage duration;
	};


	InlineVec<Event, 4> events;
};