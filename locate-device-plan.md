# Locate Device — Implementation Plan

## Overview

A minimal "ping and respond" device locator. The server sends a push notification to a registered device, the device wakes up, grabs its GPS coordinates, and sends them back. **No location history is stored.** The server displays the result once and discards it.

---

## Architecture

```
[Admin CLI / Web UI]
        |
        v
[C Server (HTTP API + Push Dispatch)]
        |
        +---> FCM (Android push)
        +---> APNs (iOS push)
        |
[Device wakes up, gets GPS fix]
        |
        v
[Device POSTs location back to C server]
        |
        v
[Server prints/returns location, discards it]
```

---

## Component 1: C Server

### Description
A lightweight HTTP server written in C. Handles device registration, sends locate commands via push notifications, and receives location responses from devices.

### Dependencies
- **libmicrohttpd** — Embedded HTTP server (GNU, well-maintained, minimal)
- **libcurl** — For outbound HTTPS to FCM and APNs
- **cJSON** — JSON parsing/generation (single-file, MIT license, https://github.com/DaveGamble/cJSON)
- **OpenSSL** — For JWT signing (APNs auth) and optional TLS termination
- **SQLite3** — Minimal storage for device registry only (device_id, push_token, platform)

### Build
```
gcc -o located src/*.c -lmicrohttpd -lcurl -lcjson -lsqlite3 -lssl -lcrypto -lpthread
```

### API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/register` | Device registers itself (push token + platform) |
| DELETE | `/api/register/{device_id}` | Unregister a device |
| GET | `/api/devices` | List registered devices |
| POST | `/api/locate/{device_id}` | Trigger locate — sends push, waits for response |
| POST | `/api/location/{device_id}` | Device posts back its GPS coordinates |

### File Structure
```
server/
├── src/
│   ├── main.c              # Entry point, arg parsing, server startup
│   ├── http_server.c/.h    # libmicrohttpd setup, route dispatch
│   ├── routes.c/.h         # Handler functions for each endpoint
│   ├── push_fcm.c/.h      # Send FCM push via HTTP v1 API (libcurl)
│   ├── push_apns.c/.h     # Send APNs push via HTTP/2 (libcurl)
│   ├── device_db.c/.h     # SQLite3 device registry (CRUD)
│   ├── auth.c/.h           # Simple API key or token auth for admin endpoints
│   ├── jwt.c/.h            # JWT generation for APNs authentication
│   └── config.c/.h         # Load config from file or env vars
├── config.example.json     # Example config (FCM key, APNs cert paths, listen port)
├── Makefile
└── README.md
```

### Locate Flow (Detail)

1. Admin calls `POST /api/locate/{device_id}`
2. Server looks up device in SQLite — gets push token and platform
3. Server sends push notification:
   - **Android**: HTTP POST to `https://fcm.googleapis.com/v1/projects/{project}/messages:send` with high-priority data message containing a nonce/request_id
   - **iOS**: HTTP/2 POST to `https://api.push.apple.com/3/device/{token}` with `content-available: 1` silent push containing the same nonce
4. Server creates a **pending request** in memory (a simple hashmap: `request_id → {condition_variable, result}`)
5. Server blocks on the condition variable with a **30-second timeout**
6. When device POSTs to `/api/location/{device_id}`, the handler:
   - Matches the nonce/request_id
   - Signals the condition variable with the lat/lng
7. The blocked `/api/locate` handler wakes up and returns the location as JSON
8. Location is **not stored** — it only exists in the HTTP response

### Pending Request Store (In-Memory)
```c
// No disk storage — just a mutex-protected linked list or small hashmap
typedef struct {
    char request_id[37];    // UUID
    double latitude;
    double longitude;
    float accuracy;
    int completed;          // flag
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} pending_request_t;
```

### Configuration (config.json)
```json
{
    "listen_port": 8080,
    "api_key": "your-admin-api-key",
    "db_path": "devices.db",
    "fcm": {
        "project_id": "your-firebase-project",
        "service_account_key_path": "/path/to/firebase-sa.json"
    },
    "apns": {
        "key_path": "/path/to/AuthKey_XXXX.p8",
        "key_id": "XXXXXXXXXX",
        "team_id": "XXXXXXXXXX",
        "bundle_id": "com.yourapp.locatedevice",
        "use_sandbox": true
    }
}
```

### Security Considerations
- Admin endpoints (`/api/locate`, `/api/devices`) require API key in `Authorization` header
- Device endpoints (`/api/register`, `/api/location`) require a device secret issued at registration
- Run behind a reverse proxy (nginx/caddy) for TLS termination in production
- Rate-limit locate requests to prevent abuse

---

## Component 2: Android Client

### Description
Minimal Android app. Registers with FCM, sends token to server. On receiving a high-priority data message, grabs GPS and POSTs it back.

### Tech Stack
- **Language**: Kotlin
- **Min SDK**: 26 (Android 8.0)
- **Dependencies**: Firebase Cloud Messaging, Google Play Services Location

### File Structure
```
android/
├── app/src/main/
│   ├── java/com/locate/device/
│   │   ├── App.kt                    # Application class — register with server on first launch
│   │   ├── RegistrationManager.kt    # Handles device registration with the C server
│   │   ├── LocateFirebaseService.kt  # Extends FirebaseMessagingService
│   │   │                              # onMessageReceived() → get location → POST back
│   │   └── LocationHelper.kt         # FusedLocationProviderClient wrapper
│   ├── AndroidManifest.xml
│   └── res/
├── build.gradle.kts
└── google-services.json              # Firebase config
```

### Key Implementation Details

**LocateFirebaseService.kt** — Core logic:
```
onMessageReceived(message):
    1. Extract request_id and server_url from message.data
    2. Request single GPS fix via FusedLocationProviderClient
       - Use PRIORITY_HIGH_ACCURACY
       - Timeout: 15 seconds
       - Fallback: last known location
    3. POST to {server_url}/api/location/{device_id}
       Body: { "request_id": "...", "lat": ..., "lng": ..., "accuracy": ... }
    4. Done — no local storage
```

**Critical Android notes:**
- Use a **data message** (not notification message) so `onMessageReceived()` fires even in background
- FCM high-priority data messages wake the app even after force-quit on most devices (except aggressive OEMs like Xiaomi/Huawei — document this limitation)
- Request `ACCESS_FINE_LOCATION` and `ACCESS_BACKGROUND_LOCATION` permissions
- Post a brief foreground notification while getting the fix (required on Android 10+ for background location)

### Permissions Required
```xml
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.ACCESS_BACKGROUND_LOCATION" />
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_LOCATION" />
```

---

## Component 3: iOS Client

### Description
Minimal iOS app. Registers with APNs, sends token to server. On receiving a silent push, grabs GPS and POSTs it back.

### Tech Stack
- **Language**: Swift
- **Min iOS**: 15.0
- **Frameworks**: CoreLocation, UserNotifications (UIKit or SwiftUI — minimal UI)

### File Structure
```
ios/
├── LocateDevice/
│   ├── AppDelegate.swift            # Register for remote notifications, handle push
│   ├── RegistrationManager.swift    # POST device token to C server
│   ├── LocationFetcher.swift        # CLLocationManager — single fix on demand
│   ├── PushHandler.swift            # Parse silent push, trigger locate, POST back
│   └── Info.plist
├── LocateDevice.xcodeproj
└── LocateDevice.entitlements        # Push notification entitlement
```

### Key Implementation Details

**AppDelegate.swift** — Push handling:
```
application(_:didReceiveRemoteNotification:fetchCompletionHandler:):
    1. Parse request_id and server_url from userInfo
    2. Start CLLocationManager with desiredAccuracy = kCLLocationAccuracyBest
    3. On first fix (or timeout after 10s):
       POST to {server_url}/api/location/{device_id}
       Body: { "request_id": "...", "lat": ..., "lng": ..., "accuracy": ... }
    4. Call completionHandler(.newData)
```

**Critical iOS notes:**
- Use `content-available: 1` in APNs payload for silent push
- You get ~30 seconds of background execution — enough for a GPS fix
- **If the user force-quits the app, silent pushes will NOT wake it** — this is an iOS limitation, document it clearly
- If the app hasn't been launched recently, iOS may throttle silent pushes
- Request `Always` location permission for best reliability, but `When In Use` works if the app was recently active
- Add `location` to `UIBackgroundModes` in Info.plist

### APNs Payload Format
```json
{
    "aps": {
        "content-available": 1
    },
    "request_id": "uuid-here",
    "server_url": "https://your-server.com"
}
```

### Required Capabilities
- Push Notifications
- Background Modes: Remote notifications, Location updates

---

## Implementation Order

### Phase 1: Server Core (2-3 days)
1. Set up project with Makefile, pull in cJSON as a submodule/vendored file
2. Implement `device_db.c` — SQLite schema: `CREATE TABLE devices (id TEXT PRIMARY KEY, token TEXT, platform TEXT, secret TEXT, created_at INTEGER)`
3. Implement `http_server.c` with libmicrohttpd — route dispatch
4. Implement `routes.c` — `/api/register` and `/api/devices` endpoints
5. Implement `config.c` — load JSON config
6. Test with curl

### Phase 2: Push Notification Dispatch (2-3 days)
1. Implement `push_fcm.c` — FCM HTTP v1 API with OAuth2 service account auth
2. Implement `jwt.c` — ES256 JWT signing for APNs
3. Implement `push_apns.c` — HTTP/2 POST via libcurl with JWT bearer token
4. Implement the pending request store (in-memory hashmap + condition variables)
5. Implement `/api/locate/{device_id}` — full flow: push → wait → return
6. Implement `/api/location/{device_id}` — device callback endpoint
7. Test push delivery with a test FCM/APNs token

### Phase 3: Android Client (2-3 days)
1. Create minimal Android project with Firebase
2. Implement `RegistrationManager` — POST token to server on launch
3. Implement `LocateFirebaseService` — handle data message, get GPS, POST back
4. Test end-to-end: curl locate → push → GPS → response

### Phase 4: iOS Client (2-3 days)
1. Create minimal Xcode project with push capability
2. Implement `AppDelegate` — register for remote notifications
3. Implement `PushHandler` + `LocationFetcher` — silent push → GPS → POST back
4. Test end-to-end

### Phase 5: Hardening (1-2 days)
1. Add API key auth to admin endpoints
2. Add device secret validation
3. Add request timeout handling and error responses
4. Add basic logging (stdout/stderr, syslog optional)
5. Write README with setup instructions

---

## Testing Checklist

- [ ] Register Android device, verify it appears in `/api/devices`
- [ ] Register iOS device, verify it appears in `/api/devices`
- [ ] Locate Android device — app in foreground
- [ ] Locate Android device — app in background
- [ ] Locate Android device — app force-killed (may fail on some OEMs)
- [ ] Locate iOS device — app in foreground
- [ ] Locate iOS device — app in background
- [ ] Locate iOS device — app force-killed (expected to fail)
- [ ] Locate with device offline — verify 30s timeout and error response
- [ ] Verify no location data persists on server after response
- [ ] Verify unauthorized requests are rejected
- [ ] Unregister device, verify it's removed

---

## Known Limitations

1. **iOS force-quit**: If the user swipes the app away, silent pushes won't wake it. No workaround exists within Apple's rules.
2. **Aggressive Android OEMs**: Xiaomi, Huawei, Samsung (some models) kill background services aggressively. Users may need to disable battery optimization for the app.
3. **No offline queuing**: If the device is off or has no internet, the locate request simply times out. No retry mechanism.
4. **GPS cold start**: First fix after a long time can take 15-30 seconds. The timeout on the server should accommodate this.
5. **Network dependency**: Both the push delivery and the location callback require internet connectivity on the device.
