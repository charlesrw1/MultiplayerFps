#pragma once

template<typename T>
struct handle
{
	int id = -1;
	bool is_valid() const { return id != -1; }
};