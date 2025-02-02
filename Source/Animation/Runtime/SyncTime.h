#pragma once
#include "Percentage.h"
#include "Framework/StringName.h"
#include "Framework/InlineVec.h"
#include "Framework/EnumDefReflection.h"

NEWENUM(sync_opt , uint8_t)
{
	Default,		// clip can by the leader if it has the highest weight
	AlwaysLeader,	// clip is always the leader
	AlwaysFollower,	// clip is always the follower
};


class Node_CFG;
struct SyncGroupData
{
	StringName name;	// hashed string name of group

	Percentage time;	// normalized time, either of full clip length or the percentage through sync marker
	StringName sync_marker_name;
	bool has_sync_marker = false;

	// thse are updated as the graph updates by checking if the node's weight is heigher than the current
	// post graph update, the last_time is copied to time for the next round of updates
	Percentage update_time;
	StringName update_sync_marker_name;
	bool update_has_sync_marker = false;

	sync_opt update_owner_synctype = sync_opt::Default;
	const Node_CFG* update_owner = nullptr;
	float update_weight = 0.0;
	bool is_first_update = true;

	bool is_this_first_update() const { return is_first_update; }

	// every clip node should do the follower:
	// call this function to determine if this sync group needs to recieve an update from this node
	// if true, then compute the nodes next time by incrementing time += dt*speed, whatever else
	// set this syncgroups time with that, also account for sync markers
	bool should_write_new_update_weight(sync_opt option, float new_update_weight) {
		if (update_owner == nullptr) return true;
		if (option == sync_opt::AlwaysLeader) return true;	// leaders always update
		if (option == sync_opt::AlwaysFollower) return false; // followers never update (except when update_owner==nullptr; ie first update)
		// else: option == sync_opt::Default; syncs when has highest blend weight
		if (update_owner_synctype == sync_opt::AlwaysFollower) return true;	// update leader is always follower, update
		if (update_owner_synctype == sync_opt::AlwaysLeader) return false;	// update leader is always leader, dont update
		// update_owner must be another sync_opt::Default, update if update_weight is higher than active
		if (new_update_weight > update_weight)
			return true;
		else
			return false;
	}
	void write_to_update_time(sync_opt option, float new_update_weight, const Node_CFG* node, Percentage newtime) {
		this->update_owner = node;
		this->update_weight = new_update_weight;
		this->update_owner_synctype = option;
		this->update_time = newtime;
		this->update_has_sync_marker = false;	// TODO
	}
};
