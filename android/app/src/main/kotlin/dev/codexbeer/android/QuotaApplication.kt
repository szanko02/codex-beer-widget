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
            SyncRepository.get(this@QuotaApplication).states.collect {
                StatusNotification.render(this@QuotaApplication, it)
                WidgetUpdates.update(this@QuotaApplication)
            }
        }
    }
}
