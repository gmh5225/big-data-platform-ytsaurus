#include "schema_dictionary.h"

#include <yt/yt_proto/yt/client/table_chunk_format/proto/chunk_meta.pb.h>

#include <yt/yt/core/misc/protobuf_helpers.h>

#include <library/cpp/yt/farmhash/farm_hash.h>

namespace NYT::NTableClient {

////////////////////////////////////////////////////////////////////////////////

int TSchemaDictionary::GetIdOrRegisterTable(const TTableSchema& table)
{
    TTableSchemaInternal tableInternal;
    tableInternal.set_unique_keys(table.GetUniqueKeys());
    tableInternal.set_strict(table.GetStrict());
    for (const auto& column : table.Columns()) {
        int id = GetIdOrRegisterColumn(column);
        tableInternal.add_columns(id);
    }

    int newId = TableInternalToId_.size();
    auto result = TableInternalToId_.insert({tableInternal, newId});
    if (result.second) {
        IdToTableInternal_.push_back(tableInternal);
        IdToTable_.push_back(New<TTableSchema>(table));
        return newId;
    } else {
        return result.first->second;
    }
}

int TSchemaDictionary::GetIdOrRegisterColumn(const TColumnSchema& column)
{
    int newId = ColumnToId_.size();
    auto result = ColumnToId_.insert({column, newId});
    if (result.second) {
        IdToColumn_.emplace_back(column);
        return newId;
    } else {
        return result.first->second;
    }
}

const TTableSchemaPtr& TSchemaDictionary::GetTable(int id) const
{
    YT_VERIFY(id >= 0 && id < std::ssize(IdToTable_));
    return IdToTable_[id];
}

const TColumnSchema& TSchemaDictionary::GetColumn(int id) const
{
    YT_VERIFY(id >= 0 && id < std::ssize(IdToColumn_));
    return IdToColumn_[id];
}

////////////////////////////////////////////////////////////////////////////////

size_t TSchemaDictionary::THashInternal::operator()(const TTableSchemaInternal& tableSchema) const
{
    return MultiHash(
        tableSchema.strict(),
        tableSchema.unique_keys(),
        FarmFingerprint(tableSchema.columns().data(), tableSchema.columns().size() * sizeof(tableSchema.columns().Get(0))));
}

bool TSchemaDictionary::TEqualsInternal::operator()(
    const TTableSchemaInternal& lhs,
    const TTableSchemaInternal& rhs) const
{
    return
        lhs.strict() == rhs.strict() &&
        lhs.unique_keys() == rhs.unique_keys() &&
        lhs.columns().size() == rhs.columns().size() &&
        std::equal(lhs.columns().data(), lhs.columns().data() + lhs.columns().size(), rhs.columns().data());
}

////////////////////////////////////////////////////////////////////////////////

void ToProto(NProto::TSchemaDictionary* protoDictionary, const TSchemaDictionary& dictionary)
{
    using NYT::ToProto;

    ToProto(protoDictionary->mutable_columns(), dictionary.IdToColumn_);
    for (const auto& table : dictionary.IdToTableInternal_) {
        auto* protoTable = protoDictionary->add_tables();
        protoTable->MergeFrom(table);
    }
}

void FromProto(TSchemaDictionary* dictionary, const NProto::TSchemaDictionary& protoDictionary)
{
    using NYT::FromProto;

    FromProto(&dictionary->IdToColumn_, protoDictionary.columns());
    for (int index = 0; index < std::ssize(dictionary->IdToColumn_); ++index) {
        YT_VERIFY(dictionary->ColumnToId_.insert({dictionary->IdToColumn_[index], index}).second);
    }
    for (int index = 0; index < protoDictionary.tables().size(); ++index) {
        const auto& protoTable = protoDictionary.tables().Get(index);
        dictionary->IdToTableInternal_.emplace_back(protoTable);
        YT_VERIFY(dictionary->TableInternalToId_.insert({protoTable, index}).second);
        std::vector<TColumnSchema> columns;
        for (int id : protoTable.columns()) {
            columns.emplace_back(dictionary->IdToColumn_[id]);
        }
        dictionary->IdToTable_.push_back(New<TTableSchema>(
            std::move(columns),
            protoTable.strict(),
            protoTable.unique_keys()));
    }
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NTableClient
