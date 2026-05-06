#pragma once

#include "element_types.hpp"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handles
typedef struct ElementEngine  ElementEngine;
typedef struct UIElement      UIElement;    // reserved for future use

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

/** Create the engine for the default desktop.  Returns NULL on failure. */
ElementEngine* element_engine_create(void);

/** Destroy the engine and free all resources. */
void element_engine_destroy(ElementEngine* engine);

// -----------------------------------------------------------------------
// Enumeration
// -----------------------------------------------------------------------

/**
 * Enumerate all visible UI elements on the desktop.
 *
 * @param engine       Engine instance.
 * @param max_elements Maximum number of elements (0 = no limit).
 * @param out_count    [out] Number of elements returned.
 * @return Heap-allocated array of ElementInfo*; free with element_info_free_array().
 */
ElementInfo** element_engine_enumerate(
    ElementEngine* engine,
    uint32_t       max_elements,
    uint32_t*      out_count);

/** Free an array returned by element_engine_enumerate(). */
void element_info_free_array(ElementInfo** array, uint32_t count);

// -----------------------------------------------------------------------
// Element-level operations
// -----------------------------------------------------------------------

/** Get a fresh snapshot of one element by ID.  Caller frees with free(). */
ElementInfo* element_get_info(ElementEngine* engine, const ElementId* id);

/** Click an element.  Returns 0 on success. */
int element_click(ElementEngine* engine, const ElementId* id);

/** Type text into an element.  Returns 0 on success. */
int element_type_text(ElementEngine* engine, const ElementId* id, const char* text);

/** Scroll an element (direction: +1 = up, -1 = down).  Returns 0 on success. */
int element_scroll(ElementEngine* engine, const ElementId* id, int direction, int amount);

// -----------------------------------------------------------------------
// Caching & comparison
// -----------------------------------------------------------------------

/** Compare two ElementIds for equality. */
bool element_id_equal(const ElementId* a, const ElementId* b);

/** Hash an ElementId. */
uint64_t element_id_hash(const ElementId* id);

/** Flush the internal element cache. */
void element_engine_flush_cache(ElementEngine* engine);

#ifdef __cplusplus
}  // extern "C"
#endif

// -----------------------------------------------------------------------
// C++ internal engine class (not part of the stable C API)
// -----------------------------------------------------------------------
#ifdef __cplusplus
#include "platform_impl.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>

namespace detail {

// FNV-1a 32-bit hash over a string
inline uint32_t hash_string(const std::string& s) noexcept {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

// CRC-32 (ISO 3309 polynomial) — bitwise implementation, no lookup table
uint32_t crc32_bytes(const void* data, size_t len) noexcept;

struct ElementIdHash {
    size_t operator()(const ElementId& id) const noexcept;
};

}  // namespace detail

struct ElementEngine {
    std::unique_ptr<PlatformBackend> backend;
    std::unordered_map<ElementId, ElementInfo, detail::ElementIdHash> cache;
    std::mutex mtx;

    ElementEngine();
    bool ready() const noexcept { return backend != nullptr; }
};
#endif  // __cplusplus
