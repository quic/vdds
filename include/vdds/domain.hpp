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

#ifndef VDDS_DOMAIN_HPP
#define VDDS_DOMAIN_HPP

#include <string>
#include <vector>
#include <shared_mutex>

#include <hogl/area.hpp>

#include "topic.hpp"
#include "query.hpp"

namespace vdds {

/// Top-level VDDS domain.
/// Container for topics and PubSub data structures.
class domain {
private:
	using unique_topic = std::unique_ptr<topic>;
	using topic_vect   = std::vector<unique_topic>;

	std::string _name;  ///< Domain name.
	hogl::area* _area;  ///< Log area.

	// We could use a map or hashmap here but so far there is no
	// requirement to support large number of topics.

	topic_vect _topics; ///< Topic list (vector, protected by mutex)
	std::shared_timed_mutex _mutex; ///< Mutex used for syncing topic list access & updates

public:
	/// Create domain
	/// @param[in] domain name (should be all caps as a convention)
	explicit domain(const std::string& name = "");

	// No copies
	domain( const domain& ) = delete;
	domain& operator=( const domain& ) = delete;

	/// Get domain name.
	/// @return name as const ref to string
	const std::string& name() const { return _name; }

	/// Create topic.
	/// Topic names must be unique within the domain.
	/// If the topic with the same name already exists, and data-types match
	/// it is returned and used for all PubSub operations.
	/// Topics are never deleted (lifetime of the domain).
	/// @param[in] name topic name
	/// @param[in] data_type data type name
	/// @return name as const ref to string
	topic* create_topic(const std::string& name, const std::string& data_type);

	/// Dump domain info & stats into debug log.
	void dump(const query::filter& flt = query::filter{"any","any"});

	/// Query domain info & stats.
	/// To avoid runtime overhead the caller should preallocate query info (@see vdds::query::init)
	/// @param[out] di domain info reference
	/// @param[in] flt query filter
	void query(query::domain_info& di, const query::filter& flt = query::filter{"any","any"});
};

} // namespace vdds

#endif // VDDS_DOMAIN_HPP
