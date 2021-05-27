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

#include "vdds/domain.hpp"
#include "vdds/sub.hpp"
#include "vdds/pub.hpp"
#include "vdds/query.hpp"

#include "test-skell.hpp"
#include <hogl/flush.hpp>

#include <sstream>

// Simple test for domain interfaces

struct dummy_msg : vdds::data {
	static const char* data_type;
};
const char* dummy_msg::data_type = "dummy-type";

static bool run_query_test()
{
	hogl::post(area, area->INFO, "query test");

	vdds::domain vd("DEFAULT");

	// Create publishers and subscribers
	vdds::pub<dummy_msg> t0_pub0(vd, "PUB0", "/test/topic-0");
	vdds::sub<dummy_msg> t0_sub0(vd, "SUB0", "/test/topic-0");
	vdds::sub<dummy_msg> t0_sub1(vd, "SUB1", "/test/topic-0");
	vdds::sub<dummy_msg> t0_sub2(vd, "SUB2", "/test/topic-0");
	vdds::sub<dummy_msg> t0_sub3(vd, "SUB3", "/test/topic-0");

	vdds::pub<dummy_msg> t1_pub0(vd, "PUB0", "/test/topic-1");
	vdds::pub<dummy_msg> t1_pub1(vd, "PUB1", "/test/topic-1");
	vdds::sub<dummy_msg> t1_sub0(vd, "SUB0", "/test/topic-1");
	vdds::sub<dummy_msg> t1_sub1(vd, "SUB1", "/test/topic-1");
	vdds::sub<dummy_msg> t1_sub2(vd, "SUB2", "/test/topic-1");
	vdds::sub<dummy_msg> t1_sub3(vd, "SUB3", "/test/topic-1");

	vdds::pub<dummy_msg> t2_pub0(vd, "PUB0", "/test/topic-2");
	vdds::pub<dummy_msg> t2_pub1(vd, "PUB1", "/test/topic-2");
	vdds::sub<dummy_msg> t2_sub0(vd, "SUB0", "/test/topic-2");
	vdds::sub<dummy_msg> t2_sub1(vd, "SUB1", "/test/topic-2");
	vdds::sub<dummy_msg> t2_sub2(vd, "SUB2", "/test/topic-2");
	vdds::sub<dummy_msg> t2_sub3(vd, "SUB3", "/test/topic-2");

	vdds::pub<dummy_msg> t3_pub0(vd, "PUB0", "/test/topic-3");

	vdds::sub<dummy_msg> t4_sub0(vd, "SUB0", "/test/topic-4");

	// Topics with no subs and no pubs
	vd.create_topic("/test/topic-5", "dummy-type");
	vd.create_topic("/test/topic-6", "dummy-type");

	vd.dump();

	// Init query.
	// Pre-allocates space for specified number of topics, subs, pubs
	vdds::query::domain_info di;
	vdds::query::init(di, 128, 256, 256);

	// Query domain
	hogl::post(area, area->INFO, "domain query0 start");
	vd.query(di);
	vdds::query::clear(di);

	hogl::post(area, area->INFO, "domain query1 start");
	vd.query(di);

	hogl::post(area, area->INFO, "domain query complete");

	// Dump info
	std::ostringstream ss;
	ss << "domain: " << di.name << '\n';
	for (auto &ti : di.topics) {
		ss << " topic: " <<  ti.name
			<< " data-type: " << ti.data_type
			<< " push-count: " << ti.push_count
			<< '\n';

		if (ti.subs.size()) {
			ss << " subscribers: " << '\n';
			for (auto &si : ti.subs) {
				ss << "    "
					<< " name: " << si.name 
					<< " push-count: " << si.push_count
					<< " drop-count: " << si.drop_count
					<< " qcap: " << si.qcapacity 
					<< " qsize: " << si.qsize
					<< '\n';
			}
		}

		if (ti.pubs.size()) {
			ss << " publishers: " << '\n';
			for (auto &pi : ti.pubs) {
				ss << "    "
					<< " name: " << pi.name
					<< '\n';
			}
		}
	}

	hogl::flush();

	std::cout << ss.str();

	return true;
}

static bool run_basic_test()
{
	hogl::post(area, area->INFO, "basic test");

	vdds::domain vd("DEFAULT");

	// Create topic twice
	auto *vt0 = vd.create_topic("/test/topic/0", "dummy-data");
	auto *vt1 = vd.create_topic("/test/topic/0", "dummy-data");
	if (vt0 != vt1) {
		hogl::post(area, area->ERROR, "vt0 and vt1 are not same");
		return false;
	}

	// Create topic third time but with the different type.
	// This one must fail.
	auto *vt3 = vd.create_topic("/test/topic/0", "dummy-data-X");
	if (vt3) {
		hogl::post(area, area->ERROR, "vt3 didn't fail");
		return false;
	}

	vd.dump();
	return true;
}

static bool run_test()
{
	if (!run_basic_test())
		return false;

	if (!run_query_test())
		return false;

	return true;
}
