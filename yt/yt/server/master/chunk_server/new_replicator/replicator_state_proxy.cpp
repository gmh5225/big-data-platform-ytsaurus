#include "replicator_state.h"

#include <yt/yt/server/master/cell_master/bootstrap.h>
#include <yt/yt/server/master/cell_master/config_manager.h>

#include <yt/yt/server/master/chunk_server/chunk_manager.h>

#include <yt/yt/server/master/node_tracker_server/node_tracker.h>

namespace NYT::NChunkServer::NReplicator {

using namespace NCellMaster;

////////////////////////////////////////////////////////////////////////////////

class TReplicatorStateProxy
    : public IReplicatorStateProxy
{
public:
    explicit TReplicatorStateProxy(TBootstrap* bootstrap)
        : Bootstrap_(bootstrap)
    { }

    const IInvokerPtr& GetChunkInvoker(EChunkThreadQueue queue) const override
    {
        const auto& chunkManager = Bootstrap_->GetChunkManager();
        return chunkManager->GetChunkInvoker(queue);
    }

    const TDynamicClusterConfigPtr& GetDynamicConfig() const override
    {
        const auto& configManager = Bootstrap_->GetConfigManager();
        return configManager->GetConfig();
    }

    std::vector<NChunkServer::TMedium*> GetMedia() const override
    {
        const auto& chunkManager = Bootstrap_->GetChunkManager();
        const auto& mediaMap = chunkManager->Media();

        std::vector<NChunkServer::TMedium*> media;
        media.reserve(mediaMap.size());
        for (const auto& [mediumId, medium] : mediaMap) {
            media.push_back(medium);
        }

        return media;
    }

    std::vector<NNodeTrackerServer::TDataCenter*> GetDataCenters() const override
    {
        const auto& nodeTracker = Bootstrap_->GetNodeTracker();
        const auto& dataCenterMap = nodeTracker->DataCenters();

        std::vector<NNodeTrackerServer::TDataCenter*> dataCenters;
        dataCenters.reserve(dataCenterMap.size());
        for (const auto& [dataCenterId, dataCenter] : dataCenterMap) {
            dataCenters.push_back(dataCenter);
        }

        return dataCenters;
    }

    std::vector<NNodeTrackerServer::TRack*> GetRacks() const override
    {
        const auto& nodeTracker = Bootstrap_->GetNodeTracker();
        const auto& rackMap = nodeTracker->Racks();

        std::vector<NNodeTrackerServer::TRack*> racks;
        racks.reserve(rackMap.size());
        for (const auto& [rackId, rack] : rackMap) {
            racks.push_back(rack);
        }

        return racks;
    }

    bool CheckThreadAffinity() const override
    {
        return true;
    }

private:
    TBootstrap* const Bootstrap_;
};

////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<IReplicatorStateProxy> CreateReplicatorStateProxy(TBootstrap* bootstrap)
{
    return std::make_unique<TReplicatorStateProxy>(bootstrap);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NChunkServer::NReplicator