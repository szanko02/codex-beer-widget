pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories { google(); mavenCentral() }
}
rootProject.name = "CodexQuotaAndroid"
include(":app", ":core-model", ":core-data", ":core-sync", ":feature-dashboard")
include(":feature-notifications")
