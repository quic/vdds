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

#include <boost/lockfree/queue.hpp>
#include <vector>

#include "test-skell.hpp"

// Shared buffer for use with vdds
struct shared_buffer : vdds::data::shared_t {
	uint64_t dma_handle; // opaque DMA handle
	uint8_t *data;
        uint32_t size;
	shared_buffer() : dma_handle(0), data(nullptr), size(0) {}
};

// Test data type with shared buffer
struct test_data : vdds::data {
	static const char* data_type;

	test_data() {};
	test_data(uint64_t ts, std::shared_ptr<shared_buffer> sb)
	{
		this->timestamp = ts;
		this->shared = std::move(sb);
	}

	shared_buffer* buffer() { return static_cast<shared_buffer *>(this->shared.get()); }
};
const char* test_data::data_type = "vdds.test.data";

// Simple example of a DMA buffer, buffer pool, and an allocator that supports optimized std::allocate_shared.
// DMA in this case is just an example of an 'external resource' that needs to be managed along with local 
// memory buffers and shared_ptrs to them.
class dma_pool;

// Shared DMA buffer allocated from the pool
struct dmabuf : public shared_buffer
{
	dma_pool *pool; // dmapool that owns the buffer

	dmabuf() = delete;
	dmabuf(const dmabuf&) = delete; // no copies
	dmabuf(dma_pool *pool);
	virtual ~dmabuf() = default;
};

// DMA buffer pool.
// Allocates HW DMA buffers, and local buffers with support for extra room required for 
// shared_ptr control block.
class dma_pool
{
public:
	// Strorage for local buffers that can be wrapped in shared_ptr
	struct swbuf {
		uint8_t data[128]; // sizeof(dmabuf) + sizeof(shared_ptr::control) rounded to 128
	};

	// HW buffer info
	struct hwbuf {
		uint64_t handle;
		uint8_t* data;
		uint32_t size;
	};

	const std::string& name() const { return _name; }
	size_t size() const { return _hwbuf.size(); }
	size_t data_size() const { return _hwbuf[0].size; }

	// Allocate n DMA buffers of data_size
	dma_pool(const std::string& pname, size_t n, size_t data_size = 64*1024) :
		_free(n), _name(pname), _hwbuf(n), _swbuf(n)
	{
		hogl::post(area, area->DEBUG, "new-dmapool: %s size %u data-size %u", name(), n, data_size);
		for (size_t i=0; i<n; i++) {
			auto& b  = _hwbuf[i];
			b.size   = data_size;
			b.data   = static_cast<uint8_t*>( std::malloc(data_size) ); // alloc DMA buf & handle
			b.handle = (uint64_t) b.data;
			hogl::post(area, area->DEBUG, hogl::arg_gstr("new-hwbuf : pool %s id %u data %p dma-handle 0x%x size %u"),
				name(), i, b.data, b.handle, b.size);
			_free.push(i);
		}
	}

	// Release DMA buffers back to HW
	~dma_pool()
	{
		hogl::post(area, area->DEBUG, "del-dmapool: %s size %u data-size %u", name(), size(), data_size());
		for (size_t i=0; i<_hwbuf.size(); i++) {
			auto& b = _hwbuf[i];
			hogl::post(area, area->DEBUG, hogl::arg_gstr("del-hwbuf : pool %s id %u data %p dma-handle 0x%x size %u"),
				name(), i, b.data, b.handle, b.size);
			std::free(b.data); b.data = nullptr; // release DMA buf & handle
			b.handle = 0;
		}
        }

	// Allocate buffer ID from the pool
	bool alloc(uint16_t &i) noexcept
	{
		if (!_free.pop(i)) return false;
		hwbuf& b = _hwbuf[i];
		hogl::post(area, area->DEBUG, hogl::arg_gstr("get-dmabuf id %u dma-handle 0x%lx"), i, b.handle);
		return true;
	}

	// Release buffer ID back to the pool
	void free(uint16_t i) noexcept
	{
		hwbuf& b = _hwbuf[i];
		hogl::post(area, area->DEBUG, hogl::arg_gstr("put-dmabuf id %u dma-handle 0x%lx"), i, b.handle);
		_free.push(i);
	}

	// Get a pointer to the HW buffer data by ID
	const hwbuf* id_to_hwbuf(uint16_t i) const { return &_hwbuf[i]; }

	// Get a pointer to the SW buffer by ID
	void* id_to_swbuf(uint16_t i, size_t sz) noexcept
	{
		assert(sz <= sizeof(swbuf));
		return static_cast<void*>(&_swbuf[i]);
	}

	// Get buffer ID from SW buffer pointer
	uint16_t swbuf_to_id(void* p) const { return static_cast<swbuf*>(p) - &_swbuf[0]; }

	// Get HW buffer pointer from SW buffer pointer
	const hwbuf* swbuf_to_hwbuf(void* p) const { return id_to_hwbuf(swbuf_to_id(p)); }

private:
	boost::lockfree::queue<uint16_t, boost::lockfree::fixed_sized<true>> _free;
	std::string        _name;
	std::vector<hwbuf> _hwbuf;
	std::vector<swbuf> _swbuf;
};

// Allocate used together with the dma_pool to allocate and manage HW and SW buffers with shared_ptrs
template <class T>
struct dma_allocator
{
	using value_type = T;

	dma_pool& pool;

	dma_allocator() = delete;
	dma_allocator(dma_pool& p) noexcept : pool(p) { }
	template <class U> dma_allocator(const dma_allocator<U>& base) noexcept : pool(base.pool) { }

	value_type* allocate(std::size_t /*unused*/)
	{
		uint16_t id;
		if (!pool.alloc(id)) throw std::bad_alloc();

		value_type* p = static_cast<value_type*>( pool.id_to_swbuf(id, sizeof(value_type)) );
		hogl::post(area, area->DEBUG, hogl::arg_gstr("dma-alloc %p id %u size %u (%u)"), p, id, sizeof(value_type), sizeof(dmabuf));
		return p;
	}

	void deallocate(value_type* p, std::size_t /*unused*/) noexcept
	{
		uint16_t id = pool.swbuf_to_id(p);
		hogl::post(area, area->DEBUG, hogl::arg_gstr("dma-dealloc %p id %u size %u (%u)"), p, id, sizeof(value_type), sizeof(dmabuf));
		pool.free(id);
	}
};

template <class T, class U>
bool operator==(dma_allocator<T> const& d1, dma_allocator<U> const& d2) noexcept
{
	return d1.pool == d2.pool;
}

template <class T, class U>
bool operator!=(dma_allocator<T> const& x, dma_allocator<U> const& y) noexcept
{
	return !(x == y);
}

// Init shared DMA buffer from DMA pool
dmabuf::dmabuf(dma_pool* p) : pool(p)
{
	const dma_pool::hwbuf* hb = p->swbuf_to_hwbuf(this);
	this->dma_handle = hb->handle;
	this->size = hb->size;
	this->data = hb->data;
}

bool run_test()
{
	hogl::post(area, area->INFO, "Starting test");

	static unsigned int const n_bufs = 4;

	// Allocate dmapool and base allocator
	dma_pool pool("framebuf", n_bufs, 64*1024);
	dma_allocator<dmabuf> alloc(pool);

	// Setup vdds pubsub
	vdds::domain vd("DEFAULT");

	vdds::pub<test_data> pub0(vd, "pub0", "/test/topic-X");
	vdds::sub<test_data> sub0(vd, "sub0", "/test/topic-X");

	// Cycle through all buffers
	for (unsigned int i=0; i<n_bufs; i++)
	{
		{
			test_data d(12345 /* ts */, std::allocate_shared<dmabuf, dma_allocator<dmabuf>, dma_pool*>(alloc, &pool));
			pub0.push(d);
		}

		{
			test_data d;
			sub0.pop(d);
			hogl::post(area, area->DEBUG, "use-count %u dma-handle 0x%x", d.shared.use_count(), d.buffer()->dma_handle);

			if (d.shared.use_count() != 1) {
				hogl::post(area, area->ERROR, "incorrect use-count %u expected 5", d.shared.use_count());
				return false;
			}
		}
	}

	// Add more subscribers
	vdds::sub<test_data> sub1(vd, "sub1", "/test/topic-X");
	vdds::sub<test_data> sub2(vd, "sub2", "/test/topic-X");
	vdds::sub<test_data> sub3(vd, "sub3", "/test/topic-X");
	vdds::sub<test_data> sub4(vd, "sub4", "/test/topic-X");

	vd.dump();

	// Cycle through all buffers
	for (unsigned int i=0; i<n_bufs; i++)
	{
		{
			test_data d(12346 /* ts */, std::allocate_shared<dmabuf, dma_allocator<dmabuf>, dma_pool*>(alloc, &pool));
			pub0.push(d);
		}
	}

	{
		test_data d;
		sub4.pop(d);
		hogl::post(area, area->DEBUG, "use-count %u dma-handle 0x%x", d.shared.use_count(), d.buffer()->dma_handle);

		if (d.shared.use_count() != 5) {
			hogl::post(area, area->ERROR, "incorrect use-count %u expected 5", d.shared.use_count());
			return false;
		}
	}

	hogl::post(area, area->INFO, "sizeof(vdds::data)) %u", sizeof(vdds::data));

	return true;
}
