#pragma once
#include <windows.h>
namespace beer {
inline HICON app_icon(bool small = false) {
    return reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON,
                                              GetSystemMetrics(small ? SM_CXSMICON : SM_CXICON),
                                              GetSystemMetrics(small ? SM_CYSMICON : SM_CYICON), LR_SHARED));
}
} // namespace beer
