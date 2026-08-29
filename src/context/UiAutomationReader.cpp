#include "context/UiAutomationReader.h"

#include <initguid.h>
#include <objbase.h>
#include <UIAutomationClient.h>
#include <wrl/client.h>

namespace sveta::context {

namespace {

constexpr size_t kMaxTextLength = 800;

// Each COM-using file in this project scopes its own CoInitializeEx call
// (see rendering/Sprite.cpp) rather than assuming some other file already
// did it on this thread — this one's called from a background worker
// thread, not the UI thread that owns the app-lifetime COM init in main.cpp.
class ComScope {
public:
    ComScope() : initialized_(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {}
    ~ComScope() {
        if (initialized_) {
            CoUninitialize();
        }
    }
    ComScope(const ComScope&) = delete;
    ComScope& operator=(const ComScope&) = delete;

private:
    bool initialized_;
};

} // namespace

std::optional<std::wstring> UiAutomationReader::ExtractShallowText(HWND hwnd, int maxElements) {
    if (!hwnd) {
        return std::nullopt;
    }
    ComScope comScope;

    Microsoft::WRL::ComPtr<IUIAutomation> automation;
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
    if (FAILED(hr)) {
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IUIAutomationElement> root;
    hr = automation->ElementFromHandle(hwnd, &root);
    if (FAILED(hr) || !root) {
        return std::nullopt;
    }

    std::wstring result;

    BSTR rootName = nullptr;
    if (SUCCEEDED(root->get_CurrentName(&rootName)) && rootName && SysStringLen(rootName) > 0) {
        result.assign(rootName, SysStringLen(rootName));
    }
    if (rootName) {
        SysFreeString(rootName);
    }

    Microsoft::WRL::ComPtr<IUIAutomationCondition> trueCondition;
    automation->CreateTrueCondition(&trueCondition);

    Microsoft::WRL::ComPtr<IUIAutomationElementArray> children;
    hr = root->FindAll(TreeScope_Children, trueCondition.Get(), &children);
    if (SUCCEEDED(hr) && children) {
        int count = 0;
        children->get_Length(&count);

        int collected = 0;
        for (int i = 0; i < count && collected < maxElements && result.size() <= kMaxTextLength; ++i) {
            Microsoft::WRL::ComPtr<IUIAutomationElement> element;
            if (FAILED(children->GetElement(i, &element)) || !element) {
                continue;
            }

            BOOL offscreen = FALSE;
            element->get_CurrentIsOffscreen(&offscreen);
            if (offscreen) {
                continue;
            }

            BSTR name = nullptr;
            if (SUCCEEDED(element->get_CurrentName(&name)) && name && SysStringLen(name) > 0) {
                if (!result.empty()) {
                    result += L" | ";
                }
                result.append(name, SysStringLen(name));
                ++collected;
            }
            if (name) {
                SysFreeString(name);
            }
        }
    }

    if (result.empty()) {
        return std::nullopt;
    }
    if (result.size() > kMaxTextLength) {
        result.resize(kMaxTextLength);
    }
    return result;
}

} // namespace sveta::context
