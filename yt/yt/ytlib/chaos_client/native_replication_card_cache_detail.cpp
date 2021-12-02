#include "native_replication_card_cache_detail.h"
#include "chaos_node_service_proxy.h"

#include <yt/yt/ytlib/api/native/client.h>
#include <yt/yt/ytlib/api/native/connection.h>

#include <yt/yt/ytlib/cell_master_client/cell_directory.h>

#include <yt/yt/ytlib/node_tracker_client/channel.h>
#include <yt/yt/ytlib/node_tracker_client/node_addresses_provider.h>

#include <yt/yt/client/chaos_client/config.h>
#include <yt/yt/client/chaos_client/replication_card_serialization.h>

#include <yt/yt/client/table_client/public.h>

#include <yt/yt/core/misc/hash.h>
#include <yt/yt/core/misc/protobuf_helpers.h>

#include <yt/yt/core/rpc/balancing_channel.h>
#include <yt/yt/core/rpc/retrying_channel.h>
#include <yt/yt/core/rpc/config.h>

#include <yt/yt/core/ytree/fluent.h>

namespace NYT::NChaosClient {

using namespace NApi;

using namespace NConcurrency;
using namespace NRpc;
using namespace NTableClient;
using namespace NTabletClient;
using namespace NNodeTrackerClient;
using namespace NYTree;

using NNative::IClientPtr;
using NNative::IConnectionPtr;

using NYT::FromProto;

///////////////////////////////////////////////////////////////////////////////

class TReplicationCardCache
    : public IReplicationCardCache
    , public TAsyncExpiringCache<TReplicationCardCacheKey, TReplicationCardPtr>
{
public:
    TReplicationCardCache(
        TReplicationCardCacheConfigPtr config,
        IConnectionPtr connection,
        const NLogging::TLogger& logger);
    TFuture<TReplicationCardPtr> GetReplicationCard(const TReplicationCardCacheKey& key) override;
    TFuture<TReplicationCardPtr> DoGet(const TReplicationCardCacheKey& key, bool isPeriodicUpdate) noexcept override;
    void Clear() override;

    IChannelPtr GetChaosCacheChannel();

protected:
    class TGetSession;

    const TReplicationCardCacheConfigPtr Config_;
    const TWeakPtr<NNative::IConnection> Connection_;
    const IChannelPtr ChaosCacheChannel_;
    const NLogging::TLogger Logger;

    IChannelPtr CreateChaosCacheChannel(NNative::IConnectionPtr connection);
    IChannelPtr CreateChaosCacheChannelFromAddresses(
        IChannelFactoryPtr channelFactory,
        const std::vector<TString>& discoveredAddresses);
};

////////////////////////////////////////////////////////////////////////////////

class TReplicationCardCache::TGetSession
    : public TRefCounted
{
public:
    TGetSession(
        TReplicationCardCache* owner,
        const TReplicationCardCacheKey& key,
        const NLogging::TLogger& logger)
        : Owner_(owner)
        , Key_ (key)
        , Logger(logger)
    {
        Logger.WithTag("ReplicationCardToken: %v, CacheSessionId: %v",
            Key_.Token,
            TGuid::Create());
    }

    TReplicationCardPtr Run()
    {
        auto channel = Owner_->ChaosCacheChannel_;
        auto proxy = TChaosServiceProxy(channel);
        auto req = proxy.GetReplicationCard();
        ToProto(req->mutable_replication_card_token(), Key_.Token);
        req->set_request_coordinators(Key_.RequestCoordinators);
        req->set_request_replication_progress(Key_.RequestProgress);
        req->set_request_history(Key_.RequestHistory);

        auto rsp = WaitFor(req->Invoke())
            .ValueOrThrow();

        auto replicationCard = New<TReplicationCard>();

        FromProto(replicationCard.Get(), rsp->replication_card());

        YT_LOG_DEBUG("Got replication card (ReplicationCard: %v)",
            *replicationCard);

        return replicationCard;
    }

private:
    const TIntrusivePtr<TReplicationCardCache> Owner_;
    const TReplicationCardCacheKey Key_;

    const NLogging::TLogger Logger;
};

////////////////////////////////////////////////////////////////////////////////

TReplicationCardCache::TReplicationCardCache(
    TReplicationCardCacheConfigPtr config,
    NNative::IConnectionPtr connection,
    const NLogging::TLogger& logger)
    : TAsyncExpiringCache(config)
    , Config_(std::move(config))
    , Connection_(connection)
    , ChaosCacheChannel_(CreateChaosCacheChannel(std::move(connection)))
    , Logger(logger)
{ }

TFuture<TReplicationCardPtr> TReplicationCardCache::GetReplicationCard(const TReplicationCardCacheKey& key)
{
    return TAsyncExpiringCache::Get(key);
}

TFuture<TReplicationCardPtr> TReplicationCardCache::DoGet(const TReplicationCardCacheKey& key, bool /*isPeriodicUpdate*/) noexcept
{
    auto connection = Connection_.Lock();
    if (!connection) {
        auto error = TError("Unable to get replication card: сonnection terminated")
            << TErrorAttribute("replication_card_token", key.Token);
        return MakeFuture<TReplicationCardPtr>(error);
    }

    auto invoker = connection->GetInvoker();
    auto session = New<TGetSession>(this, key, Logger);

    return BIND(&TGetSession::Run, std::move(session))
        .AsyncVia(std::move(invoker))
        .Run();
}

void TReplicationCardCache::Clear()
{
    TAsyncExpiringCache::Clear();
}

IChannelPtr TReplicationCardCache::CreateChaosCacheChannel(NNative::IConnectionPtr connection)
{
    auto channelFactory = connection->GetChannelFactory();
    auto endpointDescription = TString("ChaosCache");
    auto endpointAttributes = ConvertToAttributes(BuildYsonStringFluently()
        .BeginMap()
            .Item("chaos_cache").Value(true)
        .EndMap());
    auto channel = CreateBalancingChannel(
        Config_,
        std::move(channelFactory),
        std::move(endpointDescription),
        std::move(endpointAttributes));
    channel = CreateRetryingChannel(
        Config_,
        std::move(channel));
    return channel;
}

////////////////////////////////////////////////////////////////////////////////

IReplicationCardCachePtr CreateNativeReplicationCardCache(
    TReplicationCardCacheConfigPtr config,
    IConnectionPtr connection,
    const NLogging::TLogger& logger)
{
    return New<TReplicationCardCache>(
        std::move(config),
        std::move(connection),
        logger);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NChaosClient