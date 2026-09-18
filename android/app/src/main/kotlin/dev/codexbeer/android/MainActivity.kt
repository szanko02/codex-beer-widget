package dev.codexbeer.android

import android.os.Bundle
import android.os.Build
import android.Manifest
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import dev.codexbeer.dashboard.Dashboard
import dev.codexbeer.sync.SyncRepository
import dev.codexbeer.notifications.StatusNotification

class MainActivity : ComponentActivity() {
    private val notificationPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted) showStatus()
    }
    private fun showStatus() {
        StatusNotification.setEnabled(this, true)
        StatusNotification.render(this, SyncRepository.get(this).states.value)
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            Dashboard(SyncRepository.get(this)) {
                Button(onClick = {
                    if (Build.VERSION.SDK_INT >= 33 && !StatusNotification.permitted(this@MainActivity))
                        notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
                    else showStatus()
                }) { Text("Показывать в шторке") }
                Button(onClick = { StatusNotification.setEnabled(this@MainActivity, false) }) { Text("Скрыть из шторки") }
            }
        }
    }
}
