package chyt

import (
	"context"

	"a.yandex-team.ru/yt/chyt/controller/internal/strawberry"
	"a.yandex-team.ru/yt/go/ypath"
	"a.yandex-team.ru/yt/go/yson"
	"a.yandex-team.ru/yt/go/yt"
)

func appendArtifactDescription(description *map[string]yson.RawValue, ytc yt.Client, name string, path ypath.Path) (err error) {
	var artifactAttrs yson.RawValue
	err = ytc.GetNode(context.TODO(), path.Attr("user_attributes"), &artifactAttrs, nil)
	if err != nil {
		return
	}
	(*description)[name] = artifactAttrs
	return
}

func buildDescription(cluster string, alias string) map[string]interface{} {
	return map[string]interface{}{
		"yql_url": strawberry.ToYsonURL(
			"https://yql.yandex-team.ru/?query=use%20chyt." + cluster + "/" + alias +
				"%3B%0A%0Aselect%201%3B&query_type=CLICKHOUSE"),
		"solomon_root_url": strawberry.ToYsonURL(
			"https://solomon.yandex-team.ru/?project=yt&cluster=" + cluster + "&service=clickhouse&operation_alias=" +
				alias),
		"solomon_dashboard_url": strawberry.ToYsonURL(
			"https://solomon.yandex-team.ru/?project=yt&cluster=" + cluster + "&service=clickhouse&cookie=Aggr" +
				"&dashboard=chyt_v2&l.operation_alias=" + alias),
	}
}