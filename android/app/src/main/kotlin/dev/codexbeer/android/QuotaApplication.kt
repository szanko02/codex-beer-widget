package dev.codexbeer.android

import android.app.Application
import dev.codexbeer.notifications.StatusNotification
import dev.codexbeer.sync.SyncRepository
import dev.codexbeer.widget.WidgetUpdates
import kotlinx.coroutines.*

class QuotaApplication : Application() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    override fun onCreate() {
        super.onCreate()
        StatusNotification.channels(this)
        dev.codexbeer.sync.RefreshWorker.schedule(this)
        if (com.google.firebase.FirebaseApp.getApps(this).isNotEmpty()) {
            com.google.firebase.messaging.FirebaseMessaging.getInstance().token.addOnSuccessListener { token ->
                val preferences = getSharedPreferences("push", MODE_PRIVATE)
                if (preferences.getString("token", null) != token) preferences.edit().putString("token", token).remove("registered").apply()
                dev.codexbeer.sync.RefreshWorker.enqueue(this)
            }
        }
        scope.launch {
            val alerts = dev.codexbeer.notifications.AlertNotifications(this@QuotaApplication)
            SyncRepository.get(this@QuotaApplication).states.collect {
                surface { alerts.render(it) }
                surface { StatusNotification.render(this@QuotaApplication, it) }
                surface { WidgetUpdates.update(this@QuotaApplication) }
            }
        }
    }
    private suspend fun surface(render: suspend () -> Unit) {
        try { render() } catch (error: Exception) {
            if (error is CancellationException) throw error
            // A failed launcher/notification surface must not stop other consumers.
        }
    }
}
