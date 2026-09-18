plugins { id("com.android.library") }
android { namespace = "dev.codexbeer.notifications"; compileSdk { version = release(37) { minorApiLevel = 2 } }; defaultConfig { minSdk = 26 }; compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 } }
dependencies {
    implementation(project(":core-sync"))
    implementation("androidx.core:core-ktx:1.19.0")
}
