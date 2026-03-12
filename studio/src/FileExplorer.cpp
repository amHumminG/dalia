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
    inline std::string OpenFileExplorer(const std::string& filterLabel = "", const std::string& filterFileTypes = "") {
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

        if (GetOpenFileName(&ofn)) {
            return std::string(fileName);
        }

        return "";
    }
}
