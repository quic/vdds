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
#include "vdds/query.hpp"
#include "vdds/utils.hpp"

#include <hogl/fmt/format.h>

#include <ostream>

namespace vdds {
namespace utils {

void to_dot(vdds::domain& vd, std::ostream& out)
{
	vdds::query::domain_info di;
	vd.query(di);

	// Generate digraph settings
	out << "digraph { \n";
	out << "  graph [splines=true, rankdir=LR] \n";
	out << "  edge  [splines=true] \n";
	out << "  node  [shape=box, style=\"rounded, filled\"] \n";

	// Generate pub nodes
	out << "{ \n";
	for (auto &ti : di.topics) {
		for (auto &pi : ti.pubs) {
			out << fmt::format("\"{}\"[fillcolor=lightblue]; \n", pi.name);
		}
	}
	out << "} \n";

	// Generate topic nodes
	out << "{ \n";
	for (auto &ti : di.topics) { 
		out << fmt::format("\"{}\"[fillcolor=orange]; \n", ti.name);
	}
	out << "} \n";

	// Generate sub nodes
	out << "{ \n";
	for (auto &ti : di.topics) { 
		for (auto &si : ti.subs) {
			out << fmt::format("\"{}\"[fillcolor=green]; \n", si.name);
		}
	}
	out << "} \n";

	// Generate edges
	for (auto &ti : di.topics) {
		// pub -> topic edge
		for (auto &pi : ti.pubs)
			out << fmt::format("\"{}\" -> \"{}\"\n", pi.name, ti.name);

		// topic -> sub edge
		for (auto &si : ti.subs) 
			out << fmt::format("\"{}\" -> \"{}\"\n", ti.name, si.name);
	}

	// End of graph
	out << "} \n";
}

} // namespace utils
} // namespace vdds
