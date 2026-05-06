#pragma once
#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#  include <windows.h>
#endif

// -------------------------------------------------------------------------
// ElementRole — coarse categorisation of UI element types
// -------------------------------------------------------------------------
enum class ElementRole : uint32_t {
    Unknown        = 0,
    Window         = 1,
    Button         = 2,
    CheckBox       = 3,
    ComboBox       = 4,
    Edit           = 5,
    ListItem       = 6,
    List           = 7,
    Menu           = 8,
    MenuItem       = 9,
    ScrollBar      = 10,
    StatusBar      = 11,
    Tab            = 12,
    TabItem        = 13,
    Text           = 14,
    ToolBar        = 15,
    ToolTip        = 16,
    Tree           = 17,
    TreeItem       = 18,
    Custom         = 19,
    Document       = 20,
    Group          = 21,
    Image          = 22,
    Link           = 23,
    Pane           = 24,
    ProgressBar    = 25,
    RadioButton    = 26,
    Slider         = 27,
    Spinner        = 28,
    Table          = 29,
    TableItem      = 30,
};

// -------------------------------------------------------------------------
// IdSource — how the identifier was generated
// -------------------------------------------------------------------------
enum class IdSource : uint8_t {
    Accessibility = 0,   // from platform accessibility API
    OCRFallback   = 1,   // perceptual hash from OCR / pixel data
};

// -------------------------------------------------------------------------
// ElementId — persistent, cross-frame unique element identifier
// -------------------------------------------------------------------------
struct ElementId {
    uint64_t  process_id;           // OS process ID
    uint64_t  window_handle;        // HWND / NSWindow* / X11 Window (as uint64)
    int32_t   runtime_id[8];        // Accessibility runtime ID (platform-specific)
    uint32_t  automation_id_hash;   // Hash of AutomationId / AT-SPI name
    uint32_t  role;                 // Element role (ElementRole cast to uint32)
    uint32_t  checksum;             // CRC32 over preceding fields
    IdSource  source;               // Accessibility or OCRFallback
    uint8_t   _pad[3];              // padding to keep alignment

    bool operator==(const ElementId& o) const noexcept {
        return process_id         == o.process_id
            && window_handle      == o.window_handle
            && std::memcmp(runtime_id, o.runtime_id, sizeof(runtime_id)) == 0
            && automation_id_hash == o.automation_id_hash
            && role               == o.role
            && checksum           == o.checksum;
    }
    bool operator!=(const ElementId& o) const noexcept { return !(*this == o); }
};

// -------------------------------------------------------------------------
// ElementRect — axis-aligned bounding box in screen coordinates
// -------------------------------------------------------------------------
struct ElementRect {
    int32_t x, y, width, height;

    bool is_valid()    const noexcept { return width > 0 && height > 0; }
    bool is_empty()    const noexcept { return width <= 0 || height <= 0; }
    int32_t center_x() const noexcept { return x + width  / 2; }
    int32_t center_y() const noexcept { return y + height / 2; }
};

// -------------------------------------------------------------------------
// ElementInfo — element information returned to callers
// -------------------------------------------------------------------------
struct ElementInfo {
    ElementId   id;
    ElementRect rect;
    char        name[256];       // Accessible name (UTF-8)
    char        role_name[64];   // Human-readable role
    char        value[256];      // Current value / text content
    bool        enabled;
    bool        focused;
    uint32_t    child_count;
    IdSource    source;          // mirror of id.source for convenience
    uint8_t     _pad[3];
};
