#pragma once

#include <yt/yt/client/table_client/public.h>

namespace NYT::NSequoiaClient {

////////////////////////////////////////////////////////////////////////////////

DEFINE_ENUM(ESequoiaTable,
    ((ChunkMetaExtensions) (0))
);

////////////////////////////////////////////////////////////////////////////////

DECLARE_REFCOUNTED_STRUCT(ITableDescriptor)

DECLARE_REFCOUNTED_CLASS(TChunkMetaExtensionsTableDescriptor)

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NSequoiaClient