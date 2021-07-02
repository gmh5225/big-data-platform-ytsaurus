#pragma once

#include "public.h"

#include <yt/yt/server/node/data_node/public.h>

#include <yt/yt/server/node/job_agent/public.h>

#include <yt/yt/server/lib/job_agent/public.h>

#include <yt/yt/server/lib/job_proxy/public.h>

#include <yt/yt/server/node/cluster_node/bootstrap.h>

namespace NYT::NExecAgent {

////////////////////////////////////////////////////////////////////////////////

struct IBootstrap
    : public virtual NClusterNode::IBootstrapBase
{
    virtual ~IBootstrap() = default;

    virtual void Initialize() = 0;
    virtual void Run() = 0;

    virtual const TGpuManagerPtr& GetGpuManager() const = 0;

    virtual const TSlotManagerPtr& GetSlotManager() const = 0;

    virtual const NJobAgent::TJobReporterPtr& GetJobReporter() const = 0;

    virtual const NJobProxy::TJobProxyConfigPtr& GetJobProxyConfigTemplate() const = 0;

    virtual const TChunkCachePtr& GetChunkCache() const = 0;

    virtual bool IsSimpleEnvironment() const = 0;

    virtual const IMasterConnectorPtr& GetMasterConnector() const = 0;

    virtual const NConcurrency::IThroughputThrottlerPtr& GetThrottler(EExecNodeThrottlerKind kind) const = 0;

    virtual const NProfiling::TSolomonExporterPtr& GetJobProxySolomonExporter() const = 0;
};

////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<IBootstrap> CreateBootstrap(NClusterNode::IBootstrap* bootstrap);

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NExecAgent