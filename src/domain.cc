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

#include <stdexcept>

#include <hogl/area.hpp>
#include <hogl/post.hpp>
#include <hogl/fmt/format.h>

#include "vdds/domain.hpp"

namespace vdds {

domain::domain(const std::string& name) : _name(name)
{ 
	_area = hogl::add_area(fmt::format("VDDS{}{}", _name.empty() ? "" : "-", _name).c_str());
	if (!_area)
		throw std::logic_error("failed to create log area");
}

// Create topic.
// Returns existing topic with same name if data type matches.
topic* domain::create_topic(const std::string& name, const std::string& data_type)
{
	std::unique_lock<std::shared_timed_mutex> lock(_mutex); // read-write exclusive

	// Look for existing topic and validate the data-type
	for (auto &p : _topics) {
		if (p->name() == name) {
			if (p->data_type() == data_type)
				return p.get();

			hogl::post(_area, _area->ERROR, hogl::arg_gstr("topic %s already exists: data-type %s requested %s"),
					name, p->data_type(), data_type);
			return 0;
		}
	}

	// Allocate new topic
	auto nt = std::make_unique<topic>(_name, name, data_type);
	auto t  = nt.get();
	_topics.push_back(std::move(nt));

	hogl::post(_area, _area->INFO, hogl::arg_gstr("new-topic %s data-type %s"), t->name(), t->data_type());
	return t;
}

void domain::dump(const query::filter& flt)
{
	std::shared_lock<std::shared_timed_mutex> lock(_mutex); // read-only shared

	if (flt.topic_name == "any" && flt.data_type == "any") {
		// Full dump with all topics and data types
		hogl::post(_area, _area->INFO, hogl::arg_gstr("ntopics %u"), _topics.size());
		for (auto &t : _topics) t->dump();
		return;
	}

	// Filtered dump
	for (auto &t: _topics) {
		if (flt.topic_name != "any" && flt.topic_name != t->name())
			continue;
		if (flt.data_type != "any" && flt.data_type != t->data_type())
			continue;
		t->dump();
	}
}

void domain::query(query::domain_info& di, const query::filter& flt)
{
	query::clear(di);
	di.name = _name;

	std::shared_lock<std::shared_timed_mutex> lock(_mutex); // read-only shared

	if (flt.topic_name == "any" && flt.data_type == "any") {
		// Full output with all topics and data types
		di.topics.resize(_topics.size());
		for (unsigned i=0; i<_topics.size(); i++) _topics[i]->query(di.topics[i]);
		return;
	}

	// Filtered output
	unsigned int i = 0;
	for (auto &t: _topics) {
		if (flt.topic_name != "any" && flt.topic_name != t->name())
			continue;
		if (flt.data_type != "any" && flt.data_type != t->data_type())
			continue;
		di.topics.resize(i + 1);
		t->query(di.topics[i]);
		i++;
	}
}

} // namespace vdds
