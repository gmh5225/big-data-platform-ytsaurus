#include "public.h"

namespace NYT::NTableClient {

////////////////////////////////////////////////////////////////////////////////

const TString PrimaryLockName("<primary>");

const TString SystemColumnNamePrefix("$");
const TString TableIndexColumnName = SystemColumnNamePrefix + "table_index";
const TString RowIndexColumnName = SystemColumnNamePrefix + "row_index";
const TString RangeIndexColumnName = SystemColumnNamePrefix + "range_index";
const TString TabletIndexColumnName = SystemColumnNamePrefix + "tablet_index";
const TString TimestampColumnName = SystemColumnNamePrefix + "timestamp";
const TString CumulativeDataWeightColumnName = SystemColumnNamePrefix + "cumulative_data_weight";

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NTableClient
