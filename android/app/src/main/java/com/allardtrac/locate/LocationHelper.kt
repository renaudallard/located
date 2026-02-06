package com.allardtrac.locate

import android.annotation.SuppressLint
import android.content.Context
import android.location.Location
import android.os.Looper
import android.util.Log
import com.google.android.gms.location.FusedLocationProviderClient
import com.google.android.gms.location.LocationCallback
import com.google.android.gms.location.LocationRequest
import com.google.android.gms.location.LocationResult
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.Priority
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * Gets a single high-accuracy GPS fix.
 * Falls back to last known location if fresh fix times out.
 */
class LocationHelper(context: Context) {

    companion object {
        private const val TAG = "LocationHelper"
        private const val TIMEOUT_SEC = 15L
    }

    private val fusedClient: FusedLocationProviderClient =
        LocationServices.getFusedLocationProviderClient(context)

    /**
     * Get a location fix. Blocks the calling thread up to TIMEOUT_SEC seconds.
     * Must be called from a background thread.
     */
    @SuppressLint("MissingPermission")
    fun getLocation(): Location? {
        val latch = CountDownLatch(1)
        var result: Location? = null

        val request = LocationRequest.Builder(
            Priority.PRIORITY_HIGH_ACCURACY, 1000L
        ).setMaxUpdates(1).build()

        val callback = object : LocationCallback() {
            override fun onLocationResult(locationResult: LocationResult) {
                result = locationResult.lastLocation
                latch.countDown()
            }
        }

        fusedClient.requestLocationUpdates(request, callback, Looper.getMainLooper())

        try {
            if (!latch.await(TIMEOUT_SEC, TimeUnit.SECONDS)) {
                Log.w(TAG, "GPS timeout, trying last known location")
                fusedClient.removeLocationUpdates(callback)
                result = getLastKnown()
            }
        } catch (e: InterruptedException) {
            fusedClient.removeLocationUpdates(callback)
        }

        return result
    }

    @SuppressLint("MissingPermission")
    private fun getLastKnown(): Location? {
        return try {
            val task = fusedClient.lastLocation
            val latch = CountDownLatch(1)
            var loc: Location? = null
            task.addOnSuccessListener { location ->
                loc = location
                latch.countDown()
            }.addOnFailureListener {
                latch.countDown()
            }
            latch.await(5, TimeUnit.SECONDS)
            loc
        } catch (e: Exception) {
            Log.e(TAG, "Failed to get last known location", e)
            null
        }
    }
}
