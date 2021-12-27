#pragma once

#include "private.h"

#include <yt/yt/ytlib/api/native/public.h>

#include <yt/yt/ytlib/monitoring/public.h>

#include <yt/yt/ytlib/node_tracker_client/public.h>

#include <yt/yt/ytlib/transaction_client/public.h>

#include <yt/yt/core/bus/public.h>

#include <yt/yt/core/concurrency/public.h>

#include <yt/yt/core/http/public.h>

#include <yt/yt/core/rpc/public.h>

#include <yt/yt/core/ytree/public.h>

namespace NYT::NQueueAgent {

////////////////////////////////////////////////////////////////////////////////

class TBootstrap
{
public:
    TBootstrap(TQueueAgentServerConfigPtr config, NYTree::INodePtr configNode);
    ~TBootstrap();

    void Run();

private:
    const TQueueAgentServerConfigPtr Config_;
    const NYTree::INodePtr ConfigNode_;

    NMonitoring::TMonitoringManagerPtr MonitoringManager_;
    NConcurrency::TActionQueuePtr ControlQueue_;
    IInvokerPtr ControlInvoker_;
    NBus::IBusServerPtr BusServer_;
    NRpc::IServerPtr RpcServer_;
    NHttp::IServerPtr HttpServer_;
    ICoreDumperPtr CoreDumper_;

    NApi::NNative::IConnectionPtr NativeConnection_;
    NApi::NNative::IClientPtr NativeClient_;

    TQueueAgentPtr QueueAgent_;

    void DoRun();
};

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NQueueAgent