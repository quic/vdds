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

#include "test-skell.hpp"

// This test implements a simple subscriber with super long timeout.
// domain::kick() and domain::shutdown() are then used to wake up subscribers.

// Two dummy data types for testing filters
struct dummy_msg0 : vdds::data { static const char* data_type; };
struct dummy_msg1 : vdds::data { static const char* data_type; };

const char* dummy_msg0::data_type = "vdds.test.dummy-msg0";
const char* dummy_msg1::data_type = "vdds.test.dummy-msg1";

// Dummy sub
template<typename T>
class dummy_sub {
private:
	hogl::area*       _area;
	std::thread       _thread;
	std::atomic_bool  _killed;

	vdds::notifier_cv _nf;   // CV notifier
	vdds::sub<T>      _sub;  // Subscriber

	void loop()
	{
		hogl::post(_area, _area->INFO, "thread started");

		while (!_killed) {
			hogl::post(_area, _area->INFO, "checking queue");

			// Pop req from subscriber queue
			T m;
			while (_sub.pop(m)) {
				hogl::post(_area, _area->INFO, "msg seqno %llu timestamp %llu", m.seqno, m.timestamp);
			}

			// Use really long timeout to make sure shutdown actually overwrites it
			_nf.wait_for(std::chrono::seconds(100));
		}

		hogl::post(_area, _area->INFO, "thread stoped");
	}

public:
	dummy_sub(vdds::domain& vd, const std::string& name, const std::string& topic_name) :
		_area(hogl::add_area(name)),     // log area
		_sub(vd, name, topic_name, 16, &_nf)  // subscribe to dummy msg (qsize 16, notifier)
	{}

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

bool run_test()
{
	hogl::post(area, area->INFO, "Starting test");

	// Create default domain
	vdds::domain vd("DEFAULT");

	const char *topic_name0 = "/dummy/msg/0";
	const char *topic_name1 = "/dummy/msg/1";

	// Start dummy subscribers
	dummy_sub<dummy_msg0> d0(vd, "DS0", topic_name0);
	dummy_sub<dummy_msg0> d1(vd, "DS1", topic_name0);
	dummy_sub<dummy_msg1> d2(vd, "DS2", topic_name1);
	dummy_sub<dummy_msg1> d3(vd, "DS3", topic_name1);

	vd.dump();

	d0.start();
	d1.start();
	d2.start();
	d3.start();

	// Kick the entire domain (all subs should wakeup)
	hogl::post(area, area->INFO, "Kicking all topics");
	vd.kick();

	hogl::post(area, area->INFO, "Kick");
	for (auto i : { 0,1,2,3 }) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		// Kick topic with msg0 type
		hogl::post(area, area->INFO, "Kicking %s %u", dummy_msg0::data_type, i);
		vd.kick({ "any", dummy_msg0::data_type });
	}

	for (auto i : { 0,1,2,3 }) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		// Kick /dummy/msg/1 topic
		hogl::post(area, area->INFO, "Kicking %s %u", topic_name1, i);
		vd.kick({ topic_name1, "any" });
	}

	std::this_thread::sleep_for(std::chrono::seconds(1));

	hogl::post(area, area->INFO, "Stopping test");
	// Shutdown domain (all subs should start timeing out)
	vd.shutdown(std::chrono::microseconds(1000));

	std::this_thread::sleep_for(std::chrono::seconds(1));

	d3.kill();
	d2.kill();
	d1.kill();
	d0.kill();

	return true;
}
