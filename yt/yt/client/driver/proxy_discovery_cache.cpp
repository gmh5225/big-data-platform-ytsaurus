#include "proxy_discovery_cache.h"

#include "private.h"

#include <yt/yt/client/api/client.h>

#include <yt/yt/client/api/rpc_proxy/address_helpers.h>

#include <yt/yt/core/misc/async_expiring_cache.h>

#include <util/digest/multi.h>

namespace NYT::NDriver {

using namespace NYPath;
using namespace NYTree;
using namespace NYson;
using namespace NApi;
using namespace NConcurrency;
using namespace NApi::NRpcProxy;

////////////////////////////////////////////////////////////////////////////////

bool TProxyDiscoveryRequest::operator==(const TProxyDiscoveryRequest& other) const
{
    return
        Type == other.Type &&
        Role == other.Role &&
        AddressType == other.AddressType &&
        NetworkName == other.NetworkName;
}

bool TProxyDiscoveryRequest::operator!=(const TProxyDiscoveryRequest& other) const
{
    return !(*this == other);
}

TProxyDiscoveryRequest::operator size_t() const
{
    return MultiHash(
        Type,
        Role,
        AddressType,
        NetworkName);
}

////////////////////////////////////////////////////////////////////////////////

void FormatValue(TStringBuilderBase* builder, const TProxyDiscoveryRequest& request, TStringBuf /*spec*/)
{
    builder->AppendFormat("{Type: %v, Role: %v, AddressType: %v, NetworkName: %v}",
        request.Type,
        request.Role,
        request.AddressType,
        request.NetworkName);
}

TString ToString(const TProxyDiscoveryRequest& request)
{
    return ToStringViaBuilder(request);
}

////////////////////////////////////////////////////////////////////////////////

class TProxyDiscoveryCache
    : public TAsyncExpiringCache<TProxyDiscoveryRequest, TProxyDiscoveryResponse>
    , public IProxyDiscoveryCache
{
public:
    TProxyDiscoveryCache(
        TAsyncExpiringCacheConfigPtr config,
        IClientPtr client)
        : TAsyncExpiringCache(
            std::move(config),
            DriverLogger.WithTag("Cache: ProxyDiscovery"))
        , Client_(std::move(client))
    { }

    TFuture<TProxyDiscoveryResponse> Discover(
        const TProxyDiscoveryRequest& request) override
    {
        return Get(request);
    }

private:
    const IClientPtr Client_;

    TFuture<TProxyDiscoveryResponse> DoGet(
        const TProxyDiscoveryRequest& request,
        bool /*isPeriodicUpdate*/) noexcept override
    {
        TGetNodeOptions options;
        options.ReadFrom = EMasterChannelKind::LocalCache;
        options.SuppressUpstreamSync = true;
        options.SuppressTransactionCoordinatorSync = true;
        options.Attributes = {BannedAttributeName, RoleAttributeName, AddressesAttributeName};

        auto path = GetProxyRegistryPath(request.Type);

        return Client_->GetNode(path, options)
            .Apply(BIND([=] (const TYsonString& yson) {
                TProxyDiscoveryResponse response;
                for (const auto& [proxyAddress, proxyNode] : ConvertTo<THashMap<TString, IMapNodePtr>>(yson)) {
                    if (!proxyNode->FindChild(AliveNodeName)) {
                        continue;
                    }

                    if (proxyNode->Attributes().Get(BannedAttributeName, false)) {
                        continue;
                    }

                    if (proxyNode->Attributes().Get<TString>(RoleAttributeName, DefaultProxyRole) != request.Role) {
                        continue;
                    }

                    auto addresses = proxyNode->Attributes().Get<TProxyAddressMap>(AddressesAttributeName, {});
                    auto address = GetAddressOrNull(addresses, request.AddressType, request.NetworkName);

                    if (address) {
                        response.Addresses.push_back(*address);
                    } else {
                        // COMPAT(verytable): Drop it after all rpc proxies migrate to 22.3.
                        if (!proxyNode->Attributes().Contains(AddressesAttributeName)) {
                            response.Addresses.push_back(proxyAddress);
                        }
                    }
                }
                return response;
            }).AsyncVia(Client_->GetConnection()->GetInvoker()));
    }

    static TYPath GetProxyRegistryPath(EProxyType type)
    {
        switch (type) {
            case EProxyType::Rpc:
                return RpcProxiesPath;
            case EProxyType::Grpc:
                return GrpcProxiesPath;
            default:
                THROW_ERROR_EXCEPTION("Proxy type %Qlv is not supported",
                    type);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

IProxyDiscoveryCachePtr CreateProxyDiscoveryCache(
    TAsyncExpiringCacheConfigPtr config,
    IClientPtr client)
{
    return New<TProxyDiscoveryCache>(
        std::move(config),
        std::move(client));
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NDriver
