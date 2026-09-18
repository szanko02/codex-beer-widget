package dev.codexbeer.notifications

import android.Manifest
import android.app.*
import android.content.*
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import dev.codexbeer.model.LocalState
import dev.codexbeer.sync.RefreshWorker
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

object StatusNotification {
    const val STATUS_CHANNEL = "codex_status"
    const val ALERT_CHANNEL = "codex_alerts"
    const val STATUS_ID = 100
    private fun preferences(context: Context) = context.getSharedPreferences("surfaces", Context.MODE_PRIVATE)
    fun enabled(context: Context) = preferences(context).getBoolean("notifications", false)
    fun setEnabled(context: Context, enabled: Boolean) {
        preferences(context).edit().putBoolean("notifications", enabled).apply()
        if (!enabled) context.getSystemService(NotificationManager::class.java).cancel(STATUS_ID)
    }
    fun channels(context: Context) {
        context.getSystemService(NotificationManager::class.java).createNotificationChannels(listOf(
            NotificationChannel(STATUS_CHANNEL, "Codex Status", NotificationManager.IMPORTANCE_LOW),
            NotificationChannel(ALERT_CHANNEL, "Codex Alerts", NotificationManager.IMPORTANCE_DEFAULT),
        ))
    }
    fun permitted(context: Context) = Build.VERSION.SDK_INT < 33 ||
        ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

    fun render(context: Context, state: LocalState) {
        if (!enabled(context) || !permitted(context)) return
        channels(context)
        val groups = state.snapshot?.groups
        val group = groups?.get("codex") ?: groups?.values?.firstOrNull()
        val windows = group?.windows?.sortedBy { it.windowMinutes ?: Long.MAX_VALUE }.orEmpty()
        val main = windows.firstOrNull()
        val percent = main?.remaining?.let { "%.0f%%".format(it) } ?: "нет данных"
        val now = System.currentTimeMillis() / 1000
        val stale = state.stale(now) || group?.stale == true
        val reset = main?.resetsAt?.let {
            DateTimeFormatter.ofPattern("dd.MM HH:mm").withZone(ZoneId.systemDefault()).format(Instant.ofEpochSecond(it))
        } ?: "неизвестен"
        val details = "${main?.windowMinutes?.let { "$it мин" } ?: "окно неизвестно"} · сброс $reset" +
            windows.getOrNull(1)?.let { "\n${it.windowMinutes?.let { value -> "$value мин" } ?: it.slot}: ${it.remaining?.let { value -> "%.0f%%".format(value) } ?: "нет данных"}" }.orEmpty() +
            (group?.sourceUpdatedAt?.let { "\nНаблюдение: " + DateTimeFormatter.ofPattern("dd.MM HH:mm:ss")
                .withZone(ZoneId.systemDefault()).format(Instant.ofEpochSecond(it)) } ?: "\nНет наблюдений")
        val launch = context.packageManager.getLaunchIntentForPackage(context.packageName) ?: return
        val open = PendingIntent.getActivity(context, 0, launch, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        val overlay = PendingIntent.getActivity(context, 1, Intent(launch).setAction("dev.codexbeer.SHOW_OVERLAY"), PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        val refresh = PendingIntent.getBroadcast(context, 2, Intent(context, RefreshReceiver::class.java), PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        val notification = NotificationCompat.Builder(context, STATUS_CHANNEL)
            .setSmallIcon(R.drawable.ic_quota).setContentTitle("Codex $percent${if (stale) " · устарело" else ""}")
            .setContentText(details).setStyle(NotificationCompat.BigTextStyle().bigText(details))
            .setContentIntent(open).setOnlyAlertOnce(true).setOngoing(true).setShowWhen(group?.sourceUpdatedAt != null)
            .setVisibility(NotificationCompat.VISIBILITY_PRIVATE)
            .addAction(0, "Обновить", refresh).addAction(0, "Плавающий виджет", overlay)
        main?.remaining?.let { notification.setProgress(100, it.toInt(), false) }
        group?.sourceUpdatedAt?.takeIf { it <= Long.MAX_VALUE / 1000 }?.let { notification.setWhen(it * 1000) }
        context.getSystemService(NotificationManager::class.java).notify(STATUS_ID, notification.build())
    }
}

class RefreshReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) { RefreshWorker.enqueue(context) }
}
