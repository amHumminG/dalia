#pragma once

namespace dalia::studio {

    static constexpr uint32_t NULL_ID = 0;

    struct SelectionContext {
        uint32_t selectedId = NULL_ID;

        void Select(uint32_t id) { selectedId = id; }
        bool IsSelected(uint32_t id) const { return id != NULL_ID && selectedId == id; }
        bool HasSelection() { return selectedId != NULL_ID; }
        void Clear() { selectedId = NULL_ID; }
    };
}