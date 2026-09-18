plugins { id("com.android.library"); id("org.jetbrains.kotlin.plugin.compose") }
android { namespace = "dev.codexbeer.widget"; compileSdk { version = release(37) { minorApiLevel = 2 } }; defaultConfig { minSdk = 26 }; buildFeatures { compose = true }; compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 } }
dependencies {
    implementation(project(":core-data"))
    implementation("androidx.glance:glance-appwidget:1.2.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
}
