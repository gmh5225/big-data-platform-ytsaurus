package tech.ytsaurus.spark.launcher

import com.twitter.scalding.Args
import org.slf4j.LoggerFactory
import tech.ytsaurus.spyt.wrapper.Utils.parseDuration
import tech.ytsaurus.spyt.wrapper.client.YtClientConfiguration
import tech.ytsaurus.spyt.wrapper.discovery.DiscoveryService

import scala.concurrent.duration._
import scala.language.postfixOps

object HistoryServerLauncher extends App with VanillaLauncher with SparkLauncher {
  val log = LoggerFactory.getLogger(getClass)

  val launcherArgs = HistoryServerLauncherArgs(args)

  import launcherArgs._

  prepareProfiler()

  withDiscovery(ytConfig, discoveryPath) { case (discoveryService, _) =>
    val masterAddress = waitForMaster(waitMasterTimeout, discoveryService)

    withService(startHistoryServer(logPath, memory, discoveryService)) { historyServer =>
      discoveryService.registerSHS(historyServer.address)

      def isAlive: Boolean = {
        val isMasterAlive = DiscoveryService.isAlive(masterAddress.hostAndPort, 3)
        val isShsAlive = historyServer.isAlive(3)

        isMasterAlive && isShsAlive
      }

      checkPeriodically(isAlive)
      log.error("Shutdown SHS")
    }
  }
}

case class HistoryServerLauncherArgs(logPath: String,
                                     memory: String,
                                     ytConfig: YtClientConfiguration,
                                     discoveryPath: String,
                                     waitMasterTimeout: Duration)

object HistoryServerLauncherArgs {
  def apply(args: Args): HistoryServerLauncherArgs = HistoryServerLauncherArgs(
    args.required("log-path"),
    args.optional("memory").getOrElse("16G"),
    YtClientConfiguration(args.optional),
    args.optional("discovery-path").getOrElse(sys.env("SPARK_DISCOVERY_PATH")),
    args.optional("wait-master-timeout").map(parseDuration).getOrElse(5 minutes)
  )

  def apply(args: Array[String]): HistoryServerLauncherArgs = HistoryServerLauncherArgs(Args(args))
}
