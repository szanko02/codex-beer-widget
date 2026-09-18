package dev.codexbeer.tile

import android.app.PendingIntent
import android.os.Build
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import dev.codexbeer.data.LocalStateStore
import dev.codexbeer.overlay.OverlayService
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first

class QuotaTileService : TileService() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private var refresh: Job? = null
    override fun onStartListening() {
        refresh = scope.launch {
            val state = LocalStateStore(this@QuotaTileService).states.first()
            val group = state.snapshot?.groups?.get("codex") ?: state.snapshot?.groups?.values?.firstOrNull()
            val remaining = group?.windows?.minByOrNull { it.windowMinutes ?: Long.MAX_VALUE }?.remaining
            qsTile?.apply {
                label = "Codex"
                if (Build.VERSION.SDK_INT >= 29) subtitle = remaining?.let { "%.0f%%".format(it) } ?: "Нет данных"
                this.state = if (OverlayService.running) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
                updateTile()
            }
        }
    }
    override fun onStopListening() { refresh?.cancel() }
    @Suppress("DEPRECATION") override fun onClick() {
        val intent = packageManager.getLaunchIntentForPackage(packageName)?.setAction("dev.codexbeer.TOGGLE_OVERLAY") ?: return
        if (Build.VERSION.SDK_INT >= 34) startActivityAndCollapse(PendingIntent.getActivity(this, 20, intent, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE))
        else startActivityAndCollapse(intent)
    }
    override fun onDestroy() { scope.cancel(); super.onDestroy() }
}
