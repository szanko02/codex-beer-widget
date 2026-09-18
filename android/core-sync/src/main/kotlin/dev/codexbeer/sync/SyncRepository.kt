package dev.codexbeer.sync

import android.content.Context
import dev.codexbeer.data.*
import dev.codexbeer.model.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.serialization.json.*
import okhttp3.*
import okhttp3.HttpUrl.Companion.toHttpUrl
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit
import java.io.ByteArrayOutputStream

class SyncRepository private constructor(context: Context) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val store = LocalStateStore(context)
    private val credentials = CredentialStore(context)
    private val mutex = Mutex()
    private val client = OkHttpClient.Builder().callTimeout(10, TimeUnit.SECONDS)
        .followRedirects(false).followSslRedirects(false).build()
    val states = store.states.stateIn(scope, SharingStarted.Eagerly, LocalState())
    private val _changes = MutableSharedFlow<LocalState>(extraBufferCapacity = 1)
    val changes = _changes.asSharedFlow()
    private val startup = scope.launch { store.disconnected() }

    private fun origin(value: String): HttpUrl = value.toHttpUrl().also {
        require(it.isHttps && it.username.isEmpty() && it.password.isEmpty() &&
            it.encodedPath == "/" && it.query == null && it.fragment == null) { "An HTTPS relay origin is required" }
    }
    private fun request(base: String, path: String, token: String? = null, body: JsonObject? = null): String {
        val builder = Request.Builder().url(origin(base).resolve(path)!!)
        if (token != null) builder.header("Authorization", "Bearer $token")
        if (body != null) builder.post(body.toString().toRequestBody("application/json".toMediaType()))
        client.newCall(builder.build()).execute().use { response ->
            check(response.isSuccessful) { "Relay HTTP ${response.code}" }
            val source = response.body ?: error("Empty relay response")
            require(source.contentLength() <= 65536)
            val output = ByteArrayOutputStream()
            val buffer = ByteArray(4096)
            val input = source.byteStream()
            while (true) {
                val count = input.read(buffer)
                if (count == -1) break
                require(output.size() + count <= 65536)
                output.write(buffer, 0, count)
            }
            return output.toString("UTF-8")
        }
    }
    suspend fun pair(qr: String) = withContext(Dispatchers.IO) {
        startup.join()
        mutex.withLock {
            require(qr.toByteArray().size <= 4096)
            val data = quotaJson.parseToJsonElement(qr).jsonObject
            require(data["version"]?.jsonPrimitive?.int == 1)
            val base = data.getValue("origin").jsonPrimitive.content
            origin(base)
            val id = data.getValue("deviceId").jsonPrimitive.content
            val secret = data.getValue("pairingSecret").jsonPrimitive.content
            require(id.matches(Regex("[A-Za-z0-9_-]{16,128}")) && secret.matches(Regex("[A-Za-z0-9_-]{43,128}")))
            require(data.getValue("expiresAt").jsonPrimitive.long > System.currentTimeMillis() / 1000) { "Pairing expired" }
            val response = quotaJson.parseToJsonElement(request(base, "/v1/pair/redeem", body = buildJsonObject {
                put("deviceId", id); put("pairingSecret", secret)
            })).jsonObject
            require(response.getValue("deviceId").jsonPrimitive.content == id)
            val reader = response.getValue("readerSecret").jsonPrimitive.content
            require(reader.matches(Regex("[A-Za-z0-9_-]{43,128}")))
            store.clear()
            credentials.write(Credentials(base, id, reader))
        }
        refresh()
    }
    suspend fun refresh() = withContext(Dispatchers.IO) {
        startup.join()
        mutex.withLock {
            try {
                val config = credentials.read() ?: return@withLock
                val snapshot = decodeSnapshot(request(config.origin, "/v1/devices/${config.deviceId}/state", config.readerSecret))
                if (store.accept(snapshot)) _changes.emit(LocalState(snapshot, System.currentTimeMillis() / 1000, ConnectionState.CONNECTED))
            } catch (error: Exception) {
                store.disconnected()
                if (error is CancellationException) throw error
                throw IllegalStateException("Sync unavailable; last quota retained", error)
            }
        }
    }
    suspend fun unpair() = withContext(Dispatchers.IO) {
        startup.join()
        mutex.withLock {
            credentials.read()?.let { config ->
                request(config.origin, "/v1/unpair", config.readerSecret, buildJsonObject {
                    put("deviceId", config.deviceId); put("scope", "subscriber")
                })
            }
            credentials.clear()
            store.clear()
        }
    }
    companion object {
        @Volatile private var instance: SyncRepository? = null
        fun get(context: Context): SyncRepository = instance ?: synchronized(this) {
            instance ?: SyncRepository(context.applicationContext).also { instance = it }
        }
    }
}
