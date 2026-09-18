package dev.codexbeer.android

import android.app.Application
import dev.codexbeer.notifications.StatusNotification
import dev.codexbeer.sync.SyncRepository
import kotlinx.coroutines.*

class QuotaApplication : Application() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    override fun onCreate() {
        super.onCreate()
        StatusNotification.channels(this)
        scope.launch {
            SyncRepository.get(this@QuotaApplication).states.collect {
                StatusNotification.render(this@QuotaApplication, it)
            }
        }
    }
}
