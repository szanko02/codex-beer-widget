package dev.codexbeer.android

import android.os.Bundle
import android.os.Build
import android.Manifest
import android.content.Intent
import android.net.Uri
import android.provider.Settings
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.HorizontalDivider
import androidx.compose.foundation.layout.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.Alignment
import androidx.compose.runtime.*
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import dev.codexbeer.overlay.OverlayService
import kotlinx.coroutines.launch
import dev.codexbeer.dashboard.Dashboard
import dev.codexbeer.dashboard.BeerTheme
import androidx.compose.foundation.isSystemInDarkTheme
import dev.codexbeer.sync.SyncRepository
import dev.codexbeer.notifications.StatusNotification

class MainActivity : ComponentActivity() {
    private var statusRequested = false
    override fun onStart() { super.onStart(); SyncRepository.get(this).setActive("activity", true) }
    override fun onStop() { SyncRepository.get(this).setActive("activity", false); super.onStop() }
    private var overlayRequested = false
    private val overlayPermission = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (overlayRequested && Settings.canDrawOverlays(this)) startOverlay()
        overlayRequested = false
    }
    private val scanner = registerForActivityResult(ScanContract()) { result ->
        result.contents?.let { qr -> lifecycleScope.launch {
            try {
                SyncRepository.get(this@MainActivity).pair(qr)
                Toast.makeText(this@MainActivity, "Компьютер подключён", Toast.LENGTH_SHORT).show()
            }
            catch (error: Exception) {
                if (error is kotlinx.coroutines.CancellationException) throw error
                Toast.makeText(this@MainActivity, "Не удалось подключиться. Проверьте интернет и отсканируйте новый QR.", Toast.LENGTH_LONG).show() }
        } }
    }
    private fun startOverlay() {
        if (!Settings.canDrawOverlays(this)) {
            overlayRequested = true
            overlayPermission.launch(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:$packageName")))
            return
        }
        ContextCompat.startForegroundService(this, Intent(this, OverlayService::class.java))
    }
    override fun onNewIntent(intent: Intent) { super.onNewIntent(intent); setIntent(intent) }
    override fun onResume() {
        super.onResume()
        when (intent.action) {
            "dev.codexbeer.SHOW_OVERLAY" -> { intent.action = null; startOverlay() }
            "dev.codexbeer.TOGGLE_OVERLAY" -> {
                intent.action = null
                if (OverlayService.running) stopService(Intent(this, OverlayService::class.java)) else startOverlay()
            }
        }
    }
    private val notificationPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted && statusRequested) showStatus()
        statusRequested = false
    }
    private fun showStatus() {
        StatusNotification.setEnabled(this, true)
        StatusNotification.render(this, SyncRepository.get(this).states.value)
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            BeerTheme {
                val dark = isSystemInDarkTheme()
                SideEffect {
                    androidx.core.view.WindowCompat.getInsetsController(window, window.decorView).apply {
                        isAppearanceLightStatusBars = !dark
                        isAppearanceLightNavigationBars = !dark
                    }
                }
                Dashboard(SyncRepository.get(this), onScan = {
                    scanner.launch(ScanOptions().setDesiredBarcodeFormats(ScanOptions.QR_CODE)
                        .setPrompt("Наведите камеру на QR подключения на компьютере").setBeepEnabled(false))
                }) {
                    Button(onClick = {
                        if (Build.VERSION.SDK_INT >= 33 && !StatusNotification.permitted(this@MainActivity)) {
                            statusRequested = true
                            notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
                        } else showStatus()
                    }) { Text("Показывать в шторке") }
                    Button(onClick = { StatusNotification.setEnabled(this@MainActivity, false) }) { Text("Скрыть из шторки") }
                    Button(onClick = { startOverlay() }) { Text("Показать поверх приложений") }
                    Button(onClick = { stopService(Intent(this@MainActivity, OverlayService::class.java)) }) { Text("Скрыть плавающий виджет") }
                    HorizontalDivider()
                    Text("Плавающий виджет", style = MaterialTheme.typography.titleLarge)
                    OverlaySettings()
                    HorizontalDivider()
                    AlertSettings()
                }
            }
        }
    }
    @Composable private fun OverlaySettings() {
        val preferences = remember { getSharedPreferences("overlay", MODE_PRIVATE) }
        var size by remember { mutableFloatStateOf(preferences.getFloat("size", 140f)) }
        var opacity by remember { mutableFloatStateOf(preferences.getFloat("opacity", .85f)) }
        var ring by remember { mutableStateOf(preferences.getBoolean("ring", false)) }
        var animate by remember { mutableStateOf(preferences.getBoolean("animate", true)) }
        fun update() {
            preferences.edit().putFloat("size", size).putFloat("opacity", opacity).putBoolean("ring", ring).putBoolean("animate", animate).apply()
            if (OverlayService.running) startService(Intent(this@MainActivity, OverlayService::class.java).setAction("update"))
        }
        Text("Размер: ${size.toInt()} dp")
        Slider(value = size, onValueChange = { size = it; update() }, valueRange = 80f..240f)
        Text("Непрозрачность: ${(opacity * 100).toInt()}%")
        Slider(value = opacity, onValueChange = { opacity = it; update() }, valueRange = .2f..1f)
        SettingSwitch("Минималистическое кольцо", ring) { ring = it; update() }
        SettingSwitch("Плавное изменение уровня", animate) { animate = it; update() }
    }
    @Composable private fun AlertSettings() {
        val preferences = remember { getSharedPreferences("alerts", MODE_PRIVATE) }
        Text("Оповещения", style = MaterialTheme.typography.titleLarge)
        val options = listOf("threshold_50" to "Осталось 50%", "threshold_25" to "Осталось 25%",
            "threshold_10" to "Осталось 10%", "threshold_5" to "Осталось 5%", "threshold_0" to "Лимит исчерпан",
            "reset" to "Лимит восстановлен", "lost" to "Связь потеряна", "restored" to "Связь восстановлена")
        options.forEach { (key, label) ->
            var enabled by remember(key) { mutableStateOf(preferences.getBoolean(key, false)) }
            SettingSwitch(label, enabled) {
                enabled = it; preferences.edit().putBoolean(key, it).apply()
                if (it && Build.VERSION.SDK_INT >= 33 && !StatusNotification.permitted(this@MainActivity))
                    notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }
    @Composable private fun SettingSwitch(label: String, checked: Boolean, onChanged: (Boolean) -> Unit) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween) {
            Text(label, modifier = Modifier.weight(1f))
            Switch(checked = checked, onCheckedChange = onChanged)
        }
    }
}
