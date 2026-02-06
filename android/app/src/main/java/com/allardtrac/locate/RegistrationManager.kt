package com.allardtrac.locate

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject

/**
 * Handles device registration with the located server.
 * Stores the device secret locally for authenticating location callbacks.
 */
class RegistrationManager(private val context: Context) {

    companion object {
        private const val TAG = "RegistrationManager"
        private const val PREFS = "allardtrac"
        private const val KEY_DEVICE_ID = "device_id"
        private const val KEY_SECRET = "device_secret"
        private const val KEY_SERVER_URL = "server_url"
    }

    private val prefs: SharedPreferences =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private val client = OkHttpClient()

    val deviceId: String? get() = prefs.getString(KEY_DEVICE_ID, null)
    val secret: String? get() = prefs.getString(KEY_SECRET, null)
    val serverUrl: String? get() = prefs.getString(KEY_SERVER_URL, null)

    val isRegistered: Boolean get() = deviceId != null && secret != null

    /**
     * Register this device with the server.
     * Must be called from a background thread.
     */
    fun register(serverUrl: String, deviceId: String, fcmToken: String, name: String?): Boolean {
        val json = JSONObject().apply {
            put("id", deviceId)
            put("token", fcmToken)
            put("platform", "android")
            if (name != null) put("name", name)
        }

        val body = json.toString()
            .toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url("$serverUrl/api/register")
            .post(body)
            .build()

        return try {
            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                val respJson = JSONObject(response.body!!.string())
                val secret = respJson.getString("secret")

                prefs.edit()
                    .putString(KEY_DEVICE_ID, deviceId)
                    .putString(KEY_SECRET, secret)
                    .putString(KEY_SERVER_URL, serverUrl)
                    .apply()

                Log.i(TAG, "Registered as $deviceId")
                true
            } else {
                Log.e(TAG, "Registration failed: ${response.code}")
                false
            }
        } catch (e: Exception) {
            Log.e(TAG, "Registration error", e)
            false
        }
    }

    fun clear() {
        prefs.edit().clear().apply()
    }
}
