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

#ifndef VDDS_PUB_HPP
#define VDDS_PUB_HPP

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include <hogl/area.hpp>
#include <hogl/post.hpp>

#include "domain.hpp"
#include "topic.hpp"

namespace vdds {

/// Typesafe publisher.
/// Main publisher interface. Takes care of creating topic and publishing data.
/// @param T data type
template<typename T>
class pub {
private:
	vdds::pub_handle* _handle; ///> subscriber handle
	vdds::topic*      _topic;  ///> topic pointer

public:
	/// Create publisher.
	/// Creates topic and registers new publisher
	explicit pub(domain& vd, const std::string& name, const std::string& topic_name)
	{
		static_assert(sizeof(T) == sizeof(data), "data type size missmatch");

		_topic  = vd.create_topic(topic_name, T::data_type);
		if (!_topic)
			throw std::logic_error("failed to create topic");

		_handle = _topic->publish(name);
	}

	/// Remove publisher.
	/// Unregisters from the topic.
	~pub()
	{
		_topic->unpublish(_handle);
	}

	// No copies
	pub( const pub& ) = delete;
	pub& operator=( const pub& ) = delete;

	/// Get name and data type
	const std::string& name() const { return _handle->name(); }
	const std::string& data_type() const { return _topic->data_type(); }

	/// Get topic pointer
	vdds::topic* topic() { return _topic; }

	/// Push data to all subscribers.
	/// @param d ref to data.
	void push(T& d) { _topic->push(_handle, static_cast<data&>(d)); }
};

} // namespace vdds

#endif // VDDS_PUB_HPP
