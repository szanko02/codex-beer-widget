package dev.codexbeer.notifications

import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import androidx.core.app.NotificationCompat
import dev.codexbeer.model.*

class AlertNotifications(private val context: Context) {
    private val preferences = context.getSharedPreferences("alerts", Context.MODE_PRIVATE)
    private var connected: Boolean? = null
    fun render(state: LocalState) {
        val snapshot = state.snapshot ?: return
        val current = !state.stale(System.currentTimeMillis() / 1000)
        val messages = mutableListOf<String>()
        if (connected != null && current != connected) {
            val setting = if (current) "restored" else "lost"
            if (preferences.getBoolean(setting, false)) messages += if (current) "Связь восстановлена" else "Связь потеряна; показаны последние данные"
        }
        if (current || connected != null) connected = current
        if (current) {
            val ledger = preferences.getString("ledger", null)?.let {
                runCatching { quotaJson.decodeFromString<AlertLedger>(it) }.getOrNull()
            } ?: AlertLedger()
            val thresholds = listOf(50, 25, 10, 5, 0).filter { preferences.getBoolean("threshold_$it", false) }.toSet()
            val result = evaluateAlerts(snapshot, ledger, thresholds, preferences.getBoolean("reset", false))
            if (result.ledger != ledger) {
                // Commit deduplication before posting: process death must not replay alerts.
                if (!preferences.edit().putString("ledger", quotaJson.encodeToString(AlertLedger.serializer(), result.ledger)).commit()) return
            }
            messages += result.events.map { event ->
                "${event.group} · ${event.slot}: " + (event.threshold?.let { "осталось ≤$it%" } ?: "лимит восстановлен")
            }
        }
        if (messages.isEmpty() || !StatusNotification.permitted(context)) return
        StatusNotification.channels(context)
        val launch = context.packageManager.getLaunchIntentForPackage(context.packageName) ?: return
        val open = PendingIntent.getActivity(context, 30, launch, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        context.getSystemService(NotificationManager::class.java).notify(200,
            NotificationCompat.Builder(context, StatusNotification.ALERT_CHANNEL)
                .setSmallIcon(R.drawable.ic_quota).setContentTitle("Codex Quota")
                .setContentText(messages.last()).setStyle(NotificationCompat.BigTextStyle().bigText(messages.joinToString("\n")))
                .setContentIntent(open).setAutoCancel(true).setVisibility(NotificationCompat.VISIBILITY_PRIVATE).build())
    }
}
