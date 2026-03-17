#include <windows.h>
#include <shobjidl.h>
#include <string>
#include <vector>

namespace dalia::studio {

    /*
     * By default, all files are shown.
     * Each file type is separated by a semicolon ";".
     * Usage example:
     * std::string filePath = OpenFileExplorer("Image files", "*.png;*.jpg;*.gif;");
     */
    inline std::vector<std::string> OpenFileExplorer(const std::string& filterLabel = "", const std::string& filterFileTypes = "") {
        OPENFILENAME ofn;
        char fileName[2048] = ""; // Will contain path to directory of selected files and names of all selected files

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);

        const std::string defaultFilter = std::string("All Files (*.*)") + '\0' + std::string("*.*") + '\0';
        std::string filter = !filterLabel.empty() && !filterFileTypes.empty() ? filterLabel + " (" + filterFileTypes + ")" + '\0' + filterFileTypes + '\0': "";
        filter += defaultFilter;
        ofn.lpstrFilter = filter.c_str();

        ofn.lpstrFile = fileName;
        ofn.nMaxFile = sizeof(fileName);
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT;

        if (GetOpenFileName(&ofn)) {
            std::vector<std::string> filePaths;
            std::string directory = fileName;
            char* currentFile = fileName + directory.length() + 1;

            while (*currentFile) {
                filePaths.push_back(directory + "\\" + currentFile);
                currentFile += strlen(currentFile) + 1;
            }

            return filePaths;
        }

        return std::vector<std::string>();
    }
}
