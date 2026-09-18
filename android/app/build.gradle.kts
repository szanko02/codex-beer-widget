plugins { id("com.android.application"); id("org.jetbrains.kotlin.plugin.compose") }
android {
    namespace = "dev.codexbeer.android"
    compileSdk { version = release(37) { minorApiLevel = 2 } }
    defaultConfig { applicationId = "dev.codexbeer.android"; minSdk = 26; targetSdk = 37; versionCode = 1; versionName = "0.1.0-dev" }
    buildFeatures { compose = true }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
}
dependencies {
    implementation(project(":feature-dashboard"))
    implementation(project(":core-sync"))
    implementation(project(":feature-notifications"))
    implementation(project(":feature-widget"))
    implementation("androidx.activity:activity-compose:1.13.0")
    implementation(platform("androidx.compose:compose-bom:2026.09.00"))
    implementation("androidx.compose.material3:material3")
}
