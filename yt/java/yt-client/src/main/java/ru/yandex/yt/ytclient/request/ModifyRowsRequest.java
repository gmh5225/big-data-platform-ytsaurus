package ru.yandex.yt.ytclient.request;

import java.util.AbstractList;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Objects;

import javax.annotation.Nullable;

import ru.yandex.yt.rpcproxy.ERowModificationType;
import ru.yandex.yt.ytclient.object.UnversionedRowSerializer;
import ru.yandex.yt.ytclient.proxy.ApiServiceUtil;
import ru.yandex.yt.ytclient.tables.TableSchema;
import ru.yandex.yt.ytclient.wire.UnversionedRow;
import ru.yandex.yt.ytclient.wire.UnversionedValue;
import ru.yandex.yt.ytclient.wire.WireProtocolWriter;

/**
 * Row modification request that uses {@link UnversionedRow} as table row representation
 *
 * @see MappedModifyRowsRequest
 * @see UnversionedRow
 */
public class ModifyRowsRequest extends PreparableModifyRowsRequest<ModifyRowsRequest.Builder, ModifyRowsRequest> {
    private final List<UnversionedRow> rows = new ArrayList<>();

    public ModifyRowsRequest(BuilderBase<?> builder) {
        super(builder);
        if (builder.convertedRows != null) {
            this.rows.addAll(builder.convertedRows);
        }
        for (BuilderBase.RowMeta meta : builder.rows) {
            List<?> values;
            switch (meta.type) {
                case INSERT: {
                    values = meta.values != null
                        ? meta.values
                        : mapToValues(Objects.requireNonNull(meta.map), schema.getColumnsCount());
                    if (values.size() != schema.getColumns().size()) {
                        throw new IllegalArgumentException(
                                "Number of insert columns must match number of schema columns");
                    }
                    break;
                }
                case UPDATE: {
                    values = meta.values != null
                        ? meta.values
                        : mapToValues(Objects.requireNonNull(meta.map), schema.getColumnsCount());
                    if (values.size() <= schema.getKeyColumnsCount()
                            || values.size() > schema.getColumns().size()) {
                        throw new IllegalArgumentException(
                                "Number of update columns must be more than the number of key columns");
                    }
                    break;
                }
                case DELETE: {
                    values = meta.values != null
                            ? meta.values
                            : mapToValues(Objects.requireNonNull(meta.map), schema.getKeyColumnsCount());
                    if (values.size() != schema.getKeyColumnsCount()) {
                        throw new IllegalArgumentException(
                                "Number of delete columns must match number of key columns");
                    }
                    break;
                }
                default: {
                    throw new IllegalArgumentException("unknown modification type");
                }
            }

            rows.add(convertValuesToRow(values, meta.skipMissingValues, meta.aggregate));
        }
    }

    public ModifyRowsRequest(String path, TableSchema schema) {
        this(builder().setPath(path).setSchema(schema));
    }

    public static Builder builder() {
        return new Builder();
    }

    public List<UnversionedRow> getRows() {
        return Collections.unmodifiableList(rows);
    }

    private UnversionedRow convertValuesToRow(List<?> values, boolean skipMissingValues, boolean aggregate) {
        if (values.size() < schema.getKeyColumnsCount()) {
            throw new IllegalArgumentException(
                    "Number of values must be more than or equal to the number of key columns");
        }
        List<UnversionedValue> row = new ArrayList<>(values.size());
        ApiServiceUtil.convertKeyColumns(row, schema, values);
        ApiServiceUtil.convertValueColumns(row, schema, values, skipMissingValues, aggregate);
        return new UnversionedRow(row);
    }

    private List<Object> mapToValues(Map<String, ?> values, int size) {
        return new AbstractList<Object>() {
            @Override
            public Object get(int index) {
                return values.get(schema.getColumns().get(index).getName());
            }

            @Override
            public int size() {
                return size;
            }
        };
    }

    @Override
    public void serializeRowsetTo(List<byte[]> attachments) {
        WireProtocolWriter writer = new WireProtocolWriter(attachments);
        writer.writeUnversionedRowset(rows, new UnversionedRowSerializer(schema));
        writer.finish();
    }

    @Override
    public Builder toBuilder() {
        return builder()
                .setRows(rows)
                .setPath(path)
                .setSchema(schema)
                .setRequireSyncReplica(requireSyncReplica)
                .setRowModificationTypes(rowModificationTypes)
                .setTimeout(timeout)
                .setRequestId(requestId)
                .setUserAgent(userAgent)
                .setTraceId(traceId, traceSampled)
                .setAdditionalData(additionalData);
    }

    public static class Builder extends BuilderBase<Builder> {
        @Override
        protected Builder self() {
            return this;
        }

        @Override
        public ModifyRowsRequest build() {
            return new ModifyRowsRequest(this);
        }
    }

    public abstract static class BuilderBase<
            TBuilder extends BuilderBase<TBuilder>>
            extends PreparableModifyRowsRequest.Builder<TBuilder, ModifyRowsRequest> {
        private final List<RowMeta> rows = new ArrayList<>();
        @Nullable
        private List<UnversionedRow> convertedRows;

        public TBuilder addInsert(List<?> values) {
            rows.add(new RowMeta(ModificationType.INSERT, values, false, false));
            addRowModificationType(ERowModificationType.RMT_WRITE);
            return self();
        }

        public TBuilder addInserts(Iterable<? extends List<?>> rows) {
            for (List<?> row : rows) {
                addInsert(row);
            }
            return self();
        }

        public TBuilder addUpdate(List<?> values, boolean aggregate) {
            rows.add(new RowMeta(ModificationType.UPDATE, values, true, aggregate));
            addRowModificationType(ERowModificationType.RMT_WRITE);
            return self();
        }

        public TBuilder addUpdates(Iterable<? extends List<?>> rows, boolean aggregate) {
            for (List<?> row : rows) {
                addUpdate(row, aggregate);
            }
            return self();
        }

        public TBuilder addUpdate(List<?> values) {
            return addUpdate(values, false);
        }

        public TBuilder addUpdates(Iterable<? extends List<?>> rows) {
            return addUpdates(rows, false);
        }

        public TBuilder addDelete(List<?> values) {
            rows.add(new RowMeta(ModificationType.DELETE, values, false, false));
            addRowModificationType(ERowModificationType.RMT_DELETE);
            return self();
        }

        public TBuilder addDeletes(Iterable<? extends List<?>> keys) {
            for (List<?> key : keys) {
                addDelete(key);
            }
            return self();
        }

        public TBuilder addInsert(Map<String, ?> map) {
            rows.add(new RowMeta(ModificationType.INSERT, map, false, false));
            addRowModificationType(ERowModificationType.RMT_WRITE);
            return self();
        }

        public TBuilder addUpdate(Map<String, ?> map, boolean aggregate) {
            rows.add(new RowMeta(ModificationType.UPDATE, map, true, aggregate));
            addRowModificationType(ERowModificationType.RMT_WRITE);
            return self();
        }

        public TBuilder addUpdate(Map<String, ?> map) {
            return addUpdate(map, false);
        }

        public TBuilder addDelete(Map<String, ?> map) {
            rows.add(new RowMeta(ModificationType.DELETE, map, false, false));
            addRowModificationType(ERowModificationType.RMT_DELETE);
            return self();
        }

        TBuilder setRows(@Nullable List<UnversionedRow> rows) {
            this.convertedRows = rows;
            return self();
        }

        enum ModificationType {
            UPDATE("update"),
            INSERT("insert"),
            DELETE("delete");

            String value;

            ModificationType(String value) {
                this.value = value;
            }
        }

        static class RowMeta {
            ModificationType type;
            List<?> values;
            boolean skipMissingValues;

            Map<String, ?> map;

            boolean aggregate;

            RowMeta(ModificationType type, List<?> values, boolean skipMissingValues, boolean aggregate) {
                this.type = type;
                this.values = values;
                this.skipMissingValues = skipMissingValues;
                this.aggregate = aggregate;
            }

            RowMeta(ModificationType type, Map<String, ?> map, boolean skipMissingValues, boolean aggregate) {
                this.type = type;
                this.map = map;
                this.skipMissingValues = skipMissingValues;
                this.aggregate = aggregate;
            }
        }
    }
}