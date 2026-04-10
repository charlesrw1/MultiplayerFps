#pragma once
#include <memory>
class Model;

/// Custom deleter that calls ModelMan::free_dynamic_model() on the model.
/// Used as the deleter type for DynamicModelUniquePtr.
struct DynamicModelDeleter {
    void operator()(Model* m) const;
};

/// RAII handle for a dynamically-created (procedural) model.
/// On destruction, automatically frees GPU allocations and removes the model
/// from ModelMan's live set.  Equivalent in role to DynamicMatUniquePtr for
/// materials.
using DynamicModelUniquePtr = std::unique_ptr<Model, DynamicModelDeleter>;
