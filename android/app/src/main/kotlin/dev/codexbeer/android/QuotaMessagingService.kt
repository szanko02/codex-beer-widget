package dev.codexbeer.android

import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage
import dev.codexbeer.sync.RefreshWorker

class QuotaMessagingService : FirebaseMessagingService() {
    override fun onNewToken(token: String) {
        getSharedPreferences("push", MODE_PRIVATE).edit().putString("token", token).remove("registered").apply()
        RefreshWorker.enqueue(this)
    }
    override fun onMessageReceived(message: RemoteMessage) {
        if (message.data["type"] == "quota_changed") RefreshWorker.enqueue(this)
    }
}
