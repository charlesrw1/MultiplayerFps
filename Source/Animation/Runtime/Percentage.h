#pragma once

class Percentage
{
public:
	Percentage(float p) : percent(p) {}
	Percentage(float t, float min, float max)
		: percent((t - min) / (max - min)) {}
	Percentage(float t, float duration) :
		percent(t / duration) {}
	Percentage() = default;

	float get() const { return percent; }

	float percent = 0.0;
};