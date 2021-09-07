#include "job_resources_with_quota.h"
#include "config.h"

#include <yt/yt/ytlib/node_tracker_client/helpers.h>

#include <yt/yt/ytlib/scheduler/proto/job.pb.h>

#include <yt/yt/core/ytree/fluent.h>

namespace NYT::NScheduler {

using namespace NYson;
using namespace NYTree;
using namespace NNodeTrackerClient;
using namespace NNodeTrackerClient::NProto;
using namespace NProfiling;

using std::round;

////////////////////////////////////////////////////////////////////////////////

TDiskQuota::operator bool() const
{
    return !DiskSpacePerMedium.empty() || DiskSpaceWithoutMedium;
}

void TDiskQuota::Persist(const TStreamPersistenceContext& context)
{
    using NYT::Persist;
    Persist(context, DiskSpacePerMedium);
    Persist(context, DiskSpaceWithoutMedium);
}

TDiskQuota CreateDiskQuota(i32 mediumIndex, i64 diskSpace)
{
    TDiskQuota result;
    result.DiskSpacePerMedium.emplace(mediumIndex, diskSpace);
    return result;
}

TDiskQuota CreateDiskQuotaWithoutMedium(i64 diskSpace)
{
    TDiskQuota result;
    result.DiskSpaceWithoutMedium = diskSpace;
    return result;
}

TDiskQuota  operator - (const TDiskQuota& quota)
{
    TDiskQuota result;
    if (quota.DiskSpaceWithoutMedium) {
        result.DiskSpaceWithoutMedium = -*quota.DiskSpaceWithoutMedium;
    }
    for (auto [key, value] : quota.DiskSpacePerMedium) {
        result.DiskSpacePerMedium[key] = -value;
    }
    return result;
}

TDiskQuota  operator + (const TDiskQuota& lhs, const TDiskQuota& rhs)
{
    TDiskQuota result;
    result.DiskSpaceWithoutMedium = lhs.DiskSpaceWithoutMedium.value_or(0) + rhs.DiskSpaceWithoutMedium.value_or(0);
    if (*result.DiskSpaceWithoutMedium == 0) {
        result.DiskSpaceWithoutMedium = std::nullopt;
    }
    for (auto [key, value] : lhs.DiskSpacePerMedium) {
        result.DiskSpacePerMedium[key] += value;
    }
    for (auto [key, value] : rhs.DiskSpacePerMedium) {
        result.DiskSpacePerMedium[key] += value;
    }
    return result;
}

TDiskQuota& operator += (TDiskQuota& lhs, const TDiskQuota& rhs)
{
    lhs = lhs + rhs;
    return lhs;
}

TDiskQuota  operator - (const TDiskQuota& lhs, const TDiskQuota& rhs)
{
    TDiskQuota result;
    result.DiskSpaceWithoutMedium = lhs.DiskSpaceWithoutMedium.value_or(0) - rhs.DiskSpaceWithoutMedium.value_or(0);
    if (*result.DiskSpaceWithoutMedium == 0) {
        result.DiskSpaceWithoutMedium = std::nullopt;
    }
    for (auto [key, value] : lhs.DiskSpacePerMedium) {
        result.DiskSpacePerMedium[key] += value;
    }
    for (auto [key, value] : rhs.DiskSpacePerMedium) {
        result.DiskSpacePerMedium[key] -= value;
    }
    return result;
}

TDiskQuota& operator -= (TDiskQuota& lhs, const TDiskQuota& rhs)
{
    lhs = lhs - rhs;
    return lhs;
}

////////////////////////////////////////////////////////////////////////////////

TJobResourcesWithQuota::TJobResourcesWithQuota(const TJobResources& jobResources)
    : TJobResources(jobResources)
{ }

TJobResourcesWithQuota TJobResourcesWithQuota::Infinite()
{
    return TJobResourcesWithQuota(TJobResources::Infinite());
}

TJobResources TJobResourcesWithQuota::ToJobResources() const
{
    return *this;
}

void TJobResourcesWithQuota::Persist(const TStreamPersistenceContext& context)
{
    using NYT::Persist;
    TJobResources::Persist(context);
    Persist(context, DiskQuota_);
}

////////////////////////////////////////////////////////////////////////////////

bool Dominates(const TJobResourcesWithQuota& lhs, const TJobResourcesWithQuota& rhs)
{
    bool result =
    #define XX(name, Name) lhs.Get##Name() >= rhs.Get##Name() &&
        ITERATE_JOB_RESOURCES(XX)
    #undef XX
    true;
    auto rhsDiskQuota = rhs.GetDiskQuota();
    for (auto [mediumIndex, diskSpace] : lhs.GetDiskQuota().DiskSpacePerMedium) {
        auto it = rhsDiskQuota.DiskSpacePerMedium.find(mediumIndex);
        if (it != rhsDiskQuota.DiskSpacePerMedium.end() && diskSpace < it->second) {
            return false;
        }
    }
    return result;
}

bool CanSatisfyDiskQuotaRequests(
    std::vector<i64> availableDiskSpacePerLocation,
    std::vector<i64> diskSpaceRequests)
{
    std::sort(availableDiskSpacePerLocation.begin(), availableDiskSpacePerLocation.end());
    std::sort(diskSpaceRequests.begin(), diskSpaceRequests.end(), std::greater<i64>());

    for (i64 diskSpace : diskSpaceRequests) {
        auto it = std::lower_bound(availableDiskSpacePerLocation.begin(), availableDiskSpacePerLocation.end(), diskSpace);
        if (it == availableDiskSpacePerLocation.end()) {
            return false;
        }
        *it -= diskSpace;
        while (it != availableDiskSpacePerLocation.begin() && *it < *(it - 1)) {
            std::swap(*it, *(it - 1));
            --it;
        }
    }

    return true;
}

bool CanSatisfyDiskQuotaRequest(
    const std::vector<i64>& availableDiskSpacePerLocation,
    i64 diskSpaceRequest)
{
    for (i64 availableDiskSpace : availableDiskSpacePerLocation) {
        if (diskSpaceRequest <= availableDiskSpace) {
            return true;
        }
    }
    return false;
}

bool HasLocationWithDefaultMedium(const NNodeTrackerClient::NProto::TDiskResources& diskResources)
{
    bool hasLocationWithDefaultMedium = false;
    for (const auto& diskLocationResources : diskResources.disk_location_resources()) {
        if (diskLocationResources.medium_index() == diskResources.default_medium_index()) {
            hasLocationWithDefaultMedium = true;
        }
    }
    return hasLocationWithDefaultMedium;
}

bool CanSatisfyDiskQuotaRequest(
    const NNodeTrackerClient::NProto::TDiskResources& diskResources,
    TDiskQuota diskQuotaRequest)
{
    THashMap<int, std::vector<i64>> availableDiskSpacePerMedium;
    for (const auto& diskLocationResources : diskResources.disk_location_resources()) {
        availableDiskSpacePerMedium[diskLocationResources.medium_index()].push_back(
            diskLocationResources.limit() - diskLocationResources.usage());
    }
    for (auto [mediumIndex, diskSpace] : diskQuotaRequest.DiskSpacePerMedium) {
        if (!CanSatisfyDiskQuotaRequest(availableDiskSpacePerMedium[mediumIndex], diskSpace)) {
            return false;
        }
    }

    if (!diskQuotaRequest && !HasLocationWithDefaultMedium(diskResources)) {
        return false;
    }

    return true;
}

bool CanSatisfyDiskQuotaRequests(
    const NNodeTrackerClient::NProto::TDiskResources& diskResources,
    const std::vector<TDiskQuota>& diskQuotaRequests)
{
    THashMap<int, std::vector<i64>> availableDiskSpacePerMedium;
    for (const auto& diskLocationResources : diskResources.disk_location_resources()) {
        availableDiskSpacePerMedium[diskLocationResources.medium_index()].push_back(
            diskLocationResources.limit() - diskLocationResources.usage());
    }

    THashMap<int, std::vector<i64>> diskSpaceRequestsPerMedium;
    bool hasEmptyDiskRequest = false;
    for (const auto& diskQuotaRequest : diskQuotaRequests) {
        for (auto [mediumIndex, diskSpace] : diskQuotaRequest.DiskSpacePerMedium) {
            diskSpaceRequestsPerMedium[mediumIndex].push_back(diskSpace);
        }
        if (diskQuotaRequest.DiskSpaceWithoutMedium) {
            diskSpaceRequestsPerMedium[diskResources.default_medium_index()].push_back(*diskQuotaRequest.DiskSpaceWithoutMedium);
        }
        if (!diskQuotaRequest && !diskQuotaRequest.DiskSpaceWithoutMedium) {
            hasEmptyDiskRequest = true;
        }
    }

    if (hasEmptyDiskRequest && !HasLocationWithDefaultMedium(diskResources)) {
        return false;
    }

    for (const auto& [mediumIndex, diskSpaceRequests] : diskSpaceRequestsPerMedium) {
        if (!CanSatisfyDiskQuotaRequests(availableDiskSpacePerMedium[mediumIndex], diskSpaceRequests)) {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NScheduler
