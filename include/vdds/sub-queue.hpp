//  Copyright (c) 2021-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef VDDS_SUB_QUEUE_HPP
#define VDDS_SUB_QUEUE_HPP

#include <atomic>

#include "detail/spsc-queue.hpp"
#include "data.hpp"
#include "notifier.hpp"

namespace vdds {

/// Subscriber queue.
/// Simple single-read/write fifo based on vdds::spsc_queue.
/// This queue is allocated for each subscriber for each topic.
class sub_queue {
private:
	vdds::spsc_queue<data> _fifo;  ///> queue backend
	uint32_t        _drop_count;   ///> number of dropped push ops (queue was full)
	uint32_t        _push_count;   ///> number of push ops
	vdds::notifier* _notifier;     ///> notifier pointer

	const std::string  _name;      ///> queue name (subscriber name)
	const std::string  _data_type; ///> data type name
	size_t             _capacity;  ///> queue capacity

	std::mutex         _mutex;     ///> mutex used for multi-publisher push

	const char*        _trace_fmt; ///> trace format string

public:
	/// Create subscriber queue
	/// @param name queue name (subscriber name)
	/// @param tn topic name
	/// @param dt data type name
	/// @param size queue size
	/// @param n notifier pointer (null, cv, polling)
	explicit sub_queue(const std::string& name, const std::string& tn,
			const std::string& dt, size_t capacity = 16, notifier *n = nullptr);

	// No copies
	sub_queue(const sub_queue&) = delete;
	sub_queue& operator=(const sub_queue&) = delete;

	/// Get queue name and data type
	const std::string& name() const { return _name; }
	const std::string& data_type() const { return _data_type; }

	/// Get trace format string
	const char* trace_fmt() const { return _trace_fmt; }

	/// Get notifier pointer
	vdds::notifier* notifier() { return _notifier; }

	/// Get queue info (size, room, drop, etc)
	size_t capacity() const { return _capacity; }
	size_t size() const { return _capacity - _fifo.write_available(); }
	uint32_t push_count() const { return _push_count; }
	uint32_t drop_count() const { return _drop_count; }

	/// Kick queue.
	void kick(bool need_lock = false)
	{
		// CV notifier will do condition signal, noop otherwise
		if (_notifier)
			_notifier->notify();
	}

	/// Push data.
	/// Lockfree and nonblocking for single-publisher case.
	/// @param[in] d ref to data
	void push(const data &d, bool need_lock = false)
	{
		if (need_lock) _mutex.lock();

		// Update stats and push into fifo
		_push_count++;
		if (!_fifo.push(d)) _drop_count++;

		if (need_lock) _mutex.unlock();

		return kick(need_lock);
	}

	/// Pop data.
	/// Lockfree and nonblocking.
	/// @param[out] d ref to data
	/// @return false if queue is empty, true otherwise
	bool pop(data &d)
	{
		return _fifo.pop(d);
	}

	/// Shutdown queue
	void shutdown(std::chrono::nanoseconds t, bool need_lock = false)
	{
		if (_notifier)
			_notifier->shutdown(t);
	}
};

} // namespace vdds

#endif // VDDS_SUB_QUEUE_HPP
