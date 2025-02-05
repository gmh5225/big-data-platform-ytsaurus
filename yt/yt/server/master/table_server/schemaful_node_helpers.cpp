#include "schemaful_node_helpers.h"

#include "public.h"
#include "table_manager.h"

#include <yt/yt/server/master/cell_master/config.h>

#include <yt/yt/library/heavy_schema_validation/schema_validation.h>

#include <yt/yt/client/table_client/schema.h>

#include <yt/yt/core/ytree/attributes.h>

namespace NYT::NTableServer {

using namespace NTableClient;
using namespace NYTree;
using namespace NObjectClient;
using namespace NCellMaster;

////////////////////////////////////////////////////////////////////////////////

std::pair<const TTableSchema*, TTableSchemaPtr> ProcessSchemafulNodeAttributes(
    const IAttributeDictionaryPtr& combinedAttributes,
    bool dynamic,
    bool chaos,
    const TDynamicClusterConfigPtr& dynamicConfig,
    const ITableManagerPtr& tableManager)
{
    auto tableSchema = combinedAttributes->FindAndRemove<TTableSchemaPtr>("schema");
    auto schemaId = combinedAttributes->FindAndRemove<TObjectId>("schema_id");

    if (dynamic && !chaos && !tableSchema && !schemaId) {
        THROW_ERROR_EXCEPTION("Either \"schema\" or \"schema_id\" must be specified for dynamic tables");
    }

    const TTableSchema* effectiveTableSchema = nullptr;
    if (schemaId) {
        auto* schemaById = tableManager->GetMasterTableSchemaOrThrow(*schemaId);
        if (tableSchema) {
            auto* schemaByYson = tableManager->FindMasterTableSchema(*tableSchema);
            if (IsObjectAlive(schemaByYson)) {
                if (schemaById != schemaByYson) {
                    THROW_ERROR_EXCEPTION("Both \"schema\" and \"schema_id\" specified and they refer to different schemas");
                }
            } else {
                if (*schemaById->AsTableSchema() != *tableSchema) {
                    THROW_ERROR_EXCEPTION("Both \"schema\" and \"schema_id\" specified and the schemas do not match");
                }
            }
        }
        effectiveTableSchema = schemaById->AsTableSchema().Get();
    } else if (tableSchema) {
        effectiveTableSchema = &*tableSchema;
    }

    if (effectiveTableSchema) {
        // NB: Sorted dynamic tables contain unique keys, set this for user.
        if (dynamic && effectiveTableSchema->IsSorted() && !effectiveTableSchema->GetUniqueKeys()) {
            tableSchema = effectiveTableSchema->ToUniqueKeys();
            effectiveTableSchema = &*tableSchema;
        }

        if (effectiveTableSchema->HasNontrivialSchemaModification()) {
            THROW_ERROR_EXCEPTION("Cannot create table with nontrivial schema modification");
        }

        ValidateTableSchemaUpdate(TTableSchema(), *effectiveTableSchema, dynamic, true);

        if (!dynamicConfig->EnableDescendingSortOrder || (dynamic && !dynamicConfig->EnableDescendingSortOrderDynamic)) {
            ValidateNoDescendingSortOrder(*effectiveTableSchema);
        }

        if (!dynamicConfig->EnableTableColumnRenaming) {
            ValidateNoRenamedColumns(*effectiveTableSchema);
        }
    }

    return {effectiveTableSchema, std::move(tableSchema)};
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NTableServer
