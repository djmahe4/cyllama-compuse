#if defined(__linux__) && !defined(_WIN32) && !defined(__APPLE__)

#include "../include/platform_impl.hpp"
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

#ifdef HAVE_ATSPI
#include <atspi/atspi.h>
#endif

// -------------------------------------------------------------------------
// Helper: map AT-SPI role to ElementRole
// -------------------------------------------------------------------------
#ifdef HAVE_ATSPI
static ElementRole atspi_role_to_element_role(AtspiRole role) {
    switch (role) {
        case ATSPI_ROLE_WINDOW:           return ElementRole::Window;
        case ATSPI_ROLE_PUSH_BUTTON:      return ElementRole::Button;
        case ATSPI_ROLE_CHECK_BOX:        return ElementRole::CheckBox;
        case ATSPI_ROLE_COMBO_BOX:        return ElementRole::ComboBox;
        case ATSPI_ROLE_TEXT:             return ElementRole::Edit;
        case ATSPI_ROLE_ENTRY:            return ElementRole::Edit;
        case ATSPI_ROLE_LIST:             return ElementRole::List;
        case ATSPI_ROLE_LIST_ITEM:        return ElementRole::ListItem;
        case ATSPI_ROLE_MENU:             return ElementRole::Menu;
        case ATSPI_ROLE_MENU_ITEM:        return ElementRole::MenuItem;
        case ATSPI_ROLE_SCROLL_BAR:       return ElementRole::ScrollBar;
        case ATSPI_ROLE_PAGE_TAB_LIST:    return ElementRole::Tab;
        case ATSPI_ROLE_PAGE_TAB:         return ElementRole::TabItem;
        case ATSPI_ROLE_LABEL:            return ElementRole::Text;
        case ATSPI_ROLE_TOOL_BAR:         return ElementRole::ToolBar;
        case ATSPI_ROLE_TOOL_TIP:         return ElementRole::ToolTip;
        case ATSPI_ROLE_TREE:             return ElementRole::Tree;
        case ATSPI_ROLE_TREE_ITEM:        return ElementRole::TreeItem;
        case ATSPI_ROLE_DOCUMENT_FRAME:   return ElementRole::Document;
        case ATSPI_ROLE_PANEL:            return ElementRole::Pane;
        case ATSPI_ROLE_PROGRESS_BAR:     return ElementRole::ProgressBar;
        case ATSPI_ROLE_RADIO_BUTTON:     return ElementRole::RadioButton;
        case ATSPI_ROLE_SLIDER:           return ElementRole::Slider;
        case ATSPI_ROLE_SPIN_BUTTON:      return ElementRole::Spinner;
        case ATSPI_ROLE_TABLE:            return ElementRole::Table;
        case ATSPI_ROLE_TABLE_CELL:       return ElementRole::TableItem;
        default:                          return ElementRole::Custom;
    }
}
#endif  // HAVE_ATSPI

// -------------------------------------------------------------------------
// LinuxBackend
// -------------------------------------------------------------------------
class LinuxBackend : public PlatformBackend {
public:
    bool init() override {
#ifdef HAVE_ATSPI
        int ret = atspi_init();
        return ret == 0 || ret == 1;  // 1 = already initialised
#else
        return false;  // no AT-SPI headers; use fallback
#endif
    }

    std::vector<ElementInfo> enumerate_elements(uint32_t max_elements) override {
        std::vector<ElementInfo> results;
#ifdef HAVE_ATSPI
        AtspiAccessible* desktop = atspi_get_desktop(0);
        if (!desktop) return results;

        walk_accessible(desktop, results, max_elements, 0);
        g_object_unref(desktop);
#endif
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
#ifdef HAVE_ATSPI
        auto elements = enumerate_elements(0);
        for (auto& el : elements) {
            if (el.id == id) {
                // Use AT-SPI component interface to click
                // We'd need the AtspiAccessible* — re-walk to find it
                AtspiAccessible* desktop = atspi_get_desktop(0);
                if (!desktop) return -1;

                int result = find_and_click(desktop, id, 0);
                g_object_unref(desktop);
                return result;
            }
        }
#endif
        return -1;
    }

    int type_text(const ElementId& id, const std::string& text) override {
        // AT-SPI text insertion via EditableText interface
        (void)id; (void)text;
        return -1;  // Platform-specific; use pyautogui fallback
    }

    int scroll_element(const ElementId& id, int direction, int amount) override {
        (void)id; (void)direction; (void)amount;
        return -1;
    }

    const char* name() const noexcept override { return "linux_atspi"; }

private:
    static const int kMaxDepth = 8;

#ifdef HAVE_ATSPI
    void walk_accessible(AtspiAccessible* acc,
                         std::vector<ElementInfo>& results,
                         uint32_t max_elements,
                         int depth)
    {
        if (!acc || depth > kMaxDepth) return;
        if (max_elements > 0 && results.size() >= max_elements) return;

        GError* err = nullptr;
        ElementInfo info;
        std::memset(&info, 0, sizeof(info));

        // Name
        gchar* name = atspi_accessible_get_name(acc, &err);
        if (name) {
            snprintf(info.name, sizeof(info.name), "%s", name);
            g_free(name);
        }
        if (err) { g_error_free(err); err = nullptr; }

        // Role
        AtspiRole role = atspi_accessible_get_role(acc, &err);
        if (err) { g_error_free(err); err = nullptr; }
        info.id.role = static_cast<uint32_t>(atspi_role_to_element_role(role));

        gchar* role_name = atspi_accessible_get_role_name(acc, &err);
        if (role_name) {
            snprintf(info.role_name, sizeof(info.role_name), "%s", role_name);
            g_free(role_name);
        }
        if (err) { g_error_free(err); err = nullptr; }

        // Bounding rect via Component interface
        AtspiComponent* comp = reinterpret_cast<AtspiComponent*>(
            atspi_accessible_get_action(acc));  // placeholder
        (void)comp;

        // Use atspi_component_get_extents
        AtspiRect* ext = atspi_component_get_extents(
            reinterpret_cast<AtspiComponent*>(
                atspi_accessible_get_component(acc)),
            ATSPI_COORD_TYPE_SCREEN, &err);
        if (ext) {
            info.rect.x      = ext->x;
            info.rect.y      = ext->y;
            info.rect.width  = ext->width;
            info.rect.height = ext->height;
            g_free(ext);
        }
        if (err) { g_error_free(err); err = nullptr; }

        info.id.source = IdSource::Accessibility;
        info.source    = IdSource::Accessibility;

        if (info.name[0] != '\0' || info.rect.is_valid()) {
            results.push_back(info);
        }

        // Recurse
        int child_count = atspi_accessible_get_child_count(acc, &err);
        if (err) { g_error_free(err); err = nullptr; }

        for (int i = 0; i < child_count; ++i) {
            if (max_elements > 0 && results.size() >= max_elements) break;
            AtspiAccessible* child =
                atspi_accessible_get_child_at_index(acc, i, &err);
            if (err) { g_error_free(err); err = nullptr; }
            if (child) {
                walk_accessible(child, results, max_elements, depth + 1);
                g_object_unref(child);
            }
        }
    }

    int find_and_click(AtspiAccessible* acc, const ElementId& id, int depth) {
        if (!acc || depth > kMaxDepth) return -1;

        GError* err = nullptr;

        // Check if this element matches
        ElementInfo info;
        std::memset(&info, 0, sizeof(info));
        AtspiRole role = atspi_accessible_get_role(acc, &err);
        if (err) { g_error_free(err); err = nullptr; }
        info.id.role = static_cast<uint32_t>(atspi_role_to_element_role(role));

        if (info.id.role == id.role) {
            AtspiAction* action = atspi_accessible_get_action_iface(acc);
            if (action) {
                atspi_action_do_action(action, 0, &err);
                if (err) { g_error_free(err); }
                else return 0;
            }
        }

        // Recurse
        int count = atspi_accessible_get_child_count(acc, &err);
        if (err) { g_error_free(err); err = nullptr; }
        for (int i = 0; i < count; ++i) {
            AtspiAccessible* child =
                atspi_accessible_get_child_at_index(acc, i, &err);
            if (err) { g_error_free(err); err = nullptr; }
            if (child) {
                int r = find_and_click(child, id, depth + 1);
                g_object_unref(child);
                if (r == 0) return 0;
            }
        }
        return -1;
    }
#endif  // HAVE_ATSPI
};

// -------------------------------------------------------------------------
// Factory
// -------------------------------------------------------------------------
std::unique_ptr<PlatformBackend> create_platform_backend() {
    auto b = std::make_unique<LinuxBackend>();
    if (!b->init()) {
        return create_ocr_fallback_backend();
    }
    return b;
}

#endif  // defined(__linux__) && !defined(_WIN32) && !defined(__APPLE__)
