#pragma once
#include <vector>

namespace dalia::studio {

    struct SelectionContext {
        uint32_t selectedId = 0;

        bool IsSelected(uint32_t id) const { return selectedId == id; }
        void Select(uint32_t id) { selectedId = id; }
        void Clear() { selectedId = 0; }
    };
}