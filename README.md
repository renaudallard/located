# allardtrac

A lightweight device location tracking server written in C. Registers mobile devices via a JSON API and stores them in SQLite.

Follows the UNIX philosophy: the daemon serves the API, the CLI talks to it with `curl`.

## Quick Start

```sh
# install dependencies (Debian/Ubuntu)
sudo apt install libmicrohttpd-dev libsqlite3-dev

# build
make

# start the daemon
./located daemon -c config.example.json

# register a device (in another terminal)
curl -X POST http://localhost:8080/api/register \
  -d '{"id":"phone1", "token":"fcm-tok", "platform":"android", "name":"My Phone"}'

# list devices
curl http://localhost:8080/api/devices

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

Requires: `gcc`, `libmicrohttpd-dev`, `libsqlite3-dev`. Runtime: `curl`.

## Usage

```
located <command> [options]
```

| Command | Description |
|---|---|
| `daemon` / `-d` | Start the HTTP server (foreground, Ctrl-C to stop) |
| `devices` | List registered devices |
| `locate <id>` | Locate a device *(Phase 2)* |
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
    "api_key": "change-me"
}
```

All fields are optional; sensible defaults are used when omitted.

| Key | Default | Description |
|---|---|---|
| `listen_port` | `8080` | TCP port for the HTTP server |
| `db_path` | `./devices.db` | Path to the SQLite database file |
| `api_key` | *(none)* | API key *(reserved for Phase 2)* |

## API Reference

### `POST /api/register`

Register a new device.

**Request body:**
```json
{"id": "phone1", "token": "fcm-token", "platform": "android", "name": "My Phone"}
```

- `id`, `token`, `platform` are required. `platform` must be `android` or `ios`.
- `name` is optional.

**Response** (`201`): device object with a generated `secret`.

### `DELETE /api/register/<id>`

Unregister a device. Returns `200` on success, `404` if not found.

### `GET /api/devices`

List all registered devices as a JSON array.

## Man Page

```sh
man located           # after install
man ./located.1       # from source tree
```

## Project Structure

```
src/
  main.c           CLI dispatch, daemon startup
  config.c/.h      JSON config loading
  device_db.c/.h   SQLite device registry CRUD
  http_server.c/.h libmicrohttpd wrapper + route dispatch
  routes.c/.h      HTTP endpoint handlers
vendor/
  cJSON.c/.h       vendored JSON parser (MIT)
```

## License

cJSON (vendored) is MIT licensed. See `vendor/cJSON.c` for details.
