#include <windows.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <string>

namespace dalia::studio {

    inline std::string GetOpenFileName()
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) return "";

        IFileOpenDialog *pFileOpen;

        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void **>(&pFileOpen));

        if (SUCCEEDED(hr))
        {
            hr = pFileOpen->Show(nullptr);

            if (SUCCEEDED(hr))
            {
                IShellItem *pItem;
                hr = pFileOpen->GetResult(&pItem);

                if (SUCCEEDED(hr))
                {
                    PWSTR filePath;

                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);

                    if (SUCCEEDED(hr))
                    {
                        std::wstring ws(filePath);
                        std::string result(ws.begin(), ws.end());

                        CoTaskMemFree(filePath);
                        pItem->Release();
                        pFileOpen->Release();
                        CoUninitialize();

                        return result;
                    }
                }
            }
            pFileOpen->Release();
        }

        CoUninitialize();
        return "";
    }
}
