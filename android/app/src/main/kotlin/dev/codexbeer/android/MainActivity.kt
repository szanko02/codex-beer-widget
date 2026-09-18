package dev.codexbeer.android

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import dev.codexbeer.dashboard.Dashboard
import dev.codexbeer.sync.SyncRepository

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { Dashboard(SyncRepository.get(this)) }
    }
}
