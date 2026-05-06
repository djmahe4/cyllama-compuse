#ifdef __APPLE__

#include "../include/platform_impl.hpp"
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

#include <ApplicationServices/ApplicationServices.h>

// -------------------------------------------------------------------------
// Helper: map AX role string to ElementRole
// -------------------------------------------------------------------------
static ElementRole ax_role_to_element_role(CFStringRef role) {
    if (!role) return ElementRole::Unknown;

    struct RoleMap { CFStringRef ax; ElementRole er; };
    static const RoleMap table[] = {
        { kAXWindowRole,          ElementRole::Window      },
        { kAXButtonRole,          ElementRole::Button      },
        { kAXCheckBoxRole,        ElementRole::CheckBox    },
        { kAXComboBoxRole,        ElementRole::ComboBox    },
        { kAXTextFieldRole,       ElementRole::Edit        },
        { kAXTextAreaRole,        ElementRole::Edit        },
        { kAXListRole,            ElementRole::List        },
        { kAXMenuRole,            ElementRole::Menu        },
        { kAXMenuItemRole,        ElementRole::MenuItem    },
        { kAXScrollBarRole,       ElementRole::ScrollBar   },
        { kAXTabGroupRole,        ElementRole::Tab         },
        { kAXRadioButtonRole,     ElementRole::RadioButton },
        { kAXSliderRole,          ElementRole::Slider      },
        { kAXStaticTextRole,      ElementRole::Text        },
        { kAXGroupRole,           ElementRole::Group       },
        { kAXImageRole,           ElementRole::Image       },
        { kAXLinkRole,            ElementRole::Link        },
        { kAXProgressIndicatorRole, ElementRole::ProgressBar },
        { kAXTableRole,           ElementRole::Table       },
        { kAXRowRole,             ElementRole::TableItem   },
        { kAXOutlineRole,         ElementRole::Tree        },
    };
    for (const auto& m : table) {
        if (CFStringCompare(role, m.ax, 0) == kCFCompareEqualTo)
            return m.er;
    }
    return ElementRole::Custom;
}

// -------------------------------------------------------------------------
// Helper: CFString → std::string (UTF-8)
// -------------------------------------------------------------------------
static std::string cfstring_to_utf8(CFStringRef s) {
    if (!s) return {};
    CFIndex len = CFStringGetLength(s);
    CFIndex maxBytes = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::string result(static_cast<size_t>(maxBytes), '\0');
    if (CFStringGetCString(s, &result[0], maxBytes, kCFStringEncodingUTF8)) {
        result.resize(std::strlen(result.c_str()));
        return result;
    }
    return {};
}

// -------------------------------------------------------------------------
// MacOSBackend
// -------------------------------------------------------------------------
class MacOSBackend : public PlatformBackend {
public:
    bool init() override {
        return AXIsProcessTrusted();
    }

    std::vector<ElementInfo> enumerate_elements(uint32_t max_elements) override {
        std::vector<ElementInfo> results;
        AXUIElementRef systemWide = AXUIElementCreateSystemWide();
        if (!systemWide) return results;

        walk_element(systemWide, results, max_elements, 0);
        CFRelease(systemWide);
        return results;
    }

    std::unique_ptr<ElementInfo> get_element(const ElementId& id) override {
        auto elements = enumerate_elements(0);
        for (auto& el : elements) {
            if (el.id == id) return std::make_unique<ElementInfo>(el);
        }
        return nullptr;
    }

    int click_element(const ElementId& id) override {
        // Re-enumerate to find the element, then perform kAXPressAction
        auto elements = enumerate_elements(0);
        for (auto& el : elements) {
            if (el.id == id) {
                // We don't hold the AXUIElementRef; use CGEvent as fallback
                CGEventRef down = CGEventCreateMouseEvent(
                    nullptr, kCGEventLeftMouseDown,
                    CGPointMake(el.rect.center_x(), el.rect.center_y()),
                    kCGMouseButtonLeft);
                CGEventRef up = CGEventCreateMouseEvent(
                    nullptr, kCGEventLeftMouseUp,
                    CGPointMake(el.rect.center_x(), el.rect.center_y()),
                    kCGMouseButtonLeft);
                if (down) { CGEventPost(kCGHIDEventTap, down); CFRelease(down); }
                if (up)   { CGEventPost(kCGHIDEventTap, up);   CFRelease(up);   }
                return 0;
            }
        }
        return -1;
    }

    int type_text(const ElementId& id, const std::string& text) override {
        (void)id;
        // Post key events for each character
        for (char c : text) {
            UniChar uc = static_cast<UniChar>(c);
            CGEventRef down = CGEventCreateKeyboardEvent(nullptr, 0, true);
            CGEventRef up   = CGEventCreateKeyboardEvent(nullptr, 0, false);
            if (down) {
                CGEventKeyboardSetUnicodeString(down, 1, &uc);
                CGEventPost(kCGHIDEventTap, down);
                CFRelease(down);
            }
            if (up) {
                CGEventKeyboardSetUnicodeString(up, 1, &uc);
                CGEventPost(kCGHIDEventTap, up);
                CFRelease(up);
            }
        }
        return 0;
    }

    int scroll_element(const ElementId& id, int direction, int amount) override {
        auto info = get_element(id);
        if (!info) return -1;

        int32_t delta = direction > 0 ? amount : -amount;
        CGEventRef scroll = CGEventCreateScrollWheelEvent(
            nullptr, kCGScrollEventUnitLine, 1,
            static_cast<int32_t>(delta));
        if (!scroll) return -1;
        CGEventPost(kCGHIDEventTap, scroll);
        CFRelease(scroll);
        return 0;
    }

    const char* name() const noexcept override { return "macos_axapi"; }

private:
    static const int kMaxDepth = 8;

    void walk_element(AXUIElementRef el,
                      std::vector<ElementInfo>& results,
                      uint32_t max_elements,
                      int depth)
    {
        if (!el) return;
        if (depth > kMaxDepth) return;
        if (max_elements > 0 && results.size() >= max_elements) return;

        ElementInfo info;
        std::memset(&info, 0, sizeof(info));

        // Role
        CFStringRef roleRef = nullptr;
        if (AXUIElementCopyAttributeValue(el, kAXRoleAttribute,
                reinterpret_cast<CFTypeRef*>(&roleRef)) == kAXErrorSuccess
            && roleRef)
        {
            ElementRole role = ax_role_to_element_role(roleRef);
            info.id.role = static_cast<uint32_t>(role);
            std::string rname = cfstring_to_utf8(roleRef);
            snprintf(info.role_name, sizeof(info.role_name), "%s", rname.c_str());
            CFRelease(roleRef);
        }

        // Name
        CFStringRef nameRef = nullptr;
        if (AXUIElementCopyAttributeValue(el, kAXTitleAttribute,
                reinterpret_cast<CFTypeRef*>(&nameRef)) == kAXErrorSuccess
            && nameRef)
        {
            std::string n = cfstring_to_utf8(nameRef);
            snprintf(info.name, sizeof(info.name), "%s", n.c_str());
            CFRelease(nameRef);
        }

        // Value
        CFTypeRef valueRef = nullptr;
        if (AXUIElementCopyAttributeValue(el, kAXValueAttribute, &valueRef) == kAXErrorSuccess
            && valueRef)
        {
            if (CFGetTypeID(valueRef) == CFStringGetTypeID()) {
                std::string v = cfstring_to_utf8(static_cast<CFStringRef>(valueRef));
                snprintf(info.value, sizeof(info.value), "%s", v.c_str());
            }
            CFRelease(valueRef);
        }

        // Bounding rect
        CFTypeRef posRef = nullptr, sizeRef = nullptr;
        CGPoint pos = {}; CGSize sz = {};
        if (AXUIElementCopyAttributeValue(el, kAXPositionAttribute, &posRef) == kAXErrorSuccess
            && posRef)
        {
            AXValueGetValue(static_cast<AXValueRef>(posRef), kAXValueCGPointType, &pos);
            CFRelease(posRef);
        }
        if (AXUIElementCopyAttributeValue(el, kAXSizeAttribute, &sizeRef) == kAXErrorSuccess
            && sizeRef)
        {
            AXValueGetValue(static_cast<AXValueRef>(sizeRef), kAXValueCGSizeType, &sz);
            CFRelease(sizeRef);
        }
        info.rect.x      = static_cast<int32_t>(pos.x);
        info.rect.y      = static_cast<int32_t>(pos.y);
        info.rect.width  = static_cast<int32_t>(sz.width);
        info.rect.height = static_cast<int32_t>(sz.height);

        // PID
        pid_t pid = 0;
        AXUIElementGetPid(el, &pid);
        info.id.process_id = static_cast<uint64_t>(pid);

        info.id.source = IdSource::Accessibility;
        info.source    = IdSource::Accessibility;

        if (info.rect.is_valid() || info.name[0] != '\0') {
            results.push_back(info);
        }

        // Recurse into children
        CFArrayRef children = nullptr;
        if (AXUIElementCopyAttributeValue(el, kAXChildrenAttribute,
                reinterpret_cast<CFTypeRef*>(&children)) == kAXErrorSuccess
            && children)
        {
            CFIndex n = CFArrayGetCount(children);
            for (CFIndex i = 0; i < n; ++i) {
                auto child = reinterpret_cast<AXUIElementRef>(
                    CFArrayGetValueAtIndex(children, i));
                walk_element(child, results, max_elements, depth + 1);
            }
            CFRelease(children);
        }
    }
};

// -------------------------------------------------------------------------
// Factory
// -------------------------------------------------------------------------
std::unique_ptr<PlatformBackend> create_platform_backend() {
    auto b = std::make_unique<MacOSBackend>();
    if (!b->init()) {
        return create_ocr_fallback_backend();
    }
    return b;
}

#endif  // __APPLE__
