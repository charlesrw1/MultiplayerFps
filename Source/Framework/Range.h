#pragma once
#include <vector>
#include <string>
class Range {
public:
	Range(int end) : endIdx(end) {}
	template<typename T>
	Range(const std::vector<T>& v) : endIdx((int)v.size()) {}
	Range(const std::string& s) : endIdx((int)s.size()) {}

	struct Iterator {
		int index = 0;
		int endIdx = 0;
		int operator*() const {
			return index;
		}
		Iterator& operator++() {
			index += 1;
			return *this;
		}
		bool operator!=(const Iterator& o) const {
			return index < endIdx;
		}
	};
	Iterator begin() const { return { 0,endIdx }; }
	Iterator end() const { return { 0,endIdx }; }
private:
	int endIdx = 0;
};
template<typename T>
class IPairs {
public:
	IPairs(std::vector<T>& ar) :ar(ar){
	}
	struct Data {
		int index = 0;
		T* data=nullptr;
	};
	struct Iterator {
		Iterator(std::vector<T>& ar) : ar(ar) {}

		Data operator*() const {
			return Data{ index,&ar.at(index) };
		}
		Iterator& operator++() {
			index += 1;
			return *this;
		}
		bool operator!=(const Iterator& o) const {
			return index < (int)ar.size();
		}
	private:
		std::vector<T>& ar;
		int index = 0;
	};

	Iterator begin() const { return Iterator(ar); }
	Iterator end() const { return Iterator(ar); }
private:
	std::vector<T>& ar;
};