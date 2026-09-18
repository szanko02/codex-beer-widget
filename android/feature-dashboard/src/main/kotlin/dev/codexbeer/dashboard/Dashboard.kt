package dev.codexbeer.dashboard

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import dev.codexbeer.sync.SyncRepository
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@Composable
fun Dashboard(repository: SyncRepository) {
    val state by repository.states.collectAsStateWithLifecycle()
    var pairing by remember { mutableStateOf("") }
    var message by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    var now by remember { mutableLongStateOf(System.currentTimeMillis() / 1000) }
    val scope = rememberCoroutineScope()
    LaunchedEffect(Unit) { while (true) { delay(1000); now = System.currentTimeMillis() / 1000 } }
    fun run(action: suspend () -> Unit) {
        if (busy) return
        scope.launch {
            busy = true
            try { action(); message = "Готово" } catch (_: Exception) { message = "Нет соединения или pairing недействителен. Проверьте адрес, сеть и сертификат." }
            finally { busy = false }
        }
    }
    MaterialTheme {
        Surface(Modifier.fillMaxSize()) {
            Column(Modifier.safeDrawingPadding().padding(20.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Codex Quota", style = MaterialTheme.typography.headlineMedium)
                Text(if (state.stale(now)) "Последние данные · связь не подтверждена" else "Подключено")
                if (state.receivedAt > 0) Text("Получено ${maxOf(0, now - state.receivedAt)} сек. назад")
                if (state.snapshot?.groups.isNullOrEmpty()) Text("Нет данных")
                state.snapshot?.groups?.forEach { (_, group) ->
                    Text(group.name + if (group.stale) " · устарело" else "", style = MaterialTheme.typography.titleMedium)
                    group.windows.sortedBy { it.windowMinutes ?: Long.MAX_VALUE }.forEach { window ->
                        Text("${window.windowMinutes?.let { "$it мин" } ?: window.slot}: ${window.remaining?.let { "%.0f%%".format(it) } ?: "нет данных"}")
                        window.remaining?.let { LinearProgressIndicator(progress = { (it / 100).toFloat() }, modifier = Modifier.fillMaxWidth()) }
                        window.resetsAt?.let { Text("Сброс: " + DateTimeFormatter.ofPattern("dd.MM HH:mm").withZone(ZoneId.systemDefault()).format(Instant.ofEpochSecond(it))) }
                    }
                }
                Button(onClick = { run { repository.refresh() } }, enabled = !busy) { Text("Обновить") }
                OutlinedTextField(value = pairing, onValueChange = { if (it.length <= 4096) pairing = it }, label = { Text("Данные pairing QR") }, modifier = Modifier.fillMaxWidth())
                Button(onClick = { run { repository.pair(pairing); pairing = "" } }, enabled = !busy && pairing.isNotBlank()) { Text("Подключить") }
                OutlinedButton(onClick = { run { repository.unpair() } }, enabled = !busy) { Text("Отключить телефон") }
                Text(message)
            }
        }
    }
}
