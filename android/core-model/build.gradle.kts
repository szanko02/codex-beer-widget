plugins { kotlin("jvm"); kotlin("plugin.serialization") }
kotlin { jvmToolchain(17) }
dependencies {
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.9.0")
    testImplementation(kotlin("test"))
}
tasks.test { systemProperty("quota.fixtures", rootProject.file("../protocol/fixtures/normalization.json").absolutePath) }
