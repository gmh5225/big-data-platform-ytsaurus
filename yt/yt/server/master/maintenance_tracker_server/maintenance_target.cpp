#include "maintenance_target.h"

#include <yt/yt/server/master/cell_master/serialize.h>

namespace NYT::NMaintenanceTrackerServer {

using namespace NCellMaster;

////////////////////////////////////////////////////////////////////////////////

bool TNontemplateMaintenanceTargetBase::GetMaintenanceFlag(EMaintenanceType type) const
{
    return MaintenanceCounts_[type] > 0;
}

bool TNontemplateMaintenanceTargetBase::ClearMaintenanceFlag(EMaintenanceType type)
{
    if (MaintenanceCounts_[type] == 0) {
        return false;
    }

    EraseNodesIf(MaintenanceRequests_, [&] (const auto& request) {
        return request.second.Type == type;
    });
    MaintenanceCounts_[type] = 0;

    return true;
}

bool TNontemplateMaintenanceTargetBase::SetMaintenanceFlag(
    EMaintenanceType type,
    TString user,
    TInstant timestamp)
{
    auto wasAlreadySet = ClearMaintenanceFlag(type);

    TMaintenanceRequest newRequest = {
        .User = std::move(user),
        .Type = type,
        .Comment = "Generated by directly setting corresponding flag",
        .Timestamp = timestamp,
    };
    Y_UNUSED(AddMaintenance(GetBuiltinMaintenanceId(type), std::move(newRequest)));

    return !wasAlreadySet;
}

bool TNontemplateMaintenanceTargetBase::AddMaintenance(
    TMaintenanceId id,
    TMaintenanceRequest request)
{
    auto type = request.Type;
    EmplaceOrCrash(MaintenanceRequests_, id, std::move(request));
    return ++MaintenanceCounts_[type] == 1;
}

std::optional<EMaintenanceType> TNontemplateMaintenanceTargetBase::RemoveMaintenance(TMaintenanceId id)
{
    auto it = MaintenanceRequests_.find(id);
    YT_VERIFY(it != MaintenanceRequests_.end());

    auto type = it->second.Type;
    MaintenanceRequests_.erase(it);
    if (--MaintenanceCounts_[type] == 0) {
        return type;
    }
    return std::nullopt;
}

void TNontemplateMaintenanceTargetBase::Save(TSaveContext& context) const
{
    NYT::Save(context, MaintenanceRequests_);
}

void TNontemplateMaintenanceTargetBase::Load(TLoadContext& context)
{
    NYT::Load(context, MaintenanceRequests_);

    std::fill(MaintenanceCounts_.begin(), MaintenanceCounts_.end(), 0);
    for (const auto& [id, request] : MaintenanceRequests_) {
        ++MaintenanceCounts_[request.Type];
    }
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NMaintenanceTrackerServer