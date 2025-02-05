#pragma once

#include "public.h"

#include <yt/yt/server/master/cell_master/public.h>

#include <yt/yt/core/rpc/public.h>

namespace NYT::NChunkServer {

////////////////////////////////////////////////////////////////////////////////

NRpc::IServicePtr CreateChunkService(NCellMaster::TBootstrap* boostrap);

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NChunkServer
