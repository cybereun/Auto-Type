// COM entry points and registration as a Korean keyboard input method (TSF text service).
#include <new>
#include <string>

#include "TextService.h"

namespace autotype {

HINSTANCE g_module = nullptr;
LONG g_refs = 0;

void dllAddRef() { InterlockedIncrement(&g_refs); }
void dllRelease() { InterlockedDecrement(&g_refs); }

const Model& sharedModel() {
    static Model model = [] {
        Model m;
        HRSRC info = FindResourceW(g_module, MAKEINTRESOURCEW(kModelResource), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
        HGLOBAL handle = info ? LoadResource(g_module, info) : nullptr;
        const void* data = handle ? LockResource(handle) : nullptr;
        if (data) m.load(static_cast<const uint8_t*>(data), SizeofResource(g_module, info));
        return m;
    }();
    return model;
}

namespace {

class ClassFactory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** out) override {
        if (!out) return E_INVALIDARG;
        *out = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *out = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { dllAddRef(); return 2; }
    STDMETHODIMP_(ULONG) Release() override { dllRelease(); return 1; }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** out) override {
        if (!out) return E_INVALIDARG;
        *out = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* service = new (std::nothrow) TextService();
        if (!service) return E_OUTOFMEMORY;
        HRESULT hr = service->QueryInterface(riid, out);
        service->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        lock ? dllAddRef() : dllRelease();
        return S_OK;
    }
};

ClassFactory g_factory;

std::wstring clsidKey() {
    wchar_t guid[64] = {};
    StringFromGUID2(kClsid, guid, 64);
    return std::wstring(L"CLSID\\") + guid;
}

bool setValue(HKEY root, const std::wstring& path, const wchar_t* name, const std::wstring& value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LONG result = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                                 static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

const GUID* const kCategories[] = {
    &GUID_TFCAT_TIP_KEYBOARD,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
};

}  // namespace
}  // namespace autotype

using namespace autotype;

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() { return g_refs <= 0 ? S_OK : S_FALSE; }

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** out) {
    if (!out) return E_INVALIDARG;
    *out = nullptr;
    if (!IsEqualCLSID(clsid, kClsid)) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, out);
}

STDAPI DllRegisterServer() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(g_module, path, MAX_PATH);

    // 1. COM server
    const std::wstring key = clsidKey();
    if (!setValue(HKEY_CLASSES_ROOT, key, nullptr, kDescription) ||
        !setValue(HKEY_CLASSES_ROOT, key + L"\\InprocServer32", nullptr, path) ||
        !setValue(HKEY_CLASSES_ROOT, key + L"\\InprocServer32", L"ThreadingModel", L"Apartment"))
        return E_FAIL;

    // 2. Input method profile under Korean, next to the Microsoft IME
    ITfInputProcessorProfileMgr* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profiles));
    if (FAILED(hr)) return hr;
    hr = profiles->RegisterProfile(kClsid, kLangId, kProfile, kDescription, static_cast<ULONG>(wcslen(kDescription)),
                                   path, static_cast<ULONG>(wcslen(path)), 0, nullptr, 0, TRUE, 0);
    profiles->Release();
    if (FAILED(hr)) return hr;

    // 3. Categories: a keyboard TIP that also works in Store apps and the system tray
    ITfCategoryMgr* categories = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                          reinterpret_cast<void**>(&categories));
    if (FAILED(hr)) return hr;
    for (const GUID* category : kCategories) categories->RegisterCategory(kClsid, *category, kClsid);
    categories->Release();
    return S_OK;
}

STDAPI DllUnregisterServer() {
    ITfInputProcessorProfileMgr* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profiles)))) {
        profiles->UnregisterProfile(kClsid, kLangId, kProfile, 0);
        profiles->Release();
    }
    ITfCategoryMgr* categories = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(&categories)))) {
        for (const GUID* category : kCategories) categories->UnregisterCategory(kClsid, *category, kClsid);
        categories->Release();
    }
    RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidKey().c_str());
    return S_OK;
}
