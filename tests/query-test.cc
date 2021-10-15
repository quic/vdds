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
#include "vdds/pub.hpp"
#include "vdds/sub.hpp"
#include "vdds/query.hpp"
#include "vdds/utils.hpp"

#include "test-skell.hpp"

#include <hogl/fmt/format.h>

#include <fstream>

// Domain and topic query tests

// Simple thread + loop
class engine {
public:
	std::thread _thread;
	std::atomic_bool _killed;
	virtual void loop() = 0;

	void start()
	{
		_killed = false;
		_thread = std::thread([&](){ loop(); });
	}

	void kill()
	{
		_killed = true;
		_thread.join();
	}
};

// Query runner 
class query_runner : public engine {
private:
	std::string _name;
	hogl::area* _area;
	vdds::domain& _vd;
	std::string _topic;

	void loop()
	{
		hogl::tls tls("QRR-" + _name);

		hogl::post(_area, _area->INFO, "runner %s started", _name);

		vdds::query::domain_info di;
		vdds::query::init(di, 100, 10, 10);

		while (!_killed) {
			hogl::post(_area, _area->DEBUG, hogl::arg_gstr("query-start: ntopics %u"), di.topics.size());
			_vd.query(di, { _topic, "any" });
			hogl::post(_area, _area->DEBUG, hogl::arg_gstr("query-done: ntopics %u"), di.topics.size());

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		hogl::post(_area, _area->INFO, "runner %s stoped", _name);
	}

public:
	query_runner(vdds::domain& vd, const std::string& name, const std::string& topic = "any") :
		_name(name),
		_area(hogl::add_area("QRR-" + name)),
		_vd(vd),
		_topic(topic)
	{}
};

static void create_topic(vdds::domain& vd, const std::string& name, const std::string& dtype, unsigned int npub, unsigned int nsub)
{
	vdds::topic* t = vd.create_topic(name, dtype);

	for (unsigned int i=0; i<nsub; i++)
		(void) t->subscribe(fmt::format("SUB-{}", i));

	for (unsigned int i=0; i<npub; i++)
		(void) t->publish(fmt::format("PUB-{}", i));
}

bool run_test()
{
	hogl::post(area, area->INFO, "Starting test");

	// Create default domain
	vdds::domain vd("MAIN");

	for (unsigned int i=0; i<100; i++)
		create_topic(vd, fmt::format("/query/test/topic/{}", i), fmt::format("query.test.data.{}", i), 1, 10);

	vd.dump();

	query_runner qr0(vd, "0", "any");                   // all topics
	query_runner qr1(vd, "1", "/query/test/topic/0");   // single topic query
	query_runner qr2(vd, "2", "/query/test/topic/99");  // single topic query

	// Start everything up
	qr0.start();
	qr1.start();
	qr2.start();

	// Let things run
	std::this_thread::sleep_for(std::chrono::seconds( optmap["duration"].as<unsigned int>() ));

	// Stop everything
	qr0.kill();
	qr1.kill();
	qr2.kill();

	// Save domain as .dot for graphviz
	auto ss = std::ofstream("query-test.dot", std::ofstream::trunc);
	vdds::utils::to_dot(vd, ss);

	return true;
}
