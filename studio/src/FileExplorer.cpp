#include <windows.h>
#include <shobjidl.h>
#include <string>

namespace dalia::studio {

    /*
     * By default, all files are shown.
     * Each file type is separated by a semicolon ";".
     * Usage example:
     * std::string filePath = OpenFileExplorer("Image files", "*.png;*.jpg;*.gif;");
     */
    inline std::string OpenFileExplorer(const std::string& filterLabel = "", const std::string& filterFileTypes = "")
    {
        OPENFILENAME ofn;
        char fileName[MAX_PATH] = "";

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);

        const std::string defaultFilter = std::string("All Files (*.*)") + '\0' + std::string("*.*") + '\0';
        std::string filter = !filterLabel.empty() && !filterFileTypes.empty() ? filterLabel + " (" + filterFileTypes + ")" + '\0' + filterFileTypes + '\0': "";
        filter += defaultFilter;
        ofn.lpstrFilter = filter.c_str();

        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileName(&ofn))
        {
            return std::string(fileName);
        }

        return "";
    }

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
