#include "../include/element_engine.hpp"

// -------------------------------------------------------------------------
// Cross-frame identity cache helpers
// These functions operate under the engine's mutex (caller must hold lock).
// -------------------------------------------------------------------------

void cache_update(ElementEngine* engine, const ElementInfo& info) {
    if (!engine) return;
    engine->cache[info.id] = info;
}

const ElementInfo* cache_lookup(ElementEngine* engine, const ElementId& id) {
    if (!engine) return nullptr;
    auto it = engine->cache.find(id);
    if (it == engine->cache.end()) return nullptr;
    return &it->second;
}

void cache_flush(ElementEngine* engine) {
    if (!engine) return;
    engine->cache.clear();
}
