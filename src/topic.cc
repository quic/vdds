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

#include <stdexcept>

#include <hogl/area.hpp>
#include <hogl/post.hpp>
#include <hogl/fmt/format.h>

#include "vdds/topic.hpp"

namespace vdds {

topic::topic(const std::string& domain, const std::string& name, const std::string& data_type) :
	_domain(domain), _name(name), _data_type(data_type),
	_next_seqno(0),
	_cache_ptr(new cache()),
	_cache_refcnt(0)
{ 
	_area = hogl::add_area(fmt::format("VDDS{}{}", _domain.empty() ? "" : "-", _domain).c_str());
	if (!_area)
		throw std::logic_error("failed to create log area");
}

topic::~topic()
{
	cache* c = _cache_ptr;
	delete c;
}

// Create a copy of the cache
topic::cache* topic::cache_copy()
{
	const cache* cc = _cache_ptr.load();
	return new cache(*cc);
}

void topic::cache_swap(cache *nc)
{
	const cache* cc = _cache_ptr.load();

	// Replace the cache pointer.
	_cache_ptr.exchange(nc);

	hogl::post(_area, _area->DEBUG, hogl::arg_gstr("%s swapped cache: %p to %p"), _name, (void *) cc, (void *) nc);

	// At this point all new topic::push() operations will use the new cache.
	// To release the old cache we have to wait for the refcount to drop to zero.
	while (_cache_refcnt.load() != 0);

	hogl::post(_area, _area->DEBUG, hogl::arg_gstr("%s deleting old cache: %p"), _name, (void *) cc);

	delete cc;
}

sub_queue* topic::subscribe(const std::string& name, unsigned int qsize, notifier* ntfr)
{
	sub_queue *q = new sub_queue(name, this->name(), _data_type, qsize, ntfr);

	std::unique_lock<std::shared_timed_mutex> lock(_mutex); // read-write / exclusive mode

	cache* c = cache_copy();
	c->subs.push_back(q);
	hogl::post(_area, _area->DEBUG, hogl::arg_gstr("%s add-sub: %s queue %p qcap %u notifier %s"),
			_name, q->name(), q, q->capacity(), ntfr ? ntfr->name() : std::string("null") );
	cache_swap(c);

	return q;
}

void topic::unsubscribe(sub_queue *q)
{
	std::unique_lock<std::shared_timed_mutex> lock(_mutex); // read-write / exclusive mode

	cache* c = cache_copy();
	auto i = std::find(c->subs.begin(), c->subs.end(), q);
	if (i != c->subs.end())
		c->subs.erase(i);
	hogl::post(_area, _area->DEBUG, hogl::arg_gstr("%s del-sub: %s queue %p"), _name, q->name(), q);
	cache_swap(c);

	delete q;
}

pub_handle* topic::publish(const std::string& name)
{
	pub_handle* p = new pub_handle(name, this->name());

	std::unique_lock<std::shared_timed_mutex> lock(_mutex); // read-write / exclusive mode

	cache* c = cache_copy();
	c->pubs.push_back(p);
	hogl::post(_area, _area->DEBUG, hogl::arg_gstr("%s add-pub: %s handle %p"), _name, p->name(), p);
	cache_swap(c);

	return p;
}

void topic::unpublish(pub_handle* p)
{
	std::unique_lock<std::shared_timed_mutex> lock(_mutex); // read-write / exclusive mode

	cache* c = cache_copy();
	auto i = std::find(c->pubs.begin(), c->pubs.end(), p);
	if (i != c->pubs.end())
		c->pubs.erase(i);
	hogl::post(_area, _area->DEBUG, hogl::arg_gstr("%s del-pub: %s handle %p"), _name, p->name(), p);
	cache_swap(c);

	delete p;
}

void topic::dump()
{
	std::shared_lock<std::shared_timed_mutex> lock(_mutex); // read-only / shared mode

	const cache* c = _cache_ptr;

	hogl::post(_area, _area->INFO, hogl::arg_gstr("%s nsubs %u npubs %u seqno %llu"),
			_name, c->subs.size(), c->pubs.size(), (uint64_t) _next_seqno);

	for (auto &s : c->subs) {
		hogl::post(_area, _area->INFO, hogl::arg_gstr("%s sub %s queue %p qcap %u qsize %u notifier %s pushes %llu drops %llu"),
				_name, s->name(), s, s->capacity(), s->size(),
				s->notifier() ? s->notifier()->name() : std::string("null"),
				s->push_count(), s->drop_count());
	}
	for (auto &p : c->pubs) {
		hogl::post(_area, _area->INFO, hogl::arg_gstr("%s pub %s (%p)"),
				_name, p->name(), p);
	}
}

void topic::query(query::topic_info& ti)
{
	std::shared_lock<std::shared_timed_mutex> lock(_mutex); // read-only / shared mode

	const cache* c = _cache_ptr;

	ti.name = _name;
	ti.data_type = _data_type; 
	ti.push_count = _next_seqno;
	ti.subs.resize(c->subs.size());
	ti.pubs.resize(c->pubs.size());

	for (unsigned i=0; i< c->subs.size(); i++) {
		ti.subs[i].name       = c->subs[i]->name();
		ti.subs[i].qcapacity  = c->subs[i]->capacity();
		ti.subs[i].qsize      = c->subs[i]->size();
		ti.subs[i].push_count = c->subs[i]->push_count();
		ti.subs[i].drop_count = c->subs[i]->drop_count();
	}

	for (unsigned i=0; i< c->pubs.size(); i++) {
		ti.pubs[i].name = c->pubs[i]->name();
	}
}

} // namespace vdds
