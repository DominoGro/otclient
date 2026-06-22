/*
 * Copyright (c) 2010-2026 OTClient <https://github.com/edubart/otclient>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "../pch.h"

namespace stdext
{
// Vector with inline storage for N elements — avoids heap allocation for small sizes.
    // Falls back to heap when size exceeds N. API matches std::vector for used operations.
    template<typename T, uint8_t N>
    class SmallVector
    {
        static_assert(N > 0);

        alignas(T) std::byte m_inline[sizeof(T) * N];
        T* m_heap{ nullptr };
        uint8_t m_size{ 0 };
        uint8_t m_capacity{ N };

        T* buf() noexcept { return m_heap ? m_heap : reinterpret_cast<T*>(m_inline); }
        const T* buf() const noexcept { return m_heap ? m_heap : reinterpret_cast<const T*>(m_inline); }

        void grow() {
            const uint8_t newCap = static_cast<uint8_t>(m_capacity * 2);
            T* newHeap = static_cast<T*>(::operator new(sizeof(T) * newCap));
            T* old = buf();
            for (uint8_t i = 0; i < m_size; ++i) {
                new(newHeap + i) T(std::move(old[i]));
                old[i].~T();
            }
            if (m_heap) ::operator delete(m_heap);
            m_heap = newHeap;
            m_capacity = newCap;
        }

    public:
        using value_type = T;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        SmallVector() = default;
        ~SmallVector() { clear(); if (m_heap) ::operator delete(m_heap); }

        SmallVector(const SmallVector&) = delete;
        SmallVector& operator=(const SmallVector&) = delete;

        SmallVector(SmallVector&& o) noexcept : m_size(o.m_size), m_capacity(o.m_capacity) {
            if (o.m_heap) {
                m_heap = o.m_heap;
                o.m_heap = nullptr;
            } else {
                T* src = reinterpret_cast<T*>(o.m_inline);
                T* dst = reinterpret_cast<T*>(m_inline);
                for (uint8_t i = 0; i < m_size; ++i) {
                    new(dst + i) T(std::move(src[i]));
                    src[i].~T();
                }
            }
            o.m_size = 0;
            o.m_capacity = N;
        }

        SmallVector& operator=(SmallVector&& o) noexcept {
            if (this != &o) { this->~SmallVector(); new(this) SmallVector(std::move(o)); }
            return *this;
        }

        uint8_t size() const noexcept { return m_size; }
        bool empty() const noexcept { return m_size == 0; }

        iterator begin() noexcept { return buf(); }
        iterator end() noexcept { return buf() + m_size; }
        const_iterator begin() const noexcept { return buf(); }
        const_iterator end() const noexcept { return buf() + m_size; }
        reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

        T& operator[](size_t i) noexcept { return buf()[i]; }
        const T& operator[](size_t i) const noexcept { return buf()[i]; }
        T& front() noexcept { return buf()[0]; }
        T& back() noexcept { return buf()[m_size - 1]; }
        const T& front() const noexcept { return buf()[0]; }
        const T& back() const noexcept { return buf()[m_size - 1]; }

        template<typename... Args>
        T& emplace_back(Args&&... args) {
            if (m_size == m_capacity) grow();
            T* p = buf() + m_size++;
            new(p) T(std::forward<Args>(args)...);
            return *p;
        }

        void push_back(const T& v) { emplace_back(v); }
        void push_back(T&& v) { emplace_back(std::move(v)); }

        iterator erase(iterator pos) {
            std::move(pos + 1, end(), pos);
            buf()[--m_size].~T();
            return pos;
        }

        iterator insert(iterator pos, const T& v) {
            const size_t idx = pos - buf();
            if (m_size == m_capacity) grow();
            T* p = buf();
            // shift elements right
            if (m_size > idx) {
                new(p + m_size) T(std::move(p[m_size - 1]));
                std::move_backward(p + idx, p + m_size - 1, p + m_size);
                p[idx].~T();
            }
            new(p + idx) T(v);
            ++m_size;
            return p + idx;
        }

        void clear() noexcept {
            T* p = buf();
            for (uint8_t i = 0; i < m_size; ++i) p[i].~T();
            m_size = 0;
        }
    };
    template <class K, class V,
        class Hash = phmap::priv::hash_default_hash<K>,
        class Eq = phmap::priv::hash_default_eq<K>,
        class Alloc = phmap::priv::Allocator<phmap::priv::Pair<const K, V>>>
    using map = phmap::flat_hash_map<K, V, Hash, Eq, Alloc>;

    template <class T,
        class Hash = phmap::priv::hash_default_hash<T>,
        class Eq = phmap::priv::hash_default_eq<T>,
        class Alloc = phmap::priv::Allocator<T>>
        using set = phmap::flat_hash_set<T, Hash, Eq, Alloc>;

    template<typename T>
    concept OnlyEnum = std::is_enum_v<T>;

    template<OnlyEnum Key>
    class dynamic_storage
    {
    public:
        template<typename T>
        void set(const Key& key, const T& value) { m_data[key] = value; }

        bool remove(const Key& k) { return m_data.erase(k) > 0; }

        template<typename T> T get(const Key& k, const T defaultValue = T()) const
        {
            auto it = m_data.find(k);
            if (it == m_data.end()) {
                return defaultValue;
            }

            try {
                return std::any_cast<T>(it->second);
            } catch (const std::exception&) {
                return defaultValue;
            }
        }

        bool has(const Key& k) const { return m_data.contains(k); }

        size_t size() const { return m_data.count(); }

        void clear() { m_data.clear(); }

    private:
        map<Key, std::any> m_data;
    };
}
