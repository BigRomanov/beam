#include "reactor.h"
#include "tcpstream.h"
#include "coarsetimer.h"
#include "utility/config.h"
#include "utility/helpers.h"
#include <assert.h>
#include <stdlib.h>

#define LOG_VERBOSE_ENABLED 1
#include "utility/logger.h"

namespace beam { namespace io {

Reactor::Ptr Reactor::create() {
    struct make_shared_enabler : public Reactor {};
    Reactor::Ptr ptr = std::make_shared<make_shared_enabler>();
    ErrorCode errorCode = ptr->initialize();
    IO_EXCEPTION_IF(errorCode);
    return ptr;
}

Reactor::Reactor() :
    _handlePool(config().get_int("io.handle_pool_size", 256, 0, 65536)),
    _connectRequestsPool(config().get_int("io.connect_pool_size", 16, 0, 512)),
    _writeRequestsPool(config().get_int("io.write_pool_size", 256, 0, 65536)),
    _shutdownRequestsPool(config().get_int("io.shutdown_pool_size", 16, 0, 512))
{
    memset(&_loop,0,sizeof(uv_loop_t));
    memset(&_stopEvent, 0, sizeof(uv_async_t));
}

ErrorCode Reactor::initialize() {
    _creatingInternalObjects=true;

    ErrorCode errorCode = (ErrorCode)uv_loop_init(&_loop);
    if (errorCode != 0) {
        LOG_ERROR() << "cannot initialize uv loop, error=" << errorCode;
        return errorCode;
    }

    _loop.data = this;

    errorCode = (ErrorCode)uv_async_init(&_loop, &_stopEvent, [](uv_async_t* handle) { uv_stop(handle->loop); });
    if (errorCode != 0) {
        uv_loop_close(&_loop);
        LOG_ERROR() << "cannot initialize loop stop event, error=" << errorCode;
        return errorCode;
    }
    _stopEvent.data = this;

    _connectTimer = CoarseTimer::create(
        shared_from_this(),
        config().get_int("io.connect_timer_resolution", 1000, 1, 60000),
        BIND_THIS_MEMFN(connect_timeout_callback)
    );

    _creatingInternalObjects=false;
    return EC_OK;
}

Reactor::~Reactor() {
    LOG_VERBOSE() << ".";

    if (!_loop.data) {
        LOG_DEBUG() << "loop wasn't initialized";
        return;
    }

    if (_stopEvent.data)
        uv_close((uv_handle_t*)&_stopEvent, 0);

    _connectTimer.reset();

    for (auto& cr : _connectRequests) {
        uv_handle_t* h = (uv_handle_t*)(cr.second.request->handle);
        async_close(h);
    }

    for (uv_shutdown_t* sr : _shutdownRequests) {
        uv_handle_t* h = (uv_handle_t*)(sr->handle);
        async_close(h);
    }

    // run one cycle to release all closing handles
    uv_run(&_loop, UV_RUN_NOWAIT);

    if (uv_loop_close(&_loop) == UV_EBUSY) {
        LOG_DEBUG() << "closing unclosed handles";
        uv_walk(
            &_loop,
            [](uv_handle_t* handle, void*) {
                if (!uv_is_closing(handle)) {
                    handle->data = 0;
                    uv_close(handle, 0);
                }
            },
            0
        );

        // once more
        uv_run(&_loop, UV_RUN_NOWAIT);
        uv_loop_close(&_loop);
    }
}

void Reactor::run() {
    if (!_loop.data) {
        LOG_DEBUG() << "loop wasn't initialized";
        return;
    }

    // NOTE: blocks
    uv_run(&_loop, UV_RUN_DEFAULT);
}

void Reactor::stop() {
    int errorCode = uv_async_send(&_stopEvent);
    if (errorCode != 0) {
        LOG_DEBUG() << "cannot post stop signal to event loop";
    }
}

ErrorCode Reactor::init_object(ErrorCode errorCode, Reactor::Object* o, uv_handle_t* h) {
    if (errorCode != 0) {
        _handlePool.release(h);
        return errorCode;
    }
    h->data = o;
    if (!_creatingInternalObjects) {
        o->_reactor = shared_from_this();
    }
    o->_handle = h;
    return EC_OK;
}

ErrorCode Reactor::init_asyncevent(Reactor::Object* o, uv_async_cb cb) {
    assert(o);
    assert(cb);

    uv_handle_t* h = _handlePool.alloc();
    ErrorCode errorCode = (ErrorCode)uv_async_init(
        &_loop,
        (uv_async_t*)h,
        cb
    );
    return init_object(errorCode, o, h);
}

ErrorCode Reactor::init_timer(Reactor::Object* o) {
    assert(o);

    uv_handle_t* h = _handlePool.alloc();
    ErrorCode errorCode = (ErrorCode)uv_timer_init(&_loop, (uv_timer_t*)h);
    return init_object(errorCode, o, h);
}

ErrorCode Reactor::start_timer(Reactor::Object* o, unsigned intervalMsec, bool isPeriodic, uv_timer_cb cb) {
    assert(o);
    assert(cb);
    assert(o->_handle && o->_handle->type == UV_TIMER);

    return (ErrorCode)uv_timer_start(
        (uv_timer_t*)o->_handle,
        cb,
        intervalMsec,
        isPeriodic ? intervalMsec : 0
    );
}

void Reactor::cancel_timer(Object* o) {
    assert(o);

    uv_handle_t* h = o->_handle;
    if (h) {
        assert(h->type == UV_TIMER);
        if (uv_is_active(h)) {
            if (uv_timer_stop((uv_timer_t*)h) != 0) {
                LOG_DEBUG() << "cannot stop timer";
            }
        }
    }
}

ErrorCode Reactor::init_tcpserver(Object* o, Address bindAddress, uv_connection_cb cb) {
    assert(o);
    assert(cb);

    uv_handle_t* h = _handlePool.alloc();
    ErrorCode errorCode = (ErrorCode)uv_tcp_init(&_loop, (uv_tcp_t*)h);
    if (init_object(errorCode, o, h) != EC_OK) {
        return errorCode;
    }

    sockaddr_in addr;
    bindAddress.fill_sockaddr_in(addr);

    errorCode = (ErrorCode)uv_tcp_bind((uv_tcp_t*)h, (const sockaddr*)&addr, 0);
    if (errorCode != 0) {
        return errorCode;
    }

    errorCode = (ErrorCode)uv_listen(
        (uv_stream_t*)h,
        config().get_int("io.tcp_listen_backlog", 32, 5, 2000),
        cb
    );

    return errorCode;
}

ErrorCode Reactor::init_tcpstream(Object* o) {
    assert(o);

    uv_handle_t* h = _handlePool.alloc();
    ErrorCode errorCode = (ErrorCode)uv_tcp_init(&_loop, (uv_tcp_t*)h);
    return init_object(errorCode, o, h);
}

ErrorCode Reactor::accept_tcpstream(Object* acceptor, Object* newConnection) {
    assert(acceptor->_handle);

    ErrorCode errorCode = init_tcpstream(newConnection);
    if (errorCode != 0) {
        return errorCode;
    }

    errorCode = (ErrorCode)uv_accept((uv_stream_t*)acceptor->_handle, (uv_stream_t*)newConnection->_handle);
    if (errorCode != 0) {
        newConnection->async_close();
    }

    return errorCode;
}

void Reactor::shutdown_tcpstream(Object* o, BufferChain&& unsent) {
    assert(o);
    uv_handle_t* h = o->_handle;
    if (!h) {
        // already closed
        return;
    }
    h->data = 0;
    assert(o->_reactor.get() == this);
    uv_shutdown_t* req = _shutdownRequestsPool.alloc();
    req->data = this;
    _unsent[req] = std::move(unsent);
    uv_shutdown(
        req,
        (uv_stream_t*)h,
        [](uv_shutdown_t* req, int status) {
            if (status != 0 && status != UV_ECANCELED) {
                LOG_DEBUG() << "stream shutdown failed, code=" << error_str((ErrorCode)status);
            }
            Reactor* self = reinterpret_cast<Reactor*>(req->data);
            if (self) {
                self->async_close((uv_handle_t*&)req->handle);
                self->_shutdownRequests.erase(req);
                self->_shutdownRequestsPool.release(req);
                self->_unsent.erase(req);
            }
        }
    );
    o->_handle = 0;
    o->_reactor.reset();
}

Reactor::WriteRequest* Reactor::alloc_write_request() {
    WriteRequest* wr = _writeRequestsPool.alloc();
    wr->req.data = this;
    return wr;
}

void Reactor::release_write_request(Reactor::WriteRequest*& wr) {
    _writeRequestsPool.release(wr);
    wr = 0;
}

Result Reactor::tcp_connect(Address address, uint64_t tag, const ConnectCallback& callback, int timeoutMsec, Address bindTo) {
    assert(callback);
    assert(!address.empty());
    assert(_connectRequests.count(tag) == 0);

    if (!callback || address.empty() || _connectRequests.count(tag) > 0) {
        return make_unexpected(EC_EINVAL);
    }

    uv_handle_t* h = _handlePool.alloc();
    ErrorCode errorCode = (ErrorCode)uv_tcp_init(&_loop, (uv_tcp_t*)h);
    if (errorCode != 0) {
        _handlePool.release(h);
        return make_unexpected(errorCode);
    }

    if (!bindTo.empty()) {
        sockaddr_in bindAddr;
        bindTo.fill_sockaddr_in(bindAddr);

        errorCode = (ErrorCode)uv_tcp_bind((uv_tcp_t*)h, (const sockaddr*)&bindAddr, 0);
        if (errorCode != 0) {
            return make_unexpected(errorCode);
        }
    }
    h->data = this;

    ConnectContext& ctx = _connectRequests[tag];
    ctx.tag = tag;
    ctx.callback = callback;
    ctx.request = _connectRequestsPool.alloc();
    ctx.request->data = &ctx;

    sockaddr_in addr;
    address.fill_sockaddr_in(addr);

    errorCode = (ErrorCode)uv_tcp_connect(
        ctx.request,
        (uv_tcp_t*)h,
        (const sockaddr*)&addr,
        [](uv_connect_t* request, int errorCode) {
            assert(request);
            assert(request->data);
            assert(request->handle);
            assert(request->handle->loop);
            assert(request->handle->loop->data);
            Reactor* reactor = reinterpret_cast<Reactor*>(request->handle->loop->data);
            if (errorCode == UV_ECANCELED) {
                LOG_VERBOSE() << "callback on cancelled connect request=" << request;
                reactor->_cancelledConnectRequests.erase(request);
                reactor->_connectRequestsPool.release(request);
            } else {
                ConnectContext* ctx = reinterpret_cast<ConnectContext*>(request->data);
                reactor->connect_callback(ctx, (ErrorCode)errorCode);
            }
        }
    );

    if (!errorCode && timeoutMsec >= 0) {
        auto result = _connectTimer->set_timer(timeoutMsec, tag);
        if (!result) errorCode = result.error();
    }

    if (errorCode) {
        async_close(h);
        _connectRequests.erase(tag);
        return make_unexpected(errorCode);
    }

    return Ok();
}

void Reactor::connect_callback(Reactor::ConnectContext* ctx, ErrorCode errorCode) {
    assert(_connectRequests.count(ctx->tag)==1);
    uint64_t tag = ctx->tag;

    ConnectCallback callback = std::move(ctx->callback);
    uv_handle_t* h = (uv_handle_t*)ctx->request->handle;

    _connectRequestsPool.release(ctx->request);

    TcpStream::Ptr stream;
    if (errorCode == 0) {
        stream.reset(new TcpStream());
        stream->_handle = h;
        stream->_handle->data = stream.get();
        stream->_reactor = shared_from_this();
    } else {
        async_close(h);
    }

    _connectRequests.erase(tag);
    _connectTimer->cancel(tag);

    callback(tag, std::move(stream), errorCode);
}

void Reactor::connect_timeout_callback(uint64_t tag) {
    LOG_VERBOSE() << TRACE(tag);
    auto it = _connectRequests.find(tag);
    if (it != _connectRequests.end()) {
        ConnectCallback cb = it->second.callback;
        cancel_tcp_connect_impl(it);
        cb(tag, TcpStream::Ptr(), EC_ETIMEDOUT);
    }
}

void Reactor::cancel_tcp_connect(uint64_t tag) {
    LOG_VERBOSE() << TRACE(tag);
    auto it = _connectRequests.find(tag);
    if (it != _connectRequests.end()) {
        cancel_tcp_connect_impl(it);
        _connectTimer->cancel(tag);
    }
}

void Reactor::cancel_tcp_connect_impl(std::unordered_map<uint64_t, ConnectContext>::iterator& it) {
    uv_connect_t* request = it->second.request;
    uv_handle_t* h = (uv_handle_t*)request->handle;
    async_close(h);
    _cancelledConnectRequests.insert(request);
    _connectRequests.erase(it);
}

void Reactor::async_close(uv_handle_t*& handle) {
    LOG_VERBOSE() << TRACE(handle);

    if (!handle) return;
    handle->data = 0;

    if (!uv_is_closing(handle)) {
        uv_close(
            handle,
            [](uv_handle_s* handle) {
                assert(handle->loop);
                Reactor* reactor = reinterpret_cast<Reactor*>(handle->loop->data);
                assert(reactor);
                reactor->_handlePool.release(handle);
            }
        );
    }

    handle = 0;
}

static thread_local Reactor* s_pReactor = NULL;

Reactor::Scope::Scope(Reactor& r)
{
	m_pPrev = s_pReactor;
	s_pReactor = &r;
}

Reactor::Scope::~Scope()
{
	s_pReactor = m_pPrev;
}

Reactor& Reactor::get_Current()
{
	assert(s_pReactor); // core meltdown?
	return *s_pReactor;
}

}} //namespaces
