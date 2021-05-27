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

#ifndef VDDS_DATA_HPP
#define VDDS_DATA_HPP

#define _GNU_SOURCE 1

#include <stdint.h>
#include <memory>
#include <array>

namespace vdds {

/// Base data type for PubSub operations.
/// User-defined types shall be derived from this type.
/// Sized to be exactly 256 bytes (4 cachelines on most CPUs).
struct data {
	/// Generic shared data type used with std::shared_ptr
	struct shared_t {
		virtual ~shared_t() { };
	};

	using shared_p = std::shared_ptr<shared_t>;
	using seqno_t  = uint64_t;
	using timestamp_t = uint64_t;

	// Generic plain data type used for small payload
	using plain_t = std::array<uint8_t, 256 - sizeof(seqno_t) - sizeof(timestamp_t) - sizeof(shared_p)>;

	seqno_t     seqno;     ///< Sequence number set by vDDS publish operation.
	timestamp_t timestamp; ///< Timestamp in nanoseconds (userdefined timebase).
	shared_p    shared;    ///< Shared data. Used by derived types for shared payload.
	plain_t     plain;     ///< Plain data. Used by derived types for plain payload.
};

} // namespace vdds

#endif // VDDS_DATA_HPP
