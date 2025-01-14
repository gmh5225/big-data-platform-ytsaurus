#pragma once

#include "public.h"

#include <yt/yt/core/misc/cache_config.h>

#include <yt/yt/core/rpc/config.h>

namespace NYT::NChaosClient {

////////////////////////////////////////////////////////////////////////////////

class TReplicationCardCacheConfig
    : public TAsyncExpiringCacheConfig
    , public NRpc::TBalancingChannelConfig
    , public NRpc::TRetryingChannelConfig
{
    REGISTER_YSON_STRUCT(TReplicationCardCacheConfig)

    static void Register(TRegistrar)
    { }
};

DEFINE_REFCOUNTED_TYPE(TReplicationCardCacheConfig)

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NChaosClient

