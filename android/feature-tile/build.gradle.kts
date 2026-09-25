plugins { id("com.android.library") }
android { namespace = "dev.codexbeer.tile"; compileSdk { version = release(37) { minorApiLevel = 2 } }; defaultConfig { minSdk = 26 }; compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 } }
dependencies { implementation(project(":core-data")); implementation(project(":feature-overlay")); implementation(project(":feature-notifications")); implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2") }
