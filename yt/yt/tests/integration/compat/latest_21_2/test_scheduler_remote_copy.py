from original_tests.yt.yt.tests.integration.tests.test_scheduler_remote_copy \
    import TestSchedulerRemoteCopyCommands as BaseTestRemoteCopyCommands


class TestRemoteCopyCommandsCompatUpToCA(BaseTestRemoteCopyCommands):
    ARTIFACT_COMPONENTS = {
        "21_2": ["master", "node", "job-proxy", "exec", "tools"],
        "trunk": ["scheduler", "controller-agent", "proxy", "http-proxy"],
    }