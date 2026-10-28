#pragma once

#include <windows.h>
#include <msctf.h>

#ifdef __MINGW32__
// MinGW's msctf.h predates Windows 8; these match the Windows SDK definitions.
#define TF_CLIENTID_NULL 0
inline constexpr GUID GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT = {0x13a016df, 0x560b, 0x46cd, {0x94, 0x7a, 0x4c, 0x3a, 0xf1, 0xe0, 0xe3, 0x5d}};
inline constexpr GUID GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT = {0x25504fb4, 0x7bab, 0x4bc1, {0x9c, 0x69, 0xcf, 0x81, 0x89, 0x0f, 0x0e, 0xf5}};
inline constexpr IID IID_ITfTextInputProcessorEx = {0x6e4e2102, 0xf9cd, 0x433d, {0xb4, 0x96, 0x30, 0x3c, 0xe0, 0x3a, 0x65, 0x07}};
struct ITfTextInputProcessorEx : public ITfTextInputProcessor {
    virtual HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD dwFlags) = 0;
};
#endif

#include "Engine.h"

namespace autotype {

// {103733A2-F0D5-4DF0-B88A-D6281528AC32}
inline constexpr CLSID kClsid = {0x103733a2, 0xf0d5, 0x4df0, {0xb8, 0x8a, 0xd6, 0x28, 0x15, 0x28, 0xac, 0x32}};
// {45F819E1-0A0C-47B2-BF9C-0A94A5A4A4AB}
inline constexpr GUID kProfile = {0x45f819e1, 0x0a0c, 0x47b2, {0xbf, 0x9c, 0x0a, 0x94, 0xa5, 0xa4, 0xa4, 0xab}};
inline constexpr LANGID kLangId = MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN);
inline constexpr wchar_t kDescription[] = L"Auto-Type (한영 자동)";
inline constexpr int kModelResource = 101;

extern HINSTANCE g_module;
extern LONG g_refs;
const Model& sharedModel();

void dllAddRef();
void dllRelease();

}  // namespace autotype
