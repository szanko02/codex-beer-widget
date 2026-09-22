package dev.codexbeer.model

import kotlinx.serialization.Serializable

@Serializable
data class AlertWindow(val window: QuotaWindow, val fired: Set<Int> = emptySet())
@Serializable
data class AlertLedger(val windows: Map<String, AlertWindow> = emptyMap())
data class QuotaAlert(val group: String, val slot: String, val threshold: Int? = null)
data class AlertResult(val ledger: AlertLedger, val events: List<QuotaAlert>)

fun evaluateAlerts(snapshot: QuotaSnapshot, ledger: AlertLedger,
                   thresholds: Set<Int>, resetEnabled: Boolean): AlertResult {
    if (snapshot.stale) return AlertResult(ledger, emptyList())
    val next = ledger.windows.toMutableMap()
    val events = mutableListOf<QuotaAlert>()
    snapshot.groups.forEach { (id, group) ->
        if (group.stale) return@forEach
        group.windows.forEach windowLoop@ { window ->
            val remaining = window.remaining ?: return@windowLoop
            // Length prefix prevents collisions even when group IDs contain separators.
            val key = "${id.length}:$id:${window.slot}"
            val old = next[key]
            val reset = old != null && confirmedReset(old.window, window)
            val fired = (if (reset) emptySet() else old?.fired.orEmpty()).toMutableSet()
            if (reset && resetEnabled) events += QuotaAlert(id, window.slot)
            for (threshold in listOf(50, 25, 10, 5, 0)) {
                if (remaining <= threshold && fired.add(threshold) && threshold in thresholds)
                    events += QuotaAlert(id, window.slot, threshold)
            }
            next[key] = AlertWindow(window, fired)
        }
    }
    // Retain briefly absent groups; cap storage for changing account limit IDs.
    return AlertResult(AlertLedger(next.entries.toList().takeLast(512).associate { it.toPair() }), events)
}
