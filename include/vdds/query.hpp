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

#ifndef VDDS_QUERY_HPP
#define VDDS_QUERY_HPP

#include <stdint.h>
#include <string>
#include <vector>

namespace vdds {
namespace query {

/// Publisher info.
struct pub_info {
	std::string name; ///> publisher name
};

/// Subscriber info.
struct sub_info {
	std::string name;    ///> subscriber name
	uint32_t push_count; ///> number of pushed data messages
	uint32_t drop_count; ///> number of dropped data messages
	uint32_t qcapacity;  ///> queue capacity
	uint32_t qsize;      ///> queue size (number of queued elements)
};

/// Topic info.
struct topic_info {
	std::string name;      ///> topic name
	std::string data_type; ///> data type name
	std::vector<sub_info> subs; ///> vector of subscribers
	std::vector<pub_info> pubs; ///> vector of publishers
	uint64_t push_count;   ///> number of pushed data messages
};

/// Domain info.
struct domain_info {
	std::string name; ///> Domain name
	std::vector<topic_info> topics; ///> vector of topics
};

/// Query filter.
/// Allows for filtering domain and topic info.
/// @todo support for regex for extra flexibility
struct filter {
	std::string topic_name; ///> topic name or "any"
	std::string data_type;  ///> data_type name or "any"
};

/// Init & preallocate query result
void init(domain_info& i, size_t ntopics, size_t nsubs, size_t npubs);
void init(topic_info& i, size_t nsubs, size_t npubs);

/// Clear query results
void clear(domain_info& i);
void clear(topic_info& i);

} // namespace query
} // namespace vdds

#endif // VDDS_QUERY_HPP
