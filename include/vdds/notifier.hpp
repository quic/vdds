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

#ifndef VDDS_NOTIFIER_HPP
#define VDDS_NOTIFIER_HPP

#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>

namespace vdds {

/// Notifer interface.
/// Used for waking up subscriber threads.
/// Multiple subscriber queues can share the same notifier
class notifier {
private:
	std::string _name; ///> notifier name

public:
	/// Create notifier.
	/// @param name notifier name (null, cv, etc)
	notifier(const std::string& name) : _name(name) {}

	// No copies.
	notifier(const notifier&) = delete;
	notifier& operator=(const notifier&) = delete;

	/// Get name.
	const std::string& name() const { return _name; }

	/// Wait for nofication with a timeout.
	/// @param t timeout (chrono duration)
	virtual void wait_for(std::chrono::nanoseconds t = std::chrono::milliseconds(1)) = 0;

	/// Notify.
	/// Called from queue::push
	virtual void notify() {};

	/// Shutdown.
	/// Called from queue::shutdown
	/// @param t new timeout value
	virtual void shutdown(std::chrono::nanoseconds t = std::chrono::milliseconds(1)) {};
};

/// Polling notifier.
/// Implements a simple timeout without.
class notifier_polling : public notifier {
public:
	notifier_polling() : notifier("polling") {}

	// Wait for timeout
	void wait_for(std::chrono::nanoseconds t) override
	{
		std::this_thread::sleep_for(t);
	}
};

/// Condition variable notifier.
/// Signals cv to wakeup subscriber.
class notifier_cv : public notifier {
private:
	std::condition_variable  _cv; ///> condition variable
	std::chrono::nanoseconds _ft; ///> forced timeout (during shutdown)
	std::mutex _mutex;            ///> mutex for sync
	uint32_t   _count;            ///> number of triggers

public:
	notifier_cv() : notifier("cv"), _ft(0), _count(0) {}

	/// Wait for contion signal or timeout.
	/// @param[in] t timeout (chrono duration)
	void wait_for(std::chrono::nanoseconds t) override
	{
    		std::unique_lock<std::mutex> ul(_mutex);
		_cv.wait_for(ul, _ft.count() ? _ft : t, [&]{return _count;});
		_count = 0;
	}

	/// Wake up subscriber.
	/// Called from queue::push.
	void notify() override
	{
		_mutex.lock();
		_count++;
		_mutex.unlock();
		_cv.notify_one();
	}

	/// Wake up subscriber and set new timeout.
	/// Called from queue::shutdown.
	/// @param[in] t timeout (chrono duration)
	void shutdown(std::chrono::nanoseconds ft) override
	{
		_mutex.lock();
		_count++;
		_ft = ft;
		_mutex.unlock();
		_cv.notify_one();
	}
};

} // namespace vdds

#endif // VDDS_NOTIFIER_HPP
