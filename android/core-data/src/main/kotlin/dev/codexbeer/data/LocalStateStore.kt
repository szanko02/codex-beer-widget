package dev.codexbeer.data

import android.content.Context
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import dev.codexbeer.model.*
import kotlinx.coroutines.flow.map
import kotlinx.serialization.encodeToString

private val Context.quotaStore by preferencesDataStore("quota_state")
class LocalStateStore(context: Context) {
    private val store = context.applicationContext.quotaStore
    private val snapshotKey = stringPreferencesKey("snapshot")
    private val receivedKey = longPreferencesKey("received")
    private val connectedKey = booleanPreferencesKey("connected")
    val states = store.data.map { values ->
        val snapshot = values[snapshotKey]?.let { runCatching { decodeSnapshot(it) }.getOrNull() }
        LocalState(snapshot, values[receivedKey] ?: 0,
            if (values[connectedKey] == true) ConnectionState.CONNECTED else ConnectionState.STALE)
    }
    suspend fun accept(snapshot: QuotaSnapshot, receivedAt: Long = System.currentTimeMillis() / 1000): Boolean {
        snapshot.validated()
        var changed = false
        store.edit { values ->
            val old = values[snapshotKey]?.let { runCatching { decodeSnapshot(it) }.getOrNull() }
            if (old == null || snapshot.revision > old.revision) {
                values[snapshotKey] = quotaJson.encodeToString(snapshot)
                values[receivedKey] = receivedAt
                values[connectedKey] = true
                changed = true
            }
        }
        return changed
    }
    suspend fun disconnected() { store.edit { it[connectedKey] = false } }
    suspend fun clear() { store.edit { it.clear() } }
}
