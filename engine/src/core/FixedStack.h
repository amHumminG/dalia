#pragma once
#include <memory>

namespace dalia {

    template<typename T>
    class FixedStack {
    public:
        FixedStack(size_t capacity) {
            m_capacity = capacity;
            m_buffer = std::make_unique<T[]>(m_capacity);
        }

        ~FixedStack() = default;

        FixedStack(const FixedStack&) = delete;
        FixedStack& operator=(const FixedStack&) = delete;

        bool Push(const T& item) {
            if (IsFull()) return false;
            m_buffer[m_size] = item;
            m_size++;
            return true;
        }

        bool Pop(T& item) {
            if (IsEmpty()) return false;
            m_size--;
            item = m_buffer[m_size];
            return true;
        }

        bool IsEmpty() const { return m_size == 0; }
        bool IsFull() const { return m_size >= m_capacity; }
        size_t Size() const { return m_size; }
        size_t Capacity() const { return m_capacity; }

    private:
        std::unique_ptr<T[]> m_buffer;
        size_t m_capacity = 0;
        size_t m_size = 0;
    };
}