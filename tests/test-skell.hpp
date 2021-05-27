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

#include <stdint.h>

#include <boost/program_options.hpp>

#include <hogl/format-basic.hpp>
#include <hogl/output-stdout.hpp>
#include <hogl/engine.hpp>
#include <hogl/area.hpp>
#include <hogl/mask.hpp>
#include <hogl/post.hpp>
#include <hogl/timesource.hpp>

// Command line options
namespace po = boost::program_options;
static po::variables_map optmap;

static hogl::area *area = nullptr;

static bool run_test(); ///// *** Defined in the actual test where this skell is included

static std::vector<std::string> log_mask;

int main(int argc, char *argv[])
{
	// **** Parse command line arguments ****
	po::options_description optdesc("Data sink test app");
	optdesc.add_options()
		("help", "Print this message")
		("duration",   po::value<unsigned int>()->default_value(2), "Test duration in seconds")
		("log-format", po::value<std::string>()->default_value("fast1"), "Log output format")
		("log-mask",   po::value<std::vector<std::string>>(&log_mask)->composing(), "Log mask. Multiple masks can be specified.");

	po::store(po::parse_command_line(argc, argv, optdesc), optmap);
	po::notify(optmap);

	// ** All argument values (including the defaults) are now storred in 'optmap' **

	if (optmap.count("help")) {
		std::cout << optdesc << std::endl;
		exit(1);
	}

	hogl::format_basic lf(optmap["log-format"].as<std::string>().c_str());
	hogl::output_stdout lo(lf, 64 * 1024);

	hogl::engine::options eng_opts = hogl::engine::default_options;
	eng_opts.timesource = &hogl::monotonic_timesource;
	for (auto &m : log_mask)
		eng_opts.default_mask << m;

	hogl::activate(lo, eng_opts);

	// *** HOGL engine is running. Avoid exit()ing without going through hogl shutdown sequence below.

	area = hogl::add_area("VDDS-TEST");

	bool pass = run_test();

	hogl::deactivate();

	return pass ? 0 : 1;
}
