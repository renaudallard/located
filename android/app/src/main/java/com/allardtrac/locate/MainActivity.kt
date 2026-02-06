package com.allardtrac.locate

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.google.firebase.messaging.FirebaseMessaging

class MainActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "MainActivity"
        private const val RC_PERMISSIONS = 100
    }

    private lateinit var reg: RegistrationManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        reg = RegistrationManager(this)

        val serverUrlField = findViewById<EditText>(R.id.server_url)
        val deviceIdField = findViewById<EditText>(R.id.device_id)
        val deviceNameField = findViewById<EditText>(R.id.device_name)
        val registerBtn = findViewById<Button>(R.id.register_btn)
        val statusText = findViewById<TextView>(R.id.status_text)

        /* Restore saved values. */
        if (reg.isRegistered) {
            statusText.text = "Registered as: ${reg.deviceId}"
            serverUrlField.setText(reg.serverUrl ?: "")
            deviceIdField.setText(reg.deviceId ?: "")
            registerBtn.text = "Re-register"
        }

        registerBtn.setOnClickListener {
            val serverUrl = serverUrlField.text.toString().trimEnd('/')
            val deviceId = deviceIdField.text.toString().trim()
            val deviceName = deviceNameField.text.toString().trim().ifEmpty { null }

            if (serverUrl.isEmpty() || deviceId.isEmpty()) {
                Toast.makeText(this, "Server URL and Device ID required",
                    Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            registerBtn.isEnabled = false
            statusText.text = "Registering..."

            FirebaseMessaging.getInstance().token.addOnSuccessListener { token ->
                Thread {
                    val ok = reg.register(serverUrl, deviceId, token, deviceName)
                    runOnUiThread {
                        registerBtn.isEnabled = true
                        if (ok) {
                            statusText.text = "Registered as: $deviceId"
                            registerBtn.text = "Re-register"
                        } else {
                            statusText.text = "Registration failed"
                        }
                    }
                }.start()
            }.addOnFailureListener { e ->
                registerBtn.isEnabled = true
                statusText.text = "Failed to get FCM token"
                Log.e(TAG, "FCM token error", e)
            }
        }

        requestPermissions()
    }

    private fun requestPermissions() {
        val perms = mutableListOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION
        )

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            perms.add(Manifest.permission.POST_NOTIFICATIONS)
        }

        val needed = perms.filter {
            ActivityCompat.checkSelfPermission(this, it) !=
                PackageManager.PERMISSION_GRANTED
        }

        if (needed.isNotEmpty()) {
            ActivityCompat.requestPermissions(this,
                needed.toTypedArray(), RC_PERMISSIONS)
        } else {
            requestBackgroundLocation()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == RC_PERMISSIONS) {
            requestBackgroundLocation()
        }
    }

    private fun requestBackgroundLocation() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q &&
            ActivityCompat.checkSelfPermission(this,
                Manifest.permission.ACCESS_BACKGROUND_LOCATION) !=
                PackageManager.PERMISSION_GRANTED
        ) {
            ActivityCompat.requestPermissions(this,
                arrayOf(Manifest.permission.ACCESS_BACKGROUND_LOCATION),
                RC_PERMISSIONS + 1)
        }
    }
}
