#pragma once

class Entity;
class DeferredSpawnScope
{
public:
	DeferredSpawnScope(Entity* ptr) : entityPtr(ptr) {}
	DeferredSpawnScope(const DeferredSpawnScope& other) = delete;
	DeferredSpawnScope(DeferredSpawnScope&& other) = default;
	DeferredSpawnScope& operator=(const DeferredSpawnScope& other) = delete;
	~DeferredSpawnScope();

	Entity* entityPtr = nullptr;
};
class UnserializedSceneFile;
class DeferredSpawnScopePrefab
{
public:
	DeferredSpawnScopePrefab(UnserializedSceneFile* file) : file(file) {}
	DeferredSpawnScopePrefab(const DeferredSpawnScopePrefab& other) = delete;
	DeferredSpawnScopePrefab(DeferredSpawnScopePrefab&& other) = default;
	DeferredSpawnScopePrefab& operator=(const DeferredSpawnScopePrefab& other) = delete;
	~DeferredSpawnScopePrefab();

	UnserializedSceneFile* file = nullptr;
};