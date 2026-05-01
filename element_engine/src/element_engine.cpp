#include "../include/element_engine.hpp"
#include <cstdlib>
#include <cstring>
#include <new>

// -------------------------------------------------------------------------
// CRC-32 (bitwise, no lookup table)
// -------------------------------------------------------------------------
namespace detail {

uint32_t crc32_bytes(const void* data, size_t len) noexcept {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = p[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mix = (crc ^ byte) & 1u;
            crc = (crc >> 1) ^ (0xEDB88320u * mix);
            byte >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// FNV-1a 64-bit hash over raw bytes
static uint64_t fnv1a_64(const void* data, size_t len) noexcept {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

size_t ElementIdHash::operator()(const ElementId& id) const noexcept {
    return static_cast<size_t>(fnv1a_64(&id, sizeof(ElementId)));
}

}  // namespace detail

// -------------------------------------------------------------------------
// ElementEngine constructor
// -------------------------------------------------------------------------
ElementEngine::ElementEngine() {
    try {
        backend = create_platform_backend();
    } catch (...) {
        backend = create_ocr_fallback_backend();
    }
}

// -------------------------------------------------------------------------
// C API — lifecycle
// -------------------------------------------------------------------------

extern "C" {

ElementEngine* element_engine_create(void) {
    try {
        auto* engine = new ElementEngine();
        if (!engine->ready()) {
            delete engine;
            return nullptr;
        }
        return engine;
    } catch (...) {
        return nullptr;
    }
}

void element_engine_destroy(ElementEngine* engine) {
    delete engine;
}

// -------------------------------------------------------------------------
// C API — enumeration
// -------------------------------------------------------------------------

ElementInfo** element_engine_enumerate(
    ElementEngine* engine,
    uint32_t       max_elements,
    uint32_t*      out_count)
{
    if (!engine || !out_count) return nullptr;
    *out_count = 0;

    std::vector<ElementInfo> elements;
    try {
        std::lock_guard<std::mutex> lock(engine->mtx);
        elements = engine->backend->enumerate_elements(max_elements);
    } catch (...) {
        return nullptr;
    }

    if (elements.empty()) return nullptr;

    uint32_t count = static_cast<uint32_t>(elements.size());
    ElementInfo** arr = static_cast<ElementInfo**>(
        std::malloc(count * sizeof(ElementInfo*)));
    if (!arr) return nullptr;

    for (uint32_t i = 0; i < count; ++i) {
        arr[i] = static_cast<ElementInfo*>(std::malloc(sizeof(ElementInfo)));
        if (!arr[i]) {
            for (uint32_t j = 0; j < i; ++j) std::free(arr[j]);
            std::free(arr);
            return nullptr;
        }
        std::memcpy(arr[i], &elements[i], sizeof(ElementInfo));

        // Update cache
        engine->cache[elements[i].id] = elements[i];
    }

    *out_count = count;
    return arr;
}

void element_info_free_array(ElementInfo** array, uint32_t count) {
    if (!array) return;
    for (uint32_t i = 0; i < count; ++i) {
        std::free(array[i]);
    }
    std::free(array);
}

// -------------------------------------------------------------------------
// C API — element operations
// -------------------------------------------------------------------------

ElementInfo* element_get_info(ElementEngine* engine, const ElementId* id) {
    if (!engine || !id) return nullptr;
    try {
        std::lock_guard<std::mutex> lock(engine->mtx);
        auto info = engine->backend->get_element(*id);
        if (!info) return nullptr;
        engine->cache[*id] = *info;
        ElementInfo* result = static_cast<ElementInfo*>(
            std::malloc(sizeof(ElementInfo)));
        if (!result) return nullptr;
        std::memcpy(result, info.get(), sizeof(ElementInfo));
        return result;
    } catch (...) {
        return nullptr;
    }
}

int element_click(ElementEngine* engine, const ElementId* id) {
    if (!engine || !id) return -1;
    try {
        std::lock_guard<std::mutex> lock(engine->mtx);
        return engine->backend->click_element(*id);
    } catch (...) {
        return -1;
    }
}

int element_type_text(ElementEngine* engine, const ElementId* id, const char* text) {
    if (!engine || !id || !text) return -1;
    try {
        std::lock_guard<std::mutex> lock(engine->mtx);
        return engine->backend->type_text(*id, std::string(text));
    } catch (...) {
        return -1;
    }
}

int element_scroll(ElementEngine* engine, const ElementId* id, int direction, int amount) {
    if (!engine || !id) return -1;
    try {
        std::lock_guard<std::mutex> lock(engine->mtx);
        return engine->backend->scroll_element(*id, direction, amount);
    } catch (...) {
        return -1;
    }
}

// -------------------------------------------------------------------------
// C API — caching & comparison
// -------------------------------------------------------------------------

bool element_id_equal(const ElementId* a, const ElementId* b) {
    if (!a || !b) return false;
    return *a == *b;
}

uint64_t element_id_hash(const ElementId* id) {
    if (!id) return 0;
    return detail::fnv1a_64(id, sizeof(ElementId));
}

void element_engine_flush_cache(ElementEngine* engine) {
    if (!engine) return;
    std::lock_guard<std::mutex> lock(engine->mtx);
    engine->cache.clear();
}

}  // extern "C"
