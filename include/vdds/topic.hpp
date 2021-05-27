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

#ifndef VDDS_TOPIC_HPP
#define VDDS_TOPIC_HPP

#include <stdint.h>
#include <string>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <list>

#include <hogl/area.hpp>
#include <hogl/post.hpp>

#include "sub-queue.hpp"
#include "pub-handle.hpp"
#include "query.hpp"

namespace vdds {

/// VDDS topic.
/// Contains subscriber queues and publisher handles.
class topic {
private:
	std::string _domain;    ///> domain name
	std::string _name;      ///> topic name
	std::string _data_type; ///> data type name

	hogl::area* _area; ///> log area

	struct cache {
		std::vector<sub_queue*>  subs; ///> list of subscribers queues (vect)
		std::vector<pub_handle*> pubs; ///> list of publisher handles (vect)
	};

	std::atomic<uint64_t> _next_seqno; ///> seqno for next pub operation

	// Cache related state
	std::atomic<cache*>   _cache_ptr;    ///> cache pointer
	std::atomic<uint32_t> _cache_refcnt; ///> cache ref count
	std::shared_timed_mutex _mutex;  ///> shared_mutex protects cache reads & updates

	/// Grab cache reference
	const cache* cache_get()
	{
		_cache_refcnt.fetch_add(1, std::memory_order_acquire);
		return _cache_ptr.load();
	}

	/// Release cache reference
	void cache_put(const cache* c)
	{
		_cache_refcnt.fetch_sub(1, std::memory_order_release);
	}

	/// Get a copy of the cache.
	/// Allocates new instance and copies the content.
	cache* cache_copy();

	/// Atomically swap cache pointer and release the orignal
	void cache_swap(cache *n);

public:
	/// Create topic
	/// @param[in] domain domain name
	/// @param[in] name topic name
	/// @param[in] data_type data type name
	explicit topic(const std::string& domain, const std::string& name, const std::string& data_type);

	/// Delete topic
	~topic();

	// No copies
	topic( const topic& ) = delete;
	topic& operator=( const topic& ) = delete;

	// Get topic name and data type
	const std::string& domain() const { return _domain; }
	const std::string& name() const { return _name; }
	const std::string& data_type() const { return _data_type; }

	/// Subscribe to this topic.
	/// Creates subscriber queue.
	/// @param[in] name subscriber name
	/// @param[in] qsize size of the queue (num data elemenents)
	/// @param[in] ntfr notifier (null, cv, polling)
	/// @return pointer to the subscriber queue
	sub_queue* subscribe(const std::string& name, unsigned int qsize = 16, notifier* ntfr = nullptr);

	/// Unsubscribe from this topic.
	/// @param[in] q subscriber queue
	void unsubscribe(sub_queue *q);

	/// Publish this topic.
	/// @param[in] name publisher name
	/// @return publisher handle
	pub_handle* publish(const std::string& name);

	/// Unpublish this topic.
	/// @param[in] publisher handle.
	void unpublish(pub_handle* p);

	/// Dump topic state (pubs, subs, etc) to debug log.
	void dump();

	/// Query topic info & stats.
	/// To avoid runtime overhead the caller should preallocate query info (@see vdds::query::init)
	/// @param[out] i topic info
	void query(query::topic_info& i);

	/// Push data to all subscribers.
	/// This method pushes a copy of data into each subscriber queue.
	/// @param[in] ph publisher handle
	/// @param[in] d data reference
	void push(pub_handle* ph, data& d)
	{
		d.seqno = _next_seqno.fetch_add(1, std::memory_order_relaxed);

		// Grab cache reference
		auto *c = cache_get();

		hogl::post(_area, _area->TRACE, hogl::arg_gstr(ph->trace_fmt()),
				d.seqno, d.timestamp, c->subs.size(), c->pubs.size());

		// We need to lock only if this topic has multiple publishers
		bool need_lock = (c->pubs.size() > 1);

		for (auto &q : c->subs) {
			// Push creates a proper copy of shared_ptr data (if any)
			q->push(d, need_lock);
		}

		// Release cache reference
		cache_put(c);
	}

	/// Pop data for subscriber.
	/// This method pops a copy of data from the subscriber queue.
	/// @param[in] ph publisher handle
	/// @param[in] d data reference
	bool pop(sub_queue* sq, data& d)
	{
		if (!sq->pop(d)) return false;

		hogl::post(_area, _area->TRACE, hogl::arg_gstr(sq->trace_fmt()), d.seqno, d.timestamp);
		return true;
	}
};

} // namespace vdds

#endif // VDDS_TOPIC_HPP
