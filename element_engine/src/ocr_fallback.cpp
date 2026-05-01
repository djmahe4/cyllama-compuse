#include "../include/platform_impl.hpp"
#include <cstring>
#include <cstdio>

// -------------------------------------------------------------------------
// OcrFallbackBackend — stub; real OCR is handled by Python/EasyOCR
// -------------------------------------------------------------------------
class OcrFallbackBackend : public PlatformBackend {
public:
    bool init() override { return true; }

    std::vector<ElementInfo> enumerate_elements(uint32_t) override {
        // Return empty — Python layer will call EasyOCR instead
        return {};
    }

    std::unique_ptr<ElementInfo> get_element(const ElementId&) override {
        return nullptr;
    }

    int click_element(const ElementId&) override {
        // Fallback: caller should use pyautogui
        return -1;
    }

    int type_text(const ElementId&, const std::string&) override { return -1; }
    int scroll_element(const ElementId&, int, int) override { return -1; }
    const char* name() const noexcept override { return "ocr_fallback"; }
};

// On platforms other than Windows/macOS/Linux (or when those impls don't
// define the factory), this translation unit provides the fallback factory.
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
std::unique_ptr<PlatformBackend> create_platform_backend() {
    auto b = std::make_unique<OcrFallbackBackend>();
    b->init();
    return b;
}
#endif

// Exported so element_engine.cpp can always reach it.
std::unique_ptr<PlatformBackend> create_ocr_fallback_backend() {
    auto b = std::make_unique<OcrFallbackBackend>();
    b->init();
    return b;
}
