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

// This test implements a simple ping client / server with a pair of req/rsp topics.
// Client publishes /ping/req topic and subscribes to /ping/rsp.
// Server publishes /ping/rsp topic and subscribes to /ping/req.
// Both topics use the same ping_msg data type.

// Data type
struct ping_msg : vdds::data {
	// This static member is mandatory and defines a unique type name.
	// It is used for validating topic pub & sub registrations.
	static const char* data_type;

	// Simple payload mapped to vdds::data::plain
	struct payload_t {
		uint64_t seq0;
		uint64_t seq1;
		uint64_t seq2;
		uint64_t seq3;
	};

	// Map payload to vdds::data::plain
	payload_t* payload() { return reinterpret_cast<payload_t*>(&this->plain); }
};

// Data type name
const char* ping_msg::data_type = "vdds.test.ping-msg";

// Server
class ping_server {
private:
	hogl::area*       _area;
	std::thread       _thread;
	std::atomic_bool  _killed;

	vdds::notifier_cv   _nf;       // CV notifier for sub interface
	vdds::sub<ping_msg> _req_sub;  // Ping req subscriber
	vdds::pub<ping_msg> _rsp_pub;  // Ping rsp publisher

	void loop()
	{
		// logging ringbuf for this thread
		hogl::tls tls(_req_sub.name().c_str());

		hogl::post(_area, _area->INFO, "server started");

		while (!_killed) {
			// Pop req from subscriber queue
			ping_msg m;
			while (_req_sub.pop(m)) {
				auto p = m.payload();
				hogl::post(_area, _area->INFO, hogl::arg_gstr("req seqno %llu timestamp %llu seq0 %llx seq1 %llx seq2 %llx seq3 %llx"),
						m.seqno, m.timestamp, p->seq0, p->seq1, p->seq2, p->seq3);
				// Publish rsp
				_rsp_pub.push(m);
			}

			_nf.wait_for(std::chrono::milliseconds(1));
		}

		hogl::post(_area, _area->INFO, "server stoped");
	}

public:
	ping_server(vdds::domain& vd, const std::string& name) :
		_area(hogl::add_area("PING-SERVER")),      // log area
		_req_sub(vd, name, "/ping/req", 16, &_nf), // subscribe to req (qsize 16, notifier)
		_rsp_pub(vd, name, "/ping/rsp")            // publish rsp
	{}

	void start()
	{
		_killed = false;
		_thread = std::thread([&](){ loop(); });
	}

	bool check()
	{
		// Simple checks for the test
		if (_req_sub.queue()->drop_count() != 0) {
			hogl::post(_area, _area->ERROR, "droped %llu reqs", _req_sub.queue()->drop_count());
			return false;
		}
		return true;
	}

	void kill()
	{
		_killed = true;
		_thread.join();
	}
};

// Client
class ping_client {
private:
	hogl::area*       _area;

	std::thread       _thread;
	std::atomic_bool  _killed;

	vdds::notifier_cv   _nf;
	vdds::pub<ping_msg> _req_pub;
	vdds::sub<ping_msg> _rsp_sub;

	void loop()
	{
		// logging ringbuf for this thread
		hogl::tls tls(_rsp_sub.name().c_str());

		hogl::post(_area, _area->INFO, "client started");

		while (!_killed) {
			// Generate ping req
			ping_msg m;
			m.timestamp = tls.ring()->timestamp().to_nsec();

			auto p = m.payload();
			p->seq0 = 0x1234567890;
			p->seq1 = 0x0987654321;
			p->seq2 = 0x1a1a1a1a1a;
			p->seq3 = 0x7e7e7e7e7e;

			// Publish
			_req_pub.push(m);

			// Wait for rsp
			_nf.wait_for(std::chrono::milliseconds(100));

			// Pop and process rsp
			while (_rsp_sub.pop(m)) {
				uint64_t now = tls.ring()->timestamp().to_nsec();
				hogl::post(_area, _area->INFO, "rsp seqno %llu timestamp %llu rtt %llu (nsec)",
						m.seqno, m.timestamp, now - m.timestamp);
			}
		}

		hogl::post(_area, _area->INFO, "client stoped");
	}

public:
	ping_client(vdds::domain& vd, const std::string& name) :
		_area(hogl::add_area("PING-CLIENT")),     // log area
		_req_pub(vd, name, "/ping/req"),          // publish req
		_rsp_sub(vd, name, "/ping/rsp", 16, &_nf) // subscribe to rsp (qsize 16, notifier cv)
	{}

	void start()
	{
		_killed = false;
		_thread = std::thread([&](){ loop(); });
	}

	bool check()
	{
		// Simple checks for the test
		if (_rsp_sub.queue()->drop_count() != 0) {
			hogl::post(_area, _area->ERROR, "droped %llu rsps", _rsp_sub.queue()->drop_count());
			return false;
		}
		return true;
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

	// Start client and server (this creates topics, etc)
	ping_client c(vd, "CLIENT0");
	ping_server s(vd, "SERVER0");

	vd.dump();

	s.start();
	c.start();

	std::this_thread::sleep_for(std::chrono::seconds(1));

	bool r = c.check() && s.check();

	c.kill();
	s.kill();

	return r;
}
