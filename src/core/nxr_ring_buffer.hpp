#pragma once

#include <vector>
#include <concepts>
#include <utility>

template <typename T>
class RingBuffer {
private:
    std::vector<T> m_buffer;
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_maxSize = 0;
    bool m_isFull = false;

    size_t next_index(size_t index) const noexcept {
        size_t next = index + 1;
        return (next == m_maxSize) ? 0 : next;
    }

public:
    RingBuffer() = default;
    
    explicit RingBuffer(size_t size) {
        init(size);
    }

    void init(size_t size) {
        m_maxSize = size;
        m_buffer.resize(size);
        clear();
    }

    template <typename Item, typename OnOverwrite = void(*)(T&)>
    void push(Item&& item, OnOverwrite&& onOverwrite = nullptr) {
        if (m_maxSize == 0) return;

        if (m_isFull) {
            if constexpr (!std::is_same_v<std::decay_t<OnOverwrite>, std::nullptr_t>) {
                if (onOverwrite) {
                    onOverwrite(m_buffer[m_head]);
                }
            }
            m_tail = next_index(m_tail);
        }

        m_buffer[m_head] = std::forward<Item>(item);
        m_head = next_index(m_head);

        if (m_head == m_tail) {
            m_isFull = true;
        }
    }

    bool pop_back(T& outItem) {
        if (empty()) return false;

        m_head = (m_head == 0) ? (m_maxSize - 1) : (m_head - 1);
        outItem = std::move(m_buffer[m_head]);
        m_isFull = false;
        return true;
    }

    bool back(T& outItem) const {
        if (empty()) return false;
        size_t lastIdx = (m_head == 0) ? (m_maxSize - 1) : (m_head - 1);
        outItem = m_buffer[lastIdx];
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return (m_head == m_tail && !m_isFull);
    }

    [[nodiscard]] bool full() const noexcept {
        return m_isFull;
    }

    [[nodiscard]] size_t size() const noexcept {
        if (m_isFull) return m_maxSize;
        if (m_head >= m_tail) return m_head - m_tail;
        return m_maxSize + m_head - m_tail;
    }

    template <typename OnClear = void(*)(T&)>
    void clear(OnClear&& onClear = nullptr) {
        if constexpr (!std::is_same_v<std::decay_t<OnClear>, std::nullptr_t>) {
            if (onClear) {
                for_each(onClear);
            }
        }
        m_head = 0;
        m_tail = 0;
        m_isFull = false;
    }

    template <typename F>
        requires std::invocable<F, T&>
    void for_each(F&& f) {
        size_t count = size();
        size_t current = m_tail;
        for (size_t i = 0; i < count; ++i) {
            f(m_buffer[current]);
            current = next_index(current);
        }
    }

    template <typename F>
        requires std::invocable<F, const T&>
    void for_each(F&& f) const {
        size_t count = size();
        size_t current = m_tail;
        for (size_t i = 0; i < count; ++i) {
            f(m_buffer[current]);
            current = next_index(current);
        }
    }
};