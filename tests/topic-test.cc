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

#define _GNU_SOURCE 1

#include "vdds/topic.hpp"

#include "test-skell.hpp"

// This test code does basic validation of the low-level topic and queue interfaces.
// Regular user code should use vdds::pub and vdds::sub typesafe wrappers instead.

// Publish N messages on the topic
void do_pub(vdds::topic& t, vdds::pub_handle* ph, unsigned int n)
{
	vdds::data d;
	for (unsigned int i=0; i<n; i++) {
		d.timestamp = i;
		d.plain[0] = i;
		t.push(ph, d);
	}
}

// Pull/pop all messages from all subscriber queues
void do_sub(vdds::topic& t, std::vector<vdds::sub_queue*>& qvec)
{
	vdds::data d;
	for (auto q : qvec) { while (t.pop(q, d)); }
}

bool run_test()
{
	hogl::post(area, area->INFO, "Starting test");

	// New topic
	vdds::topic vt("", "/test/topic-0", "/test/Type-X");

	vt.dump();

	std::vector<vdds::sub_queue*> qvec;

	// Create sub queues with different notifiers and queue sizes
	vdds::notifier_cv      ntfr_cv;
	vdds::notifier_polling ntfr_polling;

	qvec.push_back(vt.subscribe("sub0"));
	qvec.push_back(vt.subscribe("sub1", 64, &ntfr_cv));
	qvec.push_back(vt.subscribe("sub2", 64, &ntfr_polling));
	qvec.push_back(vt.subscribe("sub3", 64, &ntfr_polling));
	qvec.push_back(vt.subscribe("sub4", 32));

	vt.dump();

	// Publish 64 messages
	auto ph = vt.publish("pub0");
	do_pub(vt, ph, 64);

	vt.dump();

	// Pull from all queues
	do_sub(vt, qvec);

	vt.dump();

	// FIXME: check drop counts and things (validated via dbg log for now).

	// Unsubscribe
	for (auto q : qvec)
		vt.unsubscribe(q);

	vt.unpublish(ph);

	vt.dump();

	vdds::data d;
	hogl::post(area, area->INFO, "sizeof(vdds::data)) %u", sizeof(d));
	hogl::post(area, area->INFO, "sizeof(vdds::data::plain)) %u", sizeof(d.plain));
	hogl::post(area, area->INFO, "vdds::data::plain.size %u", d.plain.size());
	hogl::post(area, area->INFO, "vdds::data::plain.cap %u", d.plain.max_size());
	hogl::post(area, area->INFO, "vdds::data::plain.empty %u", d.plain.empty());

	return true;
}
