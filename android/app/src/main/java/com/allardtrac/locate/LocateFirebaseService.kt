package com.allardtrac.locate

import android.app.Notification
import android.content.pm.ServiceInfo
import android.util.Log
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/**
 * Handles incoming FCM data messages.
 * On receiving a locate request: starts foreground service, gets GPS fix,
 * POSTs location back to the server, then stops.
 */
class LocateFirebaseService : FirebaseMessagingService() {

    companion object {
        private const val TAG = "LocateService"
        private const val NOTIFICATION_ID = 1
    }

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .writeTimeout(10, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .build()

    override fun onMessageReceived(message: RemoteMessage) {
        val data = message.data
        val requestId = data["request_id"] ?: return
        val serverUrl = data["server_url"] ?: return
        val deviceId = data["device_id"] ?: return

        Log.i(TAG, "Locate request: $requestId for device $deviceId")

        /* Start foreground service for background location access. */
        val notification = Notification.Builder(this, App.CHANNEL_ID)
            .setContentTitle("Locating device...")
            .setSmallIcon(android.R.drawable.ic_menu_mylocation)
            .build()

        startForeground(NOTIFICATION_ID, notification,
            ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION)

        try {
            val helper = LocationHelper(this)
            val location = helper.getLocation()

            if (location != null) {
                postLocation(serverUrl, deviceId, requestId,
                    location.latitude, location.longitude, location.accuracy)
            } else {
                Log.e(TAG, "Could not get location")
            }
        } finally {
            stopForeground(STOP_FOREGROUND_REMOVE)
        }
    }

    override fun onNewToken(token: String) {
        Log.i(TAG, "New FCM token: $token")
        /*
         * If already registered, the user should re-register with the new token.
         * For now, just log it. A production app would update the server.
         */
    }

    private fun postLocation(
        serverUrl: String, deviceId: String, requestId: String,
        lat: Double, lng: Double, accuracy: Float
    ) {
        val reg = RegistrationManager(this)
        val secret = reg.secret ?: run {
            Log.e(TAG, "No device secret stored")
            return
        }

        val json = JSONObject().apply {
            put("request_id", requestId)
            put("lat", lat)
            put("lng", lng)
            put("accuracy", accuracy.toDouble())
        }

        val body = json.toString()
            .toRequestBody("application/json".toMediaType())

        val request = Request.Builder()
            .url("$serverUrl/api/location/$deviceId")
            .header("X-Device-Secret", secret)
            .post(body)
            .build()

        try {
            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                Log.i(TAG, "Location posted successfully")
            } else {
                Log.e(TAG, "Location post failed: ${response.code}")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Location post error", e)
        }
    }
}
