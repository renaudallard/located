# located

A lightweight device location tracking system. Sends push notifications to registered mobile devices, which wake up, grab a GPS fix, and report their coordinates back. No location history is stored.

```
[CLI / curl]  -->  [C Server]  -->  [FCM / APNs push]
                       ^                    |
                       |                    v
                       +----  [Device POSTs GPS back]
```

## Quick Start

```sh
# install dependencies (Debian/Ubuntu)
sudo apt install libmicrohttpd-dev libsqlite3-dev libcurl4-openssl-dev libssl-dev

# build
make

# start the daemon
./located daemon -c config.example.json

# register a device
curl -X POST http://localhost:8080/api/register \
  -d '{"id":"phone1", "token":"fcm-tok", "platform":"android", "name":"My Phone"}'

# list devices (api_key required if set in config)
curl -H "Authorization: Bearer change-me" http://localhost:8080/api/devices

# locate a device (requires FCM/APNs config and a real device)
curl -X POST -H "Authorization: Bearer change-me" http://localhost:8080/api/locate/phone1

# unregister
curl -X DELETE http://localhost:8080/api/register/phone1
```

## Build & Install

```sh
make                             # build
sudo make install                # /usr/local/bin/located + man page
sudo make install PREFIX=/opt    # custom prefix
sudo make uninstall              # remove
```

Build requires: `gcc`, `libmicrohttpd-dev`, `libsqlite3-dev`, `libcurl4-openssl-dev`, `libssl-dev`.
Runtime: `curl` (used by CLI commands).

## Usage

```
located <command> [options]
```

| Command | Description |
|---|---|
| `daemon` / `-d` | Start the HTTP server (foreground, Ctrl-C to stop) |
| `devices` | List registered devices |
| `locate <id>` | Locate a device |
| `help` / `-h` | Show usage |
| `version` / `-v` | Show version |

Use `-c <path>` anywhere in argv to specify a config file.

## Configuration

```sh
cp config.example.json /etc/located.json
```

```json
{
    "listen_port": 8080,
    "db_path": "./devices.db",
    "api_key": "change-me",
    "server_url": "http://your-server:8080",
    "locate_timeout": 30,
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

All fields are optional; sensible defaults are used when omitted. FCM/APNs sections are only needed if you want to locate devices on that platform.

| Key | Default | Description |
|---|---|---|
| `listen_port` | `8080` | TCP port for the HTTP server |
| `db_path` | `./devices.db` | Path to the SQLite database file |
| `api_key` | *(none)* | API key for admin endpoints; if unset, they are open |
| `server_url` | `http://localhost:<port>` | Public URL sent in push payloads for device callbacks |
| `locate_timeout` | `30` | Seconds to wait for a device GPS response |

## API Reference

### `POST /api/register`

Register a new device.

**Request body:**
```json
{"id": "phone1", "token": "fcm-token", "platform": "android", "name": "My Phone"}
```

- `id`, `token`, `platform` required. `platform` must be `android` or `ios`.
- `name` is optional.

**Response** (`201`): device object with a generated `secret`. Store this secret on the device for authenticating location callbacks.

### `DELETE /api/register/<id>`

Unregister a device. Returns `200` on success, `404` if not found.

### `GET /api/devices`

List all registered devices as a JSON array. Requires `Authorization: Bearer <api_key>` if `api_key` is configured.

### `POST /api/locate/<id>`

Locate a device. Sends a push notification (FCM or APNs), then blocks waiting for the device to respond with GPS coordinates.

Requires `Authorization: Bearer <api_key>` if configured.

**Response** (`200`):
```json
{"device_id": "phone1", "latitude": 48.8566, "longitude": 2.3522, "accuracy": 10.5}
```

Returns `504` if the device does not respond within `locate_timeout` seconds.

### `POST /api/location/<id>`

Device callback endpoint. After receiving a push notification, the device POSTs its GPS coordinates here.

**Headers:** `X-Device-Secret: <secret>` (from registration).

**Request body:**
```json
{"request_id": "abc123", "lat": 48.8566, "lng": 2.3522, "accuracy": 10.5}
```

## Security

- **Admin endpoints** (`/api/devices`, `/api/locate`) require `Authorization: Bearer <api_key>` when `api_key` is set in config.
- **Device callbacks** (`/api/location`) require the `X-Device-Secret` header matching the device's secret from registration.
- Run behind a reverse proxy (nginx, caddy) for TLS in production.

## Mobile Clients

### Android

Kotlin app in `android/`. Uses Firebase Cloud Messaging and Google Play Services Location.

Setup:
1. Create a Firebase project and download `google-services.json` to `android/app/`.
2. Add the Firebase service account JSON key path to the server config.
3. Build with Android Studio or `./gradlew assembleDebug`.
4. On launch, enter the server URL and device ID, then tap Register.

**Permissions required:** fine location, background location, foreground service, notifications.

### iOS

Swift app in `ios/LocateDevice/`. Uses APNs and CoreLocation.

Setup:
1. Create an Xcode project and add the Swift source files from `ios/LocateDevice/`.
2. Enable Push Notifications and Background Modes (Remote notifications, Location updates) capabilities.
3. Generate an APNs authentication key in the Apple Developer portal.
4. Add the .p8 key path, key ID, team ID, and bundle ID to the server config.
5. On launch, enter the server URL and device ID, then tap Register.

**Permissions required:** location (always), push notifications.

## Locate Flow

1. Admin calls `POST /api/locate/<id>`.
2. Server looks up the device in SQLite (token, platform).
3. Server sends a push notification with a unique `request_id`.
4. Server blocks waiting on a condition variable (up to `locate_timeout` seconds).
5. Device wakes up, gets a GPS fix, POSTs to `/api/location/<id>`.
6. Server matches the `request_id`, signals the waiting thread.
7. `/api/locate` returns the coordinates. **No location data is stored.**

## Project Structure

```
src/
  main.c           CLI dispatch, daemon startup
  config.c/.h      JSON config loading
  device_db.c/.h   SQLite device registry CRUD
  http_server.c/.h libmicrohttpd wrapper + route dispatch
  routes.c/.h      HTTP endpoint handlers
  jwt.c/.h         JWT creation (RS256 for FCM OAuth2, ES256 for APNs)
  push_fcm.c/.h    FCM HTTP v1 push via libcurl
  push_apns.c/.h   APNs HTTP/2 push via libcurl
  pending.c/.h     In-memory pending request store (condvar-based)
  auth.c/.h        API key + device secret validation
vendor/
  cJSON.c/.h       vendored JSON parser (MIT)
android/           Android client (Kotlin)
ios/               iOS client (Swift)
```

## Known Limitations

- **iOS force-quit**: silent pushes will not wake a force-killed app.
- **Aggressive Android OEMs**: Xiaomi, Huawei, some Samsung models kill background services. Users may need to disable battery optimization.
- **No offline queuing**: if the device is off, the request times out.
- **GPS cold start**: first fix can take 15-30 seconds after a long idle.

## Man Page

```sh
man located           # after install
man ./located.1       # from source tree
```

## License

cJSON (vendored) is MIT licensed. See `vendor/cJSON.c` for details.
