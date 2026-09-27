package dev.codexbeer.dashboard

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.repeatOnLifecycle
import dev.codexbeer.sync.SyncRepository
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

@Composable
fun Dashboard(repository: SyncRepository, onScan: () -> Unit, actions: @Composable () -> Unit = {}) {
    val state by repository.states.collectAsStateWithLifecycle()
    val paired by repository.paired.collectAsStateWithLifecycle()
    var settings by rememberSaveable { mutableStateOf(false) }
    var disconnect by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    var now by remember { mutableLongStateOf(System.currentTimeMillis() / 1000) }
    val scope = rememberCoroutineScope()
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    LaunchedEffect(lifecycle, state.receivedAt, settings) {
        if (state.receivedAt > 0 && !settings) lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
            while (true) { now = System.currentTimeMillis() / 1000; delay(15000) }
        }
    }
    fun run(action: suspend () -> Unit) {
        if (busy) return
        scope.launch {
            busy = true
            try { action(); message = "Готово" }
            catch (error: Exception) {
                if (error is CancellationException) throw error
                message = "Не удалось связаться с компьютером. Проверьте интернет и повторите. Последние данные сохранены."
            } finally { busy = false }
        }
    }
    Surface(Modifier.fillMaxSize()) {
        Column(Modifier.safeDrawingPadding().padding(horizontal = 24.dp).verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(20.dp)) {
            Row(Modifier.fillMaxWidth().padding(top = 12.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Codex Beer", style = MaterialTheme.typography.titleLarge)
                TextButton(onClick = { settings = !settings }) { Text(if (settings) "Готово" else "Настройки") }
            }
            if (settings) {
                Text("Настройки", style = MaterialTheme.typography.headlineLarge)
                Text("Оформление следует теме телефона", color = MaterialTheme.colorScheme.onSurfaceVariant)
                if (paired) OutlinedButton(onClick = onScan, enabled = !busy) { Text("Подключить другой компьютер") }
                actions()
                if (paired) OutlinedButton(onClick = { disconnect = true }, enabled = !busy) { Text("Отключить компьютер") }
            } else if (!paired) {
                Spacer(Modifier.height(32.dp))
                Text("Ваш запас\nна сегодня", style = MaterialTheme.typography.displaySmall)
                Text("Подключите компьютер один раз. Остаток лимитов будет обновляться автоматически.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
                    shape = RoundedCornerShape(24.dp)) {
                    Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        Text("Подключение по QR", style = MaterialTheme.typography.titleLarge)
                        Text("Откройте QR подключения на компьютере и отсканируйте его камерой. Вводить код не нужно.")
                        Button(onClick = onScan, modifier = Modifier.fillMaxWidth()) { Text("Подключить компьютер") }
                    }
                }
            } else {
                Text("Осталось в запасе", style = MaterialTheme.typography.headlineLarge)
                Text(if (state.stale(now)) "Последние данные · нет свежего обновления" else "Подключено · обновляется автоматически",
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                if (state.receivedAt > 0) Text("Обновлено ${maxOf(0, now - state.receivedAt)} сек. назад",
                    style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                if (state.snapshot?.groups.isNullOrEmpty()) Text("Ждём первое обновление с компьютера")
                state.snapshot?.groups?.forEach { (_, group) ->
                    Text(group.name + if (group.stale) " · устарело" else "", style = MaterialTheme.typography.titleMedium)
                    group.windows.sortedBy { it.windowMinutes ?: Long.MAX_VALUE }.forEach { window ->
                        Card(Modifier.fillMaxWidth(), shape = RoundedCornerShape(24.dp),
                            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)) {
                            Column(Modifier.padding(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                                val duration = window.windowMinutes
                                Text(when { duration == null -> if (window.slot == "primary") "Основной лимит" else "Дополнительный лимит"
                                    duration % 1440L == 0L -> "${duration / 1440} дн."
                                    duration % 60L == 0L -> "${duration / 60} ч."
                                    else -> "$duration мин." }, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                Text(window.remaining?.let { "%.0f%%".format(it) } ?: "Нет данных", style = MaterialTheme.typography.displayMedium)
                                window.remaining?.let { LinearProgressIndicator(progress = { (it / 100).toFloat() }, modifier = Modifier.fillMaxWidth(),
                                    color = MaterialTheme.colorScheme.primary, trackColor = MaterialTheme.colorScheme.outline.copy(alpha = .2f)) }
                                window.resetsAt?.let { Text("Сброс " + DateTimeFormatter.ofPattern("dd.MM · HH:mm").withZone(ZoneId.systemDefault()).format(Instant.ofEpochSecond(it)),
                                    style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant) }
                            }
                        }
                    }
                }
                FilledTonalButton(onClick = { run { repository.refresh() } }, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
                    Text(if (busy) "Обновляем…" else "Обновить сейчас")
                }
            }
            if (message.isNotEmpty()) Text(message, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(24.dp))
        }
    }
    if (disconnect) AlertDialog(onDismissRequest = { disconnect = false }, title = { Text("Отключить компьютер?") },
        text = { Text("Для повторного подключения потребуется новый QR.") },
        confirmButton = { TextButton(onClick = { disconnect = false; run { repository.unpair() } }) { Text("Отключить") } },
        dismissButton = { TextButton(onClick = { disconnect = false }) { Text("Отмена") } })
}
