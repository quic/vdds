//  Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef VDDS_SUB_HPP
#define VDDS_SUB_HPP

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include <hogl/area.hpp>
#include <hogl/post.hpp>

#include "domain.hpp"
#include "topic.hpp"

namespace vdds {

/// Typesafe subscriber.
/// This is the main interface for subscribing to topics.
/// It takes care of all ops: create topic, subscribe and pop data.
/// @param T data type
template<typename T>
class sub {
private:
	vdds::sub_queue* _queue; ///< subscriber queue pointer
	vdds::topic*     _topic; ///< topic pointer

public:
	/// Create subscriber.
	/// Creates topic and subscribes to it.
	/// @param[in] vd reference to the domain
	/// @param[in] name subscriber name 
	/// @param[in] topic_name topic name name 
	/// @param[in] qsize size of the queue
	/// @param[in] ntfr notifier pointer (null, cv, poll) 
	explicit sub(domain& vd, const std::string& name, const std::string& topic_name,
			size_t qsize = 16, notifier* ntfr = nullptr)
	{
		static_assert(sizeof(T) == sizeof(data), "data type size missmatch");

		_topic = vd.create_topic(topic_name, T::data_type);
		if (!_topic)
			throw std::logic_error("failed to create topic");

		_queue = _topic->subscribe(name, qsize, ntfr);
	}

	/// Delete subscriber.
	/// Unsubscribes from the topic. The queue is flushed and removed.
	~sub()
	{
		_topic->unsubscribe(_queue);
	}

	/// No copy
	sub( const sub& ) = delete;
	sub& operator=( const sub& ) = delete;

	/// Get name and data type
	const std::string& name() const { return _queue->name(); }
	const std::string& data_type() const { return _queue->data_type(); }

	/// Get queue and topic pointers
	vdds::sub_queue* queue() { return _queue; }
	vdds::topic* topic() { return _topic; }

	/// Pop data from fifo
	/// @param[out] d ref to data
	/// @return false if queue is empty, true otherwise
	bool pop(T& d) { return _topic->pop(_queue, static_cast<data&>(d)); }

	/// Flush all queued data
	void flush()
	{
		T d;
		while (pop(d));
	}
};

} // namespace vdds

#endif // VDDS_SUB_HPP
