#ifdef _WIN32

#include "../include/platform_impl.hpp"
#include <cstring>
#include <cstdio>

// UIAutomation and COM headers
#include <windows.h>
#include <uiautomation.h>
#include <combaseapi.h>

// -------------------------------------------------------------------------
// Helper: map UIA ControlType to ElementRole
// -------------------------------------------------------------------------
static ElementRole uia_control_type_to_role(CONTROLTYPEID ct) {
    switch (ct) {
        case UIA_ButtonControlTypeId:       return ElementRole::Button;
        case UIA_CheckBoxControlTypeId:     return ElementRole::CheckBox;
        case UIA_ComboBoxControlTypeId:     return ElementRole::ComboBox;
        case UIA_EditControlTypeId:         return ElementRole::Edit;
        case UIA_ListItemControlTypeId:     return ElementRole::ListItem;
        case UIA_ListControlTypeId:         return ElementRole::List;
        case UIA_MenuControlTypeId:         return ElementRole::Menu;
        case UIA_MenuItemControlTypeId:     return ElementRole::MenuItem;
        case UIA_ScrollBarControlTypeId:    return ElementRole::ScrollBar;
        case UIA_StatusBarControlTypeId:    return ElementRole::StatusBar;
        case UIA_TabControlTypeId:          return ElementRole::Tab;
        case UIA_TabItemControlTypeId:      return ElementRole::TabItem;
        case UIA_TextControlTypeId:         return ElementRole::Text;
        case UIA_ToolBarControlTypeId:      return ElementRole::ToolBar;
        case UIA_ToolTipControlTypeId:      return ElementRole::ToolTip;
        case UIA_TreeControlTypeId:         return ElementRole::Tree;
        case UIA_TreeItemControlTypeId:     return ElementRole::TreeItem;
        case UIA_DocumentControlTypeId:     return ElementRole::Document;
        case UIA_GroupControlTypeId:        return ElementRole::Group;
        case UIA_ImageControlTypeId:        return ElementRole::Image;
        case UIA_HyperlinkControlTypeId:    return ElementRole::Link;
        case UIA_PaneControlTypeId:         return ElementRole::Pane;
        case UIA_ProgressBarControlTypeId:  return ElementRole::ProgressBar;
        case UIA_RadioButtonControlTypeId:  return ElementRole::RadioButton;
        case UIA_SliderControlTypeId:       return ElementRole::Slider;
        case UIA_SpinnerControlTypeId:      return ElementRole::Spinner;
        case UIA_TableControlTypeId:        return ElementRole::Table;
        case UIA_DataItemControlTypeId:     return ElementRole::TableItem;
        case UIA_WindowControlTypeId:       return ElementRole::Window;
        default:                            return ElementRole::Custom;
    }
}

// -------------------------------------------------------------------------
// Helper: convert BSTR to UTF-8 char buffer
// -------------------------------------------------------------------------
static void bstr_to_utf8(BSTR bstr, char* buf, size_t buf_size) {
    if (!bstr || !buf || buf_size == 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, buf,
                                  static_cast<int>(buf_size), nullptr, nullptr);
    if (len <= 0) buf[0] = '\0';
}

// -------------------------------------------------------------------------
// WindowsBackend
// -------------------------------------------------------------------------
class WindowsBackend : public PlatformBackend {
public:
    WindowsBackend() : m_pAutomation(nullptr) {}

    ~WindowsBackend() override {
        if (m_pAutomation) {
            m_pAutomation->Release();
            m_pAutomation = nullptr;
        }
        CoUninitialize();
    }

    bool init() override {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

        hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IUIAutomation,
                              reinterpret_cast<void**>(&m_pAutomation));
        return SUCCEEDED(hr) && m_pAutomation != nullptr;
    }

    std::vector<ElementInfo> enumerate_elements(uint32_t max_elements) override {
        std::vector<ElementInfo> results;
        if (!m_pAutomation) return results;

        IUIAutomationElement* pRoot = nullptr;
        HRESULT hr = m_pAutomation->GetRootElement(&pRoot);
        if (FAILED(hr) || !pRoot) return results;

        IUIAutomationCondition* pTrue = nullptr;
        hr = m_pAutomation->CreateTrueCondition(&pTrue);
        if (FAILED(hr) || !pTrue) {
            pRoot->Release();
            return results;
        }

        IUIAutomationElementArray* pElements = nullptr;
        hr = pRoot->FindAll(TreeScope_Descendants, pTrue, &pElements);
        pTrue->Release();
        pRoot->Release();

        if (FAILED(hr) || !pElements) return results;

        int count = 0;
        pElements->get_Length(&count);

        for (int i = 0; i < count; ++i) {
            if (max_elements > 0 && results.size() >= max_elements) break;

            IUIAutomationElement* pEl = nullptr;
            if (FAILED(pElements->GetElement(i, &pEl)) || !pEl) continue;

            ElementInfo info;
            std::memset(&info, 0, sizeof(info));

            // Name
            BSTR bstrName = nullptr;
            if (SUCCEEDED(pEl->get_CurrentName(&bstrName)) && bstrName) {
                bstr_to_utf8(bstrName, info.name, sizeof(info.name));
                SysFreeString(bstrName);
            }

            // Control type / role
            CONTROLTYPEID ctId = UIA_CustomControlTypeId;
            pEl->get_CurrentControlType(&ctId);
            ElementRole role = uia_control_type_to_role(ctId);
            info.id.role = static_cast<uint32_t>(role);
            snprintf(info.role_name, sizeof(info.role_name), "uia_%u",
                     static_cast<unsigned>(ctId));

            // Bounding rect
            RECT rect = {};
            if (SUCCEEDED(pEl->get_CurrentBoundingRectangle(&rect))) {
                info.rect.x      = rect.left;
                info.rect.y      = rect.top;
                info.rect.width  = rect.right  - rect.left;
                info.rect.height = rect.bottom - rect.top;
            }

            // Enabled / focused
            BOOL bEnabled = FALSE, bFocused = FALSE;
            pEl->get_CurrentIsEnabled(&bEnabled);
            pEl->get_CurrentHasKeyboardFocus(&bFocused);
            info.enabled = (bEnabled == TRUE);
            info.focused = (bFocused == TRUE);

            // Process / window handle
            DWORD pid = 0;
            pEl->get_CurrentProcessId(reinterpret_cast<int*>(&pid));
            info.id.process_id = pid;

            HWND hwnd = nullptr;
            pEl->get_CurrentNativeWindowHandle(reinterpret_cast<UIA_HWND*>(&hwnd));
            info.id.window_handle = reinterpret_cast<uint64_t>(hwnd);

            // Runtime ID
            SAFEARRAY* pRid = nullptr;
            if (SUCCEEDED(pEl->GetRuntimeId(&pRid)) && pRid) {
                LONG lb = 0, ub = 0;
                SafeArrayGetLBound(pRid, 1, &lb);
                SafeArrayGetUBound(pRid, 1, &ub);
                int ridLen = static_cast<int>(ub - lb + 1);
                for (int r = 0; r < ridLen && r < 8; ++r) {
                    LONG idx = lb + r;
                    SafeArrayGetElement(pRid, &idx, &info.id.runtime_id[r]);
                }
                SafeArrayDestroy(pRid);
            }

            // Value
            IUIAutomationValuePattern* pValue = nullptr;
            if (SUCCEEDED(pEl->GetCurrentPatternAs(UIA_ValuePatternId,
                    IID_IUIAutomationValuePattern,
                    reinterpret_cast<void**>(&pValue))) && pValue) {
                BSTR bstrVal = nullptr;
                if (SUCCEEDED(pValue->get_CurrentValue(&bstrVal)) && bstrVal) {
                    bstr_to_utf8(bstrVal, info.value, sizeof(info.value));
                    SysFreeString(bstrVal);
                }
                pValue->Release();
            }

            info.id.source = IdSource::Accessibility;
            info.source    = IdSource::Accessibility;

            pEl->Release();
            results.push_back(info);
        }

        pElements->Release();
        return results;
    }

    std::unique_ptr<ElementInfo> get_element(const ElementId& id) override {
        // Simplified: re-enumerate and find matching ID
        auto elements = enumerate_elements(0);
        for (auto& el : elements) {
            if (el.id == id) {
                return std::make_unique<ElementInfo>(el);
            }
        }
        return nullptr;
    }

    int click_element(const ElementId& id) override {
        if (!m_pAutomation) return -1;

        auto info = get_element(id);
        if (!info) return -1;

        // Try to find the element via runtime ID
        IUIAutomationElement* pRoot = nullptr;
        if (FAILED(m_pAutomation->GetRootElement(&pRoot)) || !pRoot) return -1;

        IUIAutomationCondition* pTrue = nullptr;
        m_pAutomation->CreateTrueCondition(&pTrue);
        IUIAutomationElementArray* pElements = nullptr;
        pRoot->FindAll(TreeScope_Descendants, pTrue, &pElements);
        pTrue->Release();
        pRoot->Release();

        if (!pElements) return -1;

        int count = 0;
        pElements->get_Length(&count);
        int result = -1;

        for (int i = 0; i < count && result != 0; ++i) {
            IUIAutomationElement* pEl = nullptr;
            if (FAILED(pElements->GetElement(i, &pEl)) || !pEl) continue;

            DWORD pid = 0;
            pEl->get_CurrentProcessId(reinterpret_cast<int*>(&pid));
            HWND hwnd = nullptr;
            pEl->get_CurrentNativeWindowHandle(reinterpret_cast<UIA_HWND*>(&hwnd));

            if (pid == id.process_id &&
                reinterpret_cast<uint64_t>(hwnd) == id.window_handle)
            {
                // Try IUIAutomationInvokePattern
                IUIAutomationInvokePattern* pInvoke = nullptr;
                if (SUCCEEDED(pEl->GetCurrentPatternAs(UIA_InvokePatternId,
                        IID_IUIAutomationInvokePattern,
                        reinterpret_cast<void**>(&pInvoke))) && pInvoke) {
                    if (SUCCEEDED(pInvoke->Invoke())) result = 0;
                    pInvoke->Release();
                }

                // Fallback: simulate click via SendMessage
                if (result != 0 && hwnd) {
                    RECT r = {};
                    pEl->get_CurrentBoundingRectangle(&r);
                    LPARAM lp = MAKELPARAM((r.left + r.right) / 2 - r.left,
                                          (r.top + r.bottom) / 2 - r.top);
                    SendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
                    SendMessage(hwnd, WM_LBUTTONUP, 0, lp);
                    result = 0;
                }
            }
            pEl->Release();
        }

        pElements->Release();
        return result;
    }

    int type_text(const ElementId& id, const std::string& text) override {
        if (!m_pAutomation) return -1;

        IUIAutomationElement* pRoot = nullptr;
        if (FAILED(m_pAutomation->GetRootElement(&pRoot)) || !pRoot) return -1;

        IUIAutomationCondition* pTrue = nullptr;
        m_pAutomation->CreateTrueCondition(&pTrue);
        IUIAutomationElementArray* pElements = nullptr;
        pRoot->FindAll(TreeScope_Descendants, pTrue, &pElements);
        pTrue->Release();
        pRoot->Release();

        if (!pElements) return -1;

        int count = 0;
        pElements->get_Length(&count);
        int result = -1;

        for (int i = 0; i < count && result != 0; ++i) {
            IUIAutomationElement* pEl = nullptr;
            if (FAILED(pElements->GetElement(i, &pEl)) || !pEl) continue;

            DWORD pid = 0;
            pEl->get_CurrentProcessId(reinterpret_cast<int*>(&pid));
            HWND hwnd = nullptr;
            pEl->get_CurrentNativeWindowHandle(reinterpret_cast<UIA_HWND*>(&hwnd));

            if (pid == id.process_id &&
                reinterpret_cast<uint64_t>(hwnd) == id.window_handle)
            {
                // Try IUIAutomationValuePattern::SetValue
                IUIAutomationValuePattern* pValue = nullptr;
                if (SUCCEEDED(pEl->GetCurrentPatternAs(UIA_ValuePatternId,
                        IID_IUIAutomationValuePattern,
                        reinterpret_cast<void**>(&pValue))) && pValue) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0,
                        text.c_str(), -1, nullptr, 0);
                    if (wlen > 0) {
                        std::vector<wchar_t> wtext(wlen);
                        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1,
                                            wtext.data(), wlen);
                        BSTR bstr = SysAllocString(wtext.data());
                        if (bstr) {
                            if (SUCCEEDED(pValue->SetValue(bstr))) result = 0;
                            SysFreeString(bstr);
                        }
                    }
                    pValue->Release();
                }

                // Fallback: WM_SETTEXT
                if (result != 0 && hwnd) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0,
                        text.c_str(), -1, nullptr, 0);
                    if (wlen > 0) {
                        std::vector<wchar_t> wtext(wlen);
                        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1,
                                            wtext.data(), wlen);
                        SendMessage(hwnd, WM_SETTEXT, 0,
                                    reinterpret_cast<LPARAM>(wtext.data()));
                        result = 0;
                    }
                }
            }
            pEl->Release();
        }

        pElements->Release();
        return result;
    }

    int scroll_element(const ElementId& id, int direction, int amount) override {
        if (!m_pAutomation) return -1;

        auto info = get_element(id);
        if (!info) return -1;

        HWND hwnd = reinterpret_cast<HWND>(id.window_handle);
        if (!hwnd) return -1;

        int clicks = direction > 0 ? amount : -amount;
        int cy = info->rect.center_y();
        int cx = info->rect.center_x();
        LPARAM lp = MAKELPARAM(cx, cy);
        WPARAM wp = MAKEWPARAM(0, static_cast<short>(clicks * WHEEL_DELTA));
        SendMessage(hwnd, WM_MOUSEWHEEL, wp, lp);
        return 0;
    }

    const char* name() const noexcept override { return "windows_uiautomation"; }

private:
    IUIAutomation* m_pAutomation;
};

// -------------------------------------------------------------------------
// Factory
// -------------------------------------------------------------------------
std::unique_ptr<PlatformBackend> create_platform_backend() {
    auto b = std::make_unique<WindowsBackend>();
    if (!b->init()) {
        return create_ocr_fallback_backend();
    }
    return b;
}

#endif  // _WIN32
