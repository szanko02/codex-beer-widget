package dev.codexbeer.model

import kotlin.test.*

class AlertEngineTest {
    private val thresholds = setOf(50, 25, 10, 5, 0)
    private fun snapshot(value: Double?, reset: Long? = 100, stale: Boolean = false, id: String = "codex") =
        QuotaSnapshot(1, 1, 1, stale, mapOf(id to QuotaGroup(id, 1, false,
            listOf(QuotaWindow("primary", value, 300, reset)))))
    @Test fun thresholdsFireOnceAcrossFurtherConsumptionAndRecovery() {
        val initial = evaluateAlerts(snapshot(9.0), AlertLedger(), thresholds, true)
        assertEquals(listOf(50, 25, 10), initial.events.map { it.threshold })
        val next = evaluateAlerts(snapshot(8.0), initial.ledger, thresholds, true)
        assertTrue(next.events.isEmpty())
        val recovery = evaluateAlerts(snapshot(40.0), next.ledger, thresholds, true)
        assertTrue(evaluateAlerts(snapshot(9.0), recovery.ledger, thresholds, true).events.isEmpty())
    }
    @Test fun onlyConfirmedResetRearmsThresholds() {
        val initial = evaluateAlerts(snapshot(4.0), AlertLedger(), thresholds, true)
        val moved = evaluateAlerts(snapshot(3.0, 200), initial.ledger, thresholds, true)
        assertTrue(moved.events.isEmpty())
        val reset = evaluateAlerts(snapshot(100.0, 300), moved.ledger, thresholds, true)
        assertEquals(listOf(null), reset.events.map { it.threshold })
        assertEquals(3, evaluateAlerts(snapshot(10.0, 300), reset.ledger, thresholds, true).events.size)
    }
    @Test fun staleNullAndOtherGroupsRemainIndependent() {
        assertTrue(evaluateAlerts(snapshot(0.0, stale = true), AlertLedger(), thresholds, true).events.isEmpty())
        assertTrue(evaluateAlerts(snapshot(null), AlertLedger(), thresholds, true).events.isEmpty())
        val first = evaluateAlerts(snapshot(0.0), AlertLedger(), thresholds, true)
        assertEquals(5, evaluateAlerts(snapshot(0.0, id = "other"), first.ledger, thresholds, true).events.size)
    }
    @Test fun persistedLedgerAndDisabledThresholdsDoNotReplay() {
        val initial = evaluateAlerts(snapshot(4.0), AlertLedger(), emptySet(), false)
        val saved = quotaJson.encodeToString(AlertLedger.serializer(), initial.ledger)
        val restored = quotaJson.decodeFromString<AlertLedger>(saved)
        assertTrue(evaluateAlerts(snapshot(4.0), restored, thresholds, true).events.isEmpty())
        assertEquals(listOf(0), evaluateAlerts(snapshot(0.0), restored, thresholds, true).events.map { it.threshold })
    }
}
