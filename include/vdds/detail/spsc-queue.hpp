//  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//
//  SPDX-License-Identifier: BSD-3-Clause

// This implementation is heavily based on the original version of SPSCQueue
// https://github.com/rigtorp/SPSCQueue
// It's been adapted to match vDDS coding style and to remove unsafe (blocking, etc)
// functionality.

// Copyright (c) 2020 Erik Rigtorp <erik@rigtorp.se>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef VDDS_DETAIL_SPSC_QUEUE_HPP
#define VDDS_DETAIL_SPSC_QUEUE_HPP

#include <atomic>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <memory>      // std::allocator
#include <new>         // std::hardware_destructive_interference_size
#include <type_traits> // std::enable_if, std::is_*_constructible

#if defined(__has_cpp_attribute) && __has_cpp_attribute(nodiscard)
#define __spsc_nodiscard [[nodiscard]]
#else
#define __spsc_nodiscard
#endif

namespace vdds {

template <typename T, typename Allocator = std::allocator<T>> class spsc_queue {
public:
	explicit spsc_queue(const size_t capacity, const Allocator &allocator = Allocator())
			: _capacity(capacity), _allocator(allocator)
	{
		// The queue needs at least one element
		if (_capacity < 1) { _capacity = 1; }
		_capacity++; // Needs one slack element

		// Prevent overflowing size_t
		if (_capacity > SIZE_MAX - 2 * kPadding) {
			_capacity = SIZE_MAX - 2 * kPadding;
		}

		_slots = std::allocator_traits<Allocator>::allocate(_allocator, _capacity + 2 * kPadding);

		static_assert(alignof(spsc_queue<T>) == kCacheLineSize, "");
		static_assert(sizeof(spsc_queue<T>) >= 3 * kCacheLineSize, "");
		assert(reinterpret_cast<char *>(&_read_idx) - reinterpret_cast<char *>(&_write_idx) >= static_cast<std::ptrdiff_t>(kCacheLineSize));
	}

	~spsc_queue() {
		while (front()) { pop(); }
		std::allocator_traits<Allocator>::deallocate(_allocator, _slots, _capacity + 2 * kPadding);
	}

	// non-copyable and non-movable
	spsc_queue(const spsc_queue &) = delete;
	spsc_queue &operator=(const spsc_queue &) = delete;

	template <typename... Args>
	__spsc_nodiscard bool emplace(Args &&...args) noexcept(std::is_nothrow_constructible<T, Args &&...>::value)
	{
		static_assert(std::is_constructible<T, Args &&...>::value, "T must be constructible with Args&&...");

		auto const w_idx = _write_idx.load(std::memory_order_relaxed);

		auto next_w_idx = w_idx + 1;
		if (next_w_idx == _capacity) { next_w_idx = 0; }

		if (next_w_idx == _read_idx_cache) {
			_read_idx_cache = _read_idx.load(std::memory_order_acquire);
			if (next_w_idx == _read_idx_cache) { return false; } // full
		}

		// inplace construct/init and update write_idx
		new (&_slots[w_idx + kPadding]) T(std::forward<Args>(args)...);
		_write_idx.store(next_w_idx, std::memory_order_release);
		return true;
	}

	__spsc_nodiscard bool
	push(const T &v) noexcept(std::is_nothrow_copy_constructible<T>::value)
	{
		static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible");
		return emplace(v);
	}

	template <typename P, typename = typename std::enable_if< std::is_constructible<T, P &&>::value>::type>
	__spsc_nodiscard bool push(P &&v) noexcept(std::is_nothrow_constructible<T, P &&>::value)
	{
		return emplace(std::forward<P>(v));
	}

	__spsc_nodiscard T *front() noexcept
	{
		auto const r_idx = _read_idx.load(std::memory_order_relaxed);
		if (r_idx == _write_idx_cache) {
			_write_idx_cache = _write_idx.load(std::memory_order_acquire);
			if (r_idx == _write_idx_cache) { return nullptr; } // empty
		}
		return &_slots[r_idx + kPadding];
	}

	void pop() noexcept
	{
		static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");

		auto const r_idx = _read_idx.load(std::memory_order_relaxed);
		assert(_write_idx.load(std::memory_order_acquire) != r_idx && "pop() must be called only after front() has returned a non-nullptr");

		_slots[r_idx + kPadding].~T();

		auto next_r_idx = r_idx + 1;
		if (next_r_idx == _capacity) { next_r_idx = 0; }

		_read_idx.store(next_r_idx, std::memory_order_release);
	}

	__spsc_nodiscard bool pop(T &v) noexcept
	{
		T *n = front();
		if (!n) { return false; } // empty
		v = *n;
		pop();
		return true;
	}

	__spsc_nodiscard size_t size() const noexcept
	{
		std::ptrdiff_t diff = _write_idx.load(std::memory_order_acquire) - _read_idx.load(std::memory_order_acquire);
		if (diff < 0) { diff += _capacity; }
		return static_cast<size_t>(diff);
	}

	__spsc_nodiscard bool empty() const noexcept
	{
		return _write_idx.load(std::memory_order_acquire) == _read_idx.load(std::memory_order_acquire);
	}

	size_t capacity() const noexcept { return _capacity - 1; }

	__spsc_nodiscard size_t write_available() const noexcept
	{
		std::ptrdiff_t diff = _read_idx.load(std::memory_order_acquire) - _write_idx.load(std::memory_order_acquire);
		if (diff < 0) { diff += _capacity; }
		return static_cast<size_t>(diff);
	}

private:
#ifdef __cpp_lib_hardware_interference_size
	static constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
       	static constexpr size_t kCacheLineSize = 64;
#endif

	// Padding to avoid false sharing between _slots and adjacent allocations
	static constexpr size_t kPadding = (kCacheLineSize - 1) / sizeof(T) + 1;

private:
	size_t _capacity;
	T     *_slots;
#if defined(__has_cpp_attribute) && __has_cpp_attribute(no_unique_address)
	Allocator _allocator [[no_unique_address]];
#else
	Allocator _allocator;
#endif
	// Align to cache line size in order to avoid false sharing
	// _read_idx_cache and _write_idx_cache is used to reduce the amount of cache
	// coherency traffic
	alignas(kCacheLineSize) std::atomic<size_t> _write_idx = {0};
	alignas(kCacheLineSize) size_t _read_idx_cache = 0;
	alignas(kCacheLineSize) std::atomic<size_t> _read_idx  = {0};
	alignas(kCacheLineSize) size_t _write_idx_cache = 0;
};
} // namespace vdds

#endif // VDDS_DETAIL_SPSC_QUEUE_HPP
