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

#include <hogl/area.hpp>
#include <hogl/post.hpp>

#include "vdds/query.hpp"

namespace vdds {
namespace query {

void init(topic_info& ti, size_t nsubs, size_t npubs)
{
	ti.name.reserve(128);
	ti.data_type.reserve(128);
	ti.subs.reserve(nsubs);
	ti.pubs.reserve(npubs);
	for (auto &si : ti.subs)
		si.name.reserve(128);
	for (auto &pi : ti.pubs)
		pi.name.reserve(128);
}

void init(domain_info& di, size_t ntopics, size_t nsubs, size_t npubs)
{
	di.name.reserve(128);
	di.topics.reserve(ntopics);
	for (auto &ti : di.topics)
		init(ti, nsubs, npubs);
}

void clear(topic_info& ti)
{
	ti.name.clear();
	ti.data_type.clear();
	ti.subs.clear();
	ti.pubs.clear();
}

void clear(domain_info& di)
{
	di.name.clear();
	di.topics.clear();
}

} // namespace query
} // namespace vdds
