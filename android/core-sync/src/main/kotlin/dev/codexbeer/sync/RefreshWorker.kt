package dev.codexbeer.sync

import android.content.Context
import androidx.work.*
import kotlinx.coroutines.CancellationException

class RefreshWorker(context: Context, parameters: WorkerParameters) : CoroutineWorker(context, parameters) {
    override suspend fun doWork(): Result = try {
        SyncRepository.get(applicationContext).refresh()
        Result.success()
    } catch (error: Exception) {
        if (error is CancellationException) throw error
        if (runAttemptCount < 4) Result.retry() else Result.failure()
    }
    companion object {
        fun schedule(context: Context) {
            WorkManager.getInstance(context).enqueueUniquePeriodicWork("quota-fallback", ExistingPeriodicWorkPolicy.KEEP,
                PeriodicWorkRequestBuilder<RefreshWorker>(15, java.util.concurrent.TimeUnit.MINUTES)
                    .setConstraints(Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build()).build())
        }
        fun enqueue(context: Context) {
            WorkManager.getInstance(context).enqueueUniqueWork("quota-refresh", ExistingWorkPolicy.KEEP,
                OneTimeWorkRequestBuilder<RefreshWorker>()
                    .setConstraints(Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build()).build())
        }
    }
}
