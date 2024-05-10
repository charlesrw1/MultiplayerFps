#pragma once
#include "ImSequencer.h"
#if 0
enum class sequence_type
{
	clip,
	transition,
	state
};
struct MySequence : public ImSequencer::SequenceInterface
{
	// interface with sequencer

	virtual int GetFrameMin() const {
		return mFrameMin;
	}
	virtual int GetFrameMax() const {
		return mFrameMax;
	}
	virtual int GetItemCount() const { return (int)items.size(); }

	virtual void Add(int type) {  };
	virtual void Del(int index) { }
	virtual void Duplicate(int index) { }

	virtual size_t GetCustomHeight(int index) { return 0; }

	void add_manual_track(std::string str, int start, int end) {

		uint32_t mask = 0;

		for (int i = 0; i < items.size(); i++) {
			const auto& item = items[i].back();
			if (item.start < end && item.end > start) {
				mask |= (1ull << i);
			}
		}

		auto save = current_item_bitmask;
		current_item_bitmask = mask;

		int index = start_track(str, sequence_type::clip);
		items[index].back().start = start;
		items[index].back().end = end;
		end_track(index);

		current_item_bitmask = save;
	}

	int start_track(const std::string& str, sequence_type type) {
		for (int i = 0; i < 64; i++) {
			bool active = current_item_bitmask & (1ull << (uint64_t)i);
			if (active)
				continue;

			ImSequencer::Item item;
			item.start = active_frame;
			item.end = active_frame + 1;
			item.text = get_cstr(str);

			if (i >= items.size()) {
				items.push_back({});
				ASSERT(i < items.size());
			}
			items[i].push_back(item);
			current_item_bitmask |= (1ull << (uint64_t)i);

			return i;
		}
		printf("Start_track full!\n");
		ASSERT(0);
		return 0;
	}
	void end_track(int index) {
		current_item_bitmask = current_item_bitmask & ~(1ull << index);
	}

	void continue_tracks() {
		for (int i = 0; i < 64; i++) {
			if (current_item_bitmask & (1ull << i)) {
				ASSERT(items.size() >= i);
				ASSERT(!items[i].empty());
				items[i].back().end = active_frame;
			}
		}
	}

	const char* get_cstr(std::string s) {
		if (interned_strings.find(s) != interned_strings.end())
			return interned_strings.find(s)->c_str();
		interned_strings.insert(s);
		return interned_strings.find(s)->c_str();
	}

	std::unordered_set<std::string> interned_strings;
	uint64_t current_item_bitmask = 0;
	std::vector<std::vector<ImSequencer::Item>> items;

	// my datas
	MySequence() : mFrameMin(0), mFrameMax(0) {}
	int mFrameMin, mFrameMax;
	int active_frame = 0;

	virtual void DoubleClick(int index) {

	}
	// Inherited via SequenceInterface
	virtual int GetItems(int index, ImSequencer::Item* item, int start = 0) override
	{
		if (start >= items[index].size()) return -1;

		*item = items[index][start];
		return ++start;
	}
};

class Timeline
{
public:
	AnimationGraphEditor* owner;
	Timeline(AnimationGraphEditor* o) : owner(o) {}
	void draw_imgui();

	void play() {}
	void pause() {}
	void stop() {}
	void save() {}

	bool needs_compile = false;
	bool is_reset = true;
	bool is_playing = false;
	float play_speed = 1.f;

	bool expaned = true;
	int first_frame = 0;

	int current_tick = 0;

	MySequence seq;
};
#endif