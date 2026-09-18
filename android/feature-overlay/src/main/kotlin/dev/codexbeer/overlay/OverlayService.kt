package dev.codexbeer.overlay

import android.animation.ValueAnimator
import android.app.*
import android.content.*
import android.content.pm.ServiceInfo
import android.content.res.Configuration
import android.graphics.*
import android.os.*
import android.provider.Settings
import android.view.*
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import dev.codexbeer.model.LocalState
import dev.codexbeer.notifications.StatusNotification
import dev.codexbeer.sync.SyncRepository
import kotlinx.coroutines.*

class OverlayService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private lateinit var manager: WindowManager
    private lateinit var mug: MugView
    private lateinit var layout: WindowManager.LayoutParams
    private var attached = false
    private var current = LocalState()
    private val preferences by lazy { getSharedPreferences("overlay", MODE_PRIVATE) }
    private val screen = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (intent.action == Intent.ACTION_SCREEN_OFF) mug.cancelAnimation() else update()
        }
    }
    override fun onBind(intent: Intent?) = null
    override fun onCreate() {
        super.onCreate()
        manager = getSystemService(WindowManager::class.java)
        mug = MugView(this)
        layout = WindowManager.LayoutParams(160, 180, WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL, PixelFormat.TRANSLUCENT).apply {
            gravity = Gravity.TOP or Gravity.START
            x = preferences.getInt("x", 30); y = preferences.getInt("y", 100)
        }
        ContextCompat.registerReceiver(this, screen, IntentFilter().apply {
            addAction(Intent.ACTION_SCREEN_OFF); addAction(Intent.ACTION_SCREEN_ON)
        }, ContextCompat.RECEIVER_NOT_EXPORTED)
    }
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == "stop" || !Settings.canDrawOverlays(this)) { stopSelf(); return START_NOT_STICKY }
        if (!attached) {
            StatusNotification.channels(this)
            val stop = PendingIntent.getService(this, 9, Intent(this, OverlayService::class.java).setAction("stop"), PendingIntent.FLAG_IMMUTABLE)
            val notification = NotificationCompat.Builder(this, StatusNotification.STATUS_CHANNEL)
                .setSmallIcon(dev.codexbeer.notifications.R.drawable.ic_quota).setContentTitle("Плавающий Codex Quota")
                .setContentText("Двойное нажатие — скрыть; удержание — настройки")
                .setOngoing(true).setOnlyAlertOnce(true).addAction(0, "Скрыть", stop).build()
            if (Build.VERSION.SDK_INT >= 34) startForeground(101, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
            else startForeground(101, notification)
            resize()
            try { manager.addView(mug, layout); attached = true; running = true }
            catch (_: RuntimeException) { stopSelf(); return START_NOT_STICKY }
            gestures()
            scope.launch { SyncRepository.get(this@OverlayService).states.collect { current = it; update() } }
        } else { resize(); update() }
        return START_NOT_STICKY
    }
    private fun resize() {
        val density = resources.displayMetrics.density
        layout.height = (preferences.getFloat("size", 140f).coerceIn(80f, 240f) * density).toInt()
        layout.width = (layout.height * 0.85).toInt()
        clamp()
        mug.alpha = preferences.getFloat("opacity", 0.85f).coerceIn(0.2f, 1f)
        if (attached) manager.updateViewLayout(mug, layout)
    }
    private fun clamp() {
        val metrics = resources.displayMetrics
        layout.x = layout.x.coerceIn(0, maxOf(0, metrics.widthPixels - layout.width))
        layout.y = layout.y.coerceIn(0, maxOf(0, metrics.heightPixels - layout.height))
    }
    private fun update() {
        val group = current.snapshot?.groups?.get("codex") ?: current.snapshot?.groups?.values?.firstOrNull()
        val windows = group?.windows?.sortedBy { it.windowMinutes ?: Long.MAX_VALUE }.orEmpty()
        val selected = windows.getOrNull(preferences.getInt("window", 0)) ?: windows.firstOrNull()
        mug.show(selected?.remaining, current.stale(System.currentTimeMillis() / 1000) || group?.stale == true,
            preferences.getBoolean("ring", false), preferences.getBoolean("animate", true) && getSystemService(PowerManager::class.java).isInteractive)
    }
    private fun gestures() {
        val detector = GestureDetector(this, object : GestureDetector.SimpleOnGestureListener() {
            override fun onDown(e: MotionEvent) = true
            override fun onSingleTapConfirmed(e: MotionEvent): Boolean {
                mug.performClick()
                preferences.edit().putInt("window", 1 - preferences.getInt("window", 0)).apply(); update(); return true
            }
            override fun onDoubleTap(e: MotionEvent): Boolean { stopSelf(); return true }
            override fun onLongPress(e: MotionEvent) {
                packageManager.getLaunchIntentForPackage(packageName)?.let { startActivity(it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) }
            }
        })
        var originX = 0f; var originY = 0f; var oldX = 0; var oldY = 0
        mug.setOnTouchListener { _, event ->
            detector.onTouchEvent(event)
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> { originX = event.rawX; originY = event.rawY; oldX = layout.x; oldY = layout.y }
                MotionEvent.ACTION_MOVE -> {
                    layout.x = oldX + (event.rawX - originX).toInt(); layout.y = oldY + (event.rawY - originY).toInt()
                    clamp(); if (attached) manager.updateViewLayout(mug, layout)
                }
                MotionEvent.ACTION_UP -> preferences.edit().putInt("x", layout.x).putInt("y", layout.y).apply()
            }
            true
        }
    }
    override fun onConfigurationChanged(newConfig: Configuration) { super.onConfigurationChanged(newConfig); resize() }
    override fun onDestroy() {
        running = false; scope.cancel(); mug.cancelAnimation()
        unregisterReceiver(screen)
        if (attached) manager.removeView(mug)
        super.onDestroy()
    }
    companion object { @Volatile var running = false; private set }
}

private class MugView(context: Context) : View(context) {
    init { contentDescription = "Codex: нажмите для смены лимита; дважды — скрыть; удерживайте для настроек" }
    override fun performClick(): Boolean { super.performClick(); return true }
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private var level = 0f
    private var known = false
    private var stale = true
    private var ring = false
    private var target: Double? = null
    private var animator: ValueAnimator? = null
    fun cancelAnimation() { animator?.cancel(); if (known) level = (target ?: 0.0).toFloat(); invalidate() }
    fun show(value: Double?, outdated: Boolean, ringMode: Boolean, animate: Boolean) {
        val changed = target != value
        stale = outdated; ring = ringMode
        if (changed) {
            animator?.cancel()
            val wasKnown = known
            known = value != null; target = value
            val next = (value ?: 0.0).toFloat()
            if (animate && wasKnown && known) {
                animator = ValueAnimator.ofFloat(level, next).apply {
                    duration = 800
                    addUpdateListener { level = it.animatedValue as Float; invalidate() }
                    start()
                }
            } else level = next
        }
        if (!animate) cancelAnimation()
        invalidate()
    }
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.save(); canvas.scale(width / 120f, height / 140f)
        paint.color = Color.argb(210, 30, 36, 43); paint.style = Paint.Style.FILL
        canvas.drawRoundRect(RectF(0f, 0f, 120f, 140f), 14f, 14f, paint)
        if (ring) {
            paint.style = Paint.Style.STROKE; paint.strokeWidth = 9f; paint.color = Color.DKGRAY
            canvas.drawOval(RectF(16f, 12f, 104f, 100f), paint)
            if (known) { paint.color = Color.rgb(237, 164, 41); canvas.drawArc(RectF(16f, 12f, 104f, 100f), -90f, level * 3.6f, false, paint) }
        } else {
            paint.style = Paint.Style.STROKE; paint.strokeWidth = 5f; paint.color = Color.LTGRAY
            canvas.drawRoundRect(RectF(81f, 32f, 108f, 82f), 8f, 8f, paint)
            canvas.drawRoundRect(RectF(19f, 13f, 85f, 104f), 8f, 8f, paint)
            if (known && level > 0) {
                paint.style = Paint.Style.FILL; paint.color = Color.rgb(237, 164, 41)
                val top = 98f - level * .78f
                canvas.drawRect(24f, top, 80f, 98f, paint)
                paint.color = Color.rgb(255, 240, 205); canvas.drawRect(24f, top, 80f, minOf(98f, top + 4f), paint)
                paint.color = Color.argb(130, 255, 255, 255)
                for (i in 0..5) { val y = 91f - i * 11; if (y > top + 6) canvas.drawCircle(34f + (i % 3) * 15f, y, 1.8f, paint) }
            }
        }
        paint.style = Paint.Style.FILL; paint.color = Color.WHITE; paint.textSize = 18f; paint.textAlign = Paint.Align.CENTER
        canvas.drawText((if (known) "%.0f%%".format(level) else "—") + if (stale) " *" else "", 60f, 127f, paint)
        canvas.restore()
    }
}
