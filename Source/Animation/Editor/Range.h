#pragma once
#include <vector>
class Range
{
public:
	Range(int end, int start = 0) {
		this->endidx = end;
		this->start = start;
		if (this->endidx < start)
			this->endidx = start;
	}
	template<typename T>
	Range(const std::vector<T>& vec) {
		this->end = (int)vec.size();
	}
	struct Iterator {
		Iterator(int idx) : idx(idx) {}
		int operator*() {
			return idx;
		}
		Iterator& operator++() {
			idx += 1;
			return *this;
		}
		bool operator!=(const Iterator& other) {
			return idx != other.idx;
		}
		int idx = 0;
	};
	Iterator begin() {
		return Iterator(start);
	}
	Iterator end() {
		return Iterator(endidx);
	}

	int start = 0;
	int endidx = 0;
};