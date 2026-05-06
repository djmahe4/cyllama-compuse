#pragma once
#include "element_types.hpp"
#include <vector>
#include <memory>
#include <string>

// -------------------------------------------------------------------------
// Abstract interface for platform-specific accessibility backends
// -------------------------------------------------------------------------
class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;

    // Initialise the backend.  Returns true on success.
    virtual bool init() = 0;

    // Enumerate all visible UI elements. Returns empty on failure.
    virtual std::vector<ElementInfo> enumerate_elements(uint32_t max_elements) = 0;

    // Get a fresh snapshot of one element.  Returns nullptr if not found.
    virtual std::unique_ptr<ElementInfo> get_element(const ElementId& id) = 0;

    // Perform a click on an element.
    virtual int click_element(const ElementId& id) = 0;

    // Type text into an element.
    virtual int type_text(const ElementId& id, const std::string& text) = 0;

    // Scroll an element.
    virtual int scroll_element(const ElementId& id, int direction, int amount) = 0;

    // Human-readable name for this backend.
    virtual const char* name() const noexcept = 0;
};

// Factory: returns the best available backend for the current platform.
// Falls back to OcrFallbackBackend if no accessibility API is found.
std::unique_ptr<PlatformBackend> create_platform_backend();

// Always available: create the OCR fallback backend explicitly.
std::unique_ptr<PlatformBackend> create_ocr_fallback_backend();
