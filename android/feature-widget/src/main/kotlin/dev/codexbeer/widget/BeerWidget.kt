package dev.codexbeer.widget

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.glance.*
import androidx.glance.action.clickable
import androidx.glance.appwidget.*
import androidx.glance.appwidget.action.actionStartActivity
import androidx.glance.layout.*
import androidx.glance.text.Text
import androidx.glance.text.TextStyle
import androidx.glance.unit.ColorProvider
import dev.codexbeer.data.LocalStateStore
import dev.codexbeer.model.LocalState
import kotlinx.coroutines.flow.first
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

class BeerWidget : GlanceAppWidget() {
    override val sizeMode = SizeMode.Responsive(setOf(DpSize(56.dp, 56.dp), DpSize(120.dp, 56.dp), DpSize(120.dp, 120.dp)))
    override suspend fun provideGlance(context: Context, id: GlanceId) {
        val state = LocalStateStore(context).states.first()
        provideContent {
            val states = remember { LocalStateStore(context).states }
            val current by states.collectAsState(initial = state)
            Content(context, current)
        }
    }
    @Composable private fun Content(context: Context, state: LocalState) {
        val size = LocalSize.current
        val group = state.snapshot?.groups?.get("codex") ?: state.snapshot?.groups?.values?.firstOrNull()
        val windows = group?.windows?.sortedBy { it.windowMinutes ?: Long.MAX_VALUE }.orEmpty()
        val primary = windows.firstOrNull()
        val percent = primary?.remaining?.let { "%.0f%%".format(it) } ?: "—"
        val now = System.currentTimeMillis() / 1000
        val stale = state.stale(now) || group?.stale == true
        val launch = context.packageManager.getLaunchIntentForPackage(context.packageName)!!
        Column(GlanceModifier.fillMaxSize().background(Color(0xff20252b)).padding(6.dp).clickable(actionStartActivity(launch)),
            verticalAlignment = Alignment.CenterVertically, horizontalAlignment = Alignment.CenterHorizontally) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Image(ImageProvider(mug(primary?.remaining)), contentDescription = "Остаток $percent",
                    modifier = GlanceModifier.size(if (size.width >= 110.dp) 42.dp else 28.dp))
                Text(percent + if (stale) " *" else "", style = TextStyle(color = ColorProvider(Color.White), fontSize = 16.sp))
            }
            if (size.width >= 110.dp) {
                Text(primary?.resetsAt?.let { "Сброс " + time(it) } ?: "Сброс неизвестен", style = smallText())
            }
            if (size.height >= 110.dp) {
                windows.getOrNull(1)?.let { Text("${it.windowMinutes ?: "?"} мин: ${it.remaining?.let { r -> "%.0f%%".format(r) } ?: "—"}", style = smallText()) }
                Text(group?.sourceUpdatedAt?.let { "Данные " + time(it) } ?: "Нет данных", style = smallText())
                if (stale) Text("Устарело", style = smallText())
            }
        }
    }
    private fun smallText() = TextStyle(color = ColorProvider(Color(0xffd2d7dd)), fontSize = 11.sp)
    private fun time(seconds: Long) = DateTimeFormatter.ofPattern("dd.MM HH:mm").withZone(ZoneId.systemDefault()).format(Instant.ofEpochSecond(seconds))
    private fun mug(remaining: Double?): Bitmap {
        val bitmap = Bitmap.createBitmap(120, 120, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        paint.color = android.graphics.Color.LTGRAY; paint.style = Paint.Style.STROKE; paint.strokeWidth = 7f
        canvas.drawRoundRect(RectF(82f, 30f, 109f, 85f), 10f, 10f, paint)
        canvas.drawRoundRect(RectF(15f, 15f, 85f, 108f), 9f, 9f, paint)
        if (remaining != null) {
            paint.style = Paint.Style.FILL; paint.color = android.graphics.Color.rgb(237, 164, 41)
            canvas.drawRect(21f, (101 - remaining.coerceIn(0.0, 100.0) * 0.8).toFloat(), 79f, 101f, paint)
        }
        return bitmap
    }
}
class BeerWidgetReceiver : GlanceAppWidgetReceiver() { override val glanceAppWidget: GlanceAppWidget = BeerWidget() }

object WidgetUpdates {
    suspend fun update(context: Context) { BeerWidget().updateAll(context) }
}
