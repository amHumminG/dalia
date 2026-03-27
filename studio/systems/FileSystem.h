#pragma once
#include <string>

namespace dalia::studio {
    class FileSystem {
        public:
        FileSystem();
        ~FileSystem();

        private:
        std::string m_path;
    };
}

