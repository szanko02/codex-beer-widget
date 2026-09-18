package dev.codexbeer.model

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

const val MAX_JSON_INTEGER: Long = 9007199254740991
val quotaJson = Json { ignoreUnknownKeys = false; explicitNulls = true }

@Serializable
data class QuotaWindow(val slot: String, val remaining: Double?, val windowMinutes: Long?, val resetsAt: Long?)

@Serializable
data class QuotaGroup(val name: String, val sourceUpdatedAt: Long?, val stale: Boolean, val windows: List<QuotaWindow>)

@Serializable
data class QuotaSnapshot(
    val version: Int, val revision: Long, val sourceUpdatedAt: Long?, val stale: Boolean,
    val groups: Map<String, QuotaGroup>,
) {
    fun validated(): QuotaSnapshot = apply {
        require(version == 1 && revision in 1..MAX_JSON_INTEGER)
        require(validTime(sourceUpdatedAt) && groups.size <= 64)
        groups.forEach { (id, group) ->
            require(id.isNotEmpty() && id.codePointCount(0, id.length) <= 128)
            require(group.name.codePointCount(0, group.name.length) <= 128 && validTime(group.sourceUpdatedAt))
            require(group.windows.size <= 2 && group.windows.map { it.slot }.distinct().size == group.windows.size)
            group.windows.forEach {
                require(it.slot == "primary" || it.slot == "secondary")
                require(it.remaining == null || (it.remaining.isFinite() && it.remaining in 0.0..100.0))
                require(validTime(it.windowMinutes) && validTime(it.resetsAt))
            }
        }
    }
}

private fun validTime(value: Long?) = value == null || value in 1..MAX_JSON_INTEGER
fun decodeSnapshot(text: String): QuotaSnapshot {
    require(text.toByteArray(Charsets.UTF_8).size <= 65536)
    return quotaJson.decodeFromString<QuotaSnapshot>(text).validated()
}

fun confirmedReset(previous: QuotaWindow, current: QuotaWindow): Boolean =
    previous.slot == current.slot && previous.remaining != null && current.remaining != null &&
        current.remaining > previous.remaining && previous.resetsAt != null && current.resetsAt != null &&
        current.resetsAt > previous.resetsAt

enum class ConnectionState { UNPAIRED, CONNECTING, CONNECTED, STALE }

data class LocalState(val snapshot: QuotaSnapshot? = null, val receivedAt: Long = 0, val connection: ConnectionState = ConnectionState.UNPAIRED) {
    fun stale(nowSeconds: Long): Boolean = connection != ConnectionState.CONNECTED || snapshot?.stale != false ||
        receivedAt <= 0 || nowSeconds < receivedAt || nowSeconds - receivedAt >= 600
}
