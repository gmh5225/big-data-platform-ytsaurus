# Правила версионирования

SPYT состоит из двух частей:
- кластер, в который заранее установлен Spark;
- клиент, в который ставится пакет `ytsaurus-spyt` через `pip`. Клиент подтягивает зависимости со Spark.

SPYT собирается в несколько артефактов:
- архив `.tar`, выкладывается в {{product-name}} и читается при запуске кластера;
- pip-пакет с `pyspark`. Внутрь пакета кладутся все файлы `.jar` для Spark, как в `.tar` из предыдущего пункта;
- `yt-data-source` как отдельный файл `.jar`. Выкладывается в {{product-name}} и добавляется отдельно для каждого джоба. Средствами Spark одновременно скачивается на драйвер и на экзекьюторы (на клиентскую машину и в кластер);
- pip-пакет `ytsaurus-spyt`. Выкладывается в репозиторий и устанавливается на клиентские хосты. Позволяет динамически скачивать `yt-data-source` той же версии и устанавливает за собой по зависимости фиксированную версию `pyspark`.



При установке новой версии `ytsaurus-spyt` автоматически обновляются `pyspark` и `yt-data-source`.

В большинстве случаев достаточно обновить `ytsaurus-spyt`. Но иногда, чтобы заработала новая функциональность, придется обновить кластер. Порядок и объём обновления можно узнать по версиям.


## Порядок обновления { #how }

- Версия кластера и клиента состоит из трех частей. Обновление последней части версии означает, что никакая совместимость не нарушена, изменение было локальным в одной компоненте.

- При добавлении новой функциональности в `ytsaurus-spyt` необходимо обновить кластер. Обновите вторую компоненту версии у всех:
    * Новый кластер (например, 0.2.0) всегда совместим со старым клиентом (например, 0.1.0). Лучше всего обновлять сначала кластер, а потом клиент.
    * Если обновить клиент (например, 0.2.0), но не обновить кластер (например, 0.1.0), то вся старая функциональность клиента продолжит работать. Но новые функции, которые появились в 0.2.0, могут не работать или работать неправильно. В логах будут выводиться специальные предупреждения о том, что необходимо обновить кластер:
    ![](../../../../../images/backward_compatibility.png){ .center }

- Версию кластера можно выбрать в момент запуска в `spark-launch-yt`. Если версия не указана, запустится последняя релизная версия.
- Версия клиента в `client mode` (в Jupyter) — это установленная версия `ytsaurus-spyt`, устанавливается через `pip`.
- Версию клиента в `cluster mode` можно указать в `spark-submit-yt`. Если версия не указана, запустится последняя релизная, совместимая с кластером.

