#pragma once

class Entity;
struct DeferredSpawnScope
{
	DeferredSpawnScope(Entity* ptr) : entityPtr(ptr) {}
	DeferredSpawnScope(const DeferredSpawnScope& other) = delete;
	DeferredSpawnScope(DeferredSpawnScope&& other) = default;
	DeferredSpawnScope& operator=(const DeferredSpawnScope& other) = delete;
	~DeferredSpawnScope();

	Entity* entityPtr = nullptr;
};