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

// Multi Pub/Sub test

// Data types
struct timesync_msg : vdds::data {
	static const char* data_type;

	struct payload_t {
		uint64_t ptp_timestamp;
		uint64_t gps_timestamp;
	};

	payload_t* payload() { return reinterpret_cast<payload_t*>(&this->plain); }
	const payload_t* payload() const { return reinterpret_cast<const payload_t*>(&this->plain); }
	uint64_t ptp_timestamp() const { return payload()->ptp_timestamp; }
	uint64_t gps_timestamp() const { return payload()->gps_timestamp; }
};

struct sensor_msg : vdds::data {
	static const char* data_type;

	struct payload_t {
		uint64_t sample[4];
	};

	payload_t* payload() { return reinterpret_cast<payload_t*>(&this->plain); }
};

struct detector_msg : vdds::data {
	static const char* data_type;

	struct payload_t {
		uint64_t avg[4];
	};

	payload_t* payload() { return reinterpret_cast<payload_t*>(&this->plain); }
};

// Data type names
const char* timesync_msg::data_type = "vdds.test.data.timesync";
const char* sensor_msg::data_type   = "vdds.test.data.sensor";
const char* detector_msg::data_type = "vdds.test.data.detector";

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

// Timer
class timesync : public engine {
private:
	std::string _name; // timer instance name (used for threads, tls, etc)
	hogl::area* _area;

	vdds::pub<timesync_msg> _pub;

	void loop()
	{
		hogl::tls tls(_name.c_str());

		hogl::post(_area, _area->INFO, "timer %s started", _name);

		// Publish timesync every 20msec
		while (!_killed) {
			timesync_msg m;
			m.timestamp = tls.ring()->timestamp().to_nsec();

			auto p = m.payload();
			p->ptp_timestamp = m.timestamp - 123456;
			p->gps_timestamp = m.timestamp - 999999;

			hogl::post(_area, _area->INFO, hogl::arg_gstr("%s timesync: timestamp %llu: ptp %llu gps %llu"),
					_name, m.timestamp, m.ptp_timestamp(), m.gps_timestamp());

			// Publish timesync data
			_pub.push(m);

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		hogl::post(_area, _area->INFO, "timer %s stoped", _name);
	}

public:
	timesync(vdds::domain& vd, const std::string& name) :
		_name(name),
		_area(hogl::add_area("TIMESYNC")),
		_pub(vd, _name, "/test/timesync") // publish
	{}
};

// Driver
class sensor_drv : public engine {
private:
	std::string _sensor_name; // sensor name (used for topics, etc)
	std::string _name;        // driver instance name (used for threads, tls, etc)
	hogl::area* _area;
	vdds::pub<sensor_msg> _pub;

	void loop()
	{
		hogl::tls tls(_name.c_str());

		hogl::post(_area, _area->INFO, "driver %s started", _name);

		// Publish new data every 10msec
		while (!_killed) {
			sensor_msg m;
			m.timestamp = tls.ring()->timestamp().to_nsec();

			auto p = m.payload();
			p->sample[0] = 1;
			p->sample[1] = 2;
			p->sample[2] = 3;
			p->sample[3] = 4;

			hogl::post(_area, _area->INFO, hogl::arg_gstr("%s new-data: timestamp %llu: %u %u %u %u"),
					_name, m.timestamp, p->sample[0], p->sample[1], p->sample[2], p->sample[3]);

			// Publish sensor data
			_pub.push(m);

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		hogl::post(_area, _area->INFO, "driver %s stoped", _name);
	}

public:
	sensor_drv(vdds::domain& vd, const std::string& sensor_name) :
		_sensor_name(sensor_name),
		_name(fmt::format("DRV-{}", sensor_name)),
		_area(hogl::add_area("SENSOR-DRV")),
		_pub(vd, _name, fmt::format("/test/sensor/data/{}", sensor_name)) // publish
	{}
};

// Detector
class detector : public engine {
private:
	std::string _name; // instance name (thread, tls, etc)
	hogl::area* _area;

	vdds::pub<detector_msg> _pub_det;

	using sensor_sub = vdds::sub<sensor_msg>;
	using unique_sub = std::unique_ptr<sensor_sub>;

	vdds::notifier_cv       _nf;
	std::vector<unique_sub> _sub_sens;

	void process(sensor_msg& sm)
	{
		detector_msg dm;

		auto sp = sm.payload();
		auto dp = dm.payload();

		hogl::post(_area, _area->INFO, hogl::arg_gstr("%s new-data: timestamp %llu: %u %u %u %u"),
				_name, sm.timestamp, sp->sample[0], sp->sample[1], sp->sample[2], sp->sample[3]);

		// Generate new detection
		dm.timestamp = sm.timestamp;
		dp->avg[0] = (sp->sample[0] + sp->sample[1] + sp->sample[2] + sp->sample[3]) / 4;

		hogl::post(_area, _area->INFO, "%s detection seqno %llu timestamp %llu: avg %llu",
				_name, sm.seqno, sm.timestamp, dp->avg[0]);

		_pub_det.push(dm);
	}

	void loop()
	{
		hogl::tls tls(_name.c_str());

		hogl::post(_area, _area->INFO, "detector %s started", _name);

		while (!_killed) {
			// Wait for data
			_nf.wait_for(std::chrono::milliseconds(100));

			// Process
			sensor_msg sm;
			for (auto &ss : _sub_sens) { while (ss->pop(sm)) process(sm); }
		}

		hogl::post(_area, _area->INFO, "detector %s stoped", _name);
	}

public:
	detector(vdds::domain& vd, const std::string& name, std::vector<std::string> sensor_names) :
		_name(name),
		_area(hogl::add_area("DETECTOR")),
		_pub_det(vd, name, fmt::format("/test/detector/data/{}", name))
	{
		// Subscribe to sensors
		for (auto& s : sensor_names)
			_sub_sens.push_back( std::make_unique<sensor_sub>(vd, name, fmt::format("/test/sensor/data/{}", s), 16, &_nf) );
	}
};

// Controller
class controller : public engine {
private:
	std::string       _name; // instance name (thread, tls, etc)
	hogl::area*       _area;

	using detector_sub  = vdds::sub<detector_msg>;
	using unique_detsub = std::unique_ptr<detector_sub>;

	using sensor_sub    = vdds::sub<sensor_msg>;
	using unique_sensub = std::unique_ptr<sensor_sub>;

	std::vector<unique_detsub> _sub_dets; // detector subscriptions
	std::vector<unique_sensub> _sub_sens; // sensor subscriptions

	vdds::notifier_cv       _nf_ts;
	vdds::sub<timesync_msg> _sub_ts;

	void process(unique_detsub& ds, detector_msg& dm)
	{
		auto dp = dm.payload();
		hogl::post(_area, _area->INFO, hogl::arg_gstr("%s new detector %s data: timestamp %llu: %u"),
				_name, ds->topic()->name(), dm.timestamp, dp->avg[0]);
	}

	void process(unique_sensub& ss, sensor_msg& sm)
	{
		auto sp = sm.payload();
		hogl::post(_area, _area->INFO, hogl::arg_gstr("%s new sensor data: timestamp %llu: %u %u %u %u"),
				_name, ss->topic()->name(), sm.timestamp, sp->sample[0], sp->sample[1], sp->sample[2], sp->sample[3]);
	}

	void process(timesync_msg& tm)
	{
		auto tp = tm.payload();
		hogl::post(_area, _area->INFO, hogl::arg_gstr("%s timesync: timestamp %llu: ptp %llu gps %llu"),
				_name, tm.timestamp, tp->ptp_timestamp, tp->gps_timestamp);
	}

	void loop()
	{
		hogl::tls tls(_name.c_str());

		hogl::post(_area, _area->INFO, "controller %s started", _name);

		while (!_killed) {
			// Wait for timesync
			_nf_ts.wait_for(std::chrono::milliseconds(100));

			hogl::post(_area, _area->TRACE, "control loop %s # ph:B", _name);

			// Process timesync data
			timesync_msg tm;
			while (_sub_ts.pop(tm)) process(tm);

			// Process sensor data
			sensor_msg sm;
			for (auto &ss : _sub_sens) { while (ss->pop(sm)) process(ss, sm); }

			// Process detector data
			detector_msg dm;
			for (auto &ds : _sub_dets) { while (ds->pop(dm)) process(ds, dm); }

			hogl::post(_area, _area->TRACE, "control loop %s # ph:E", _name);
		}

		hogl::post(_area, _area->INFO, "controller %s stoped", _name);
	}

public:
	controller(vdds::domain& vd, const std::string& name,
			std::vector<std::string> sensor_names, std::vector<std::string> detector_names) :
		_name(name),
		_area(hogl::add_area("CONTROL")),
		_sub_ts(vd, name, "/test/timesync", 2, &_nf_ts) // timesync sub uses CV notifier
	{
		// Subscribe to sensors
		for (auto& s : sensor_names)
			_sub_sens.push_back( std::make_unique<sensor_sub>(vd, name, fmt::format("/test/sensor/data/{}", s), 32) );
		// Subscribe to detectors
		for (auto& s : detector_names)
			_sub_dets.push_back( std::make_unique<detector_sub>(vd, name, fmt::format("/test/detector/data/{}", s), 32) );

		// No notifier for sensor and detector subs, but deeper queues,
		// we're going to process them when we get timesync.
	}
};

bool run_test()
{
	hogl::post(area, area->INFO, "Starting test");

	// Create default domain
	vdds::domain vd("MAIN");

	// Vector of drivers
	using unique_drv = std::unique_ptr<sensor_drv>;
	std::vector<unique_drv> drivers;

	// Create drivers
	std::vector<std::string> sensor_names = { "CAM0", "CAM1", "CAM2", "CAM3", "CAM4" };
	for (auto& s: sensor_names)
		drivers.push_back( std::make_unique<sensor_drv>(vd, s) );

	vd.dump();

	// Create detectors
	detector det0(vd, "DET0", { "CAM0", "CAM1", "CAM4" });
	detector det1(vd, "DET1", { "CAM0", "CAM2", "CAM3", "CAM4" });

	// Create controllers
	controller ctrl0(vd, "CTRL0", { "CAM0" }, { "DET0", "DET1" });
	controller ctrl1(vd, "CTRL1", { "CAM0", "CAM2" }, { "DET1" });

	// Create timesyncs.
	// We don't even start the second one it's created simply to register second publisher.
	timesync sync0(vd, "SYNC0");
	timesync sync1(vd, "SYNC1");

	vd.dump();

	// Start everything up
	ctrl0.start();
	ctrl1.start();
	det0.start();
	det1.start();
	for (auto& drv: drivers)
		drv->start();
	sync0.start();

	// Let things run for 2 seconds
	std::this_thread::sleep_for(std::chrono::seconds( optmap["duration"].as<unsigned int>() ));

	// Stop everything
	sync0.kill();
	for (auto& drv: drivers)
		drv->kill();
	det0.kill();
	det1.kill();
	ctrl0.kill();
	ctrl1.kill();

	vd.dump();

	// Save domain as .dot for graphviz
	auto ss = std::ofstream("multi-test.dot", std::ofstream::trunc);
	vdds::utils::to_dot(vd, ss);

	// Simple query for one sensor
	vdds::query::domain_info di;
	vd.query(di, { "/test/sensor/data/CAM0", "any" });
	hogl::post(area, area->INFO, "topic %s push_count %llu", di.topics[0].name, di.topics[0].push_count);

	// Dump all topics that carry sensor data
	vd.dump({ "any", sensor_msg::data_type });

	return true;
}
