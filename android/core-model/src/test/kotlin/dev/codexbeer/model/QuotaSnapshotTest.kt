package dev.codexbeer.model

import java.io.File
import kotlinx.serialization.json.*
import kotlin.test.*

class QuotaSnapshotTest {
    private fun normalized(element: JsonElement): JsonElement = when (element) {
        is JsonObject -> JsonObject(element.mapValues { normalized(it.value) })
        is JsonArray -> JsonArray(element.map { normalized(it) })
        is JsonPrimitive -> if (!element.isString && element.doubleOrNull != null) JsonPrimitive(element.double) else element
    }
    @Test fun sharedFixtures() {
        val fixtures = quotaJson.parseToJsonElement(File(System.getProperty("quota.fixtures")).readText()).jsonArray
        fixtures.forEach {
            val expected = it.jsonObject.getValue("expected")
            val state = decodeSnapshot(expected.toString())
            assertEquals(normalized(expected), normalized(quotaJson.encodeToJsonElement(state)))
        }
    }
    @Test fun resetRequiresIdentityIncreaseAndNewDeadline() {
        val old = QuotaWindow("primary", 5.0, 300, 100)
        assertTrue(confirmedReset(old, old.copy(remaining = 100.0, resetsAt = 200)))
        assertFalse(confirmedReset(old, old.copy(remaining = 100.0)))
        assertFalse(confirmedReset(old, old.copy(slot = "secondary", remaining = 100.0, resetsAt = 200)))
        assertFalse(confirmedReset(old, old.copy(remaining = null, resetsAt = 200)))
    }
    @Test fun invalidSnapshotRejected() {
        val snapshot = QuotaSnapshot(1, 1, null, true, mapOf("codex" to QuotaGroup("codex", null, true,
            listOf(QuotaWindow("primary", 101.0, 300, null)))))
        assertFailsWith<IllegalArgumentException> { snapshot.validated() }
        assertFails { decodeSnapshot("""{"version":2,"revision":1,"sourceUpdatedAt":null,"stale":true,"groups":{}}""") }
        assertFails { decodeSnapshot("""{"version":1,"revision":1,"sourceUpdatedAt":null,"stale":true,"groups":{},"token":"x"}""") }
    }
    @Test fun connectionLossRetainsPercentage() {
        val quota = QuotaSnapshot(1, 1, 100, false, mapOf("codex" to QuotaGroup("codex", 100, false,
            listOf(QuotaWindow("primary", 64.0, 300, null)))))
        val state = LocalState(quota, 100, ConnectionState.CONNECTED)
        assertFalse(state.stale(110))
        assertTrue(state.stale(701))
        assertTrue(state.stale(99))
        assertEquals(64.0, state.copy(connection = ConnectionState.STALE).snapshot!!.groups["codex"]!!.windows[0].remaining)
    }
}
