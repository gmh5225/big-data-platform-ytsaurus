#pragma once

#include "public.h"

#include <yt/yt/core/net/address.h>

#include <library/cpp/yt/memory/ref.h>

#include <util/network/init.h>

namespace NYT::NNet {

////////////////////////////////////////////////////////////////////////////////

//! Dialer establishes connection to a (resolved) network address.

struct IDialer
    : public virtual TRefCounted
{
    virtual TFuture<IConnectionPtr> Dial(const TNetworkAddress& remote) = 0;
};

DEFINE_REFCOUNTED_TYPE(IDialer);

IDialerPtr CreateDialer(
    TDialerConfigPtr config,
    NConcurrency::IPollerPtr poller,
    const NLogging::TLogger& logger);

////////////////////////////////////////////////////////////////////////////////

//! Async dialer notifies caller via callback for better performance.
using TAsyncDialerCallback = TCallback<void(const TErrorOr<SOCKET>&)>;

//! Dialer session interface.
//! Caller should hold a reference to a session until callback is called.
//! When caller releases the reference, session is dropped.
struct IAsyncDialerSession
    : public virtual TRefCounted
{
    //! Activate session. This method should be called no more than once.
    virtual void Dial() = 0;
};

DEFINE_REFCOUNTED_TYPE(IAsyncDialerSession);

//! Async dialer interface.
struct IAsyncDialer
    : public virtual TRefCounted
{
    //! Create dialer session to establish connection to a specific address.
    virtual IAsyncDialerSessionPtr CreateSession(
        const TNetworkAddress& address,
        TAsyncDialerCallback onFinished) = 0;
};

DEFINE_REFCOUNTED_TYPE(IAsyncDialer);

IAsyncDialerPtr CreateAsyncDialer(
    TDialerConfigPtr config,
    NConcurrency::IPollerPtr poller,
    const NLogging::TLogger& logger);

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NNet
