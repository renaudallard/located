import Foundation

/// Handles device registration with the located server.
/// Stores device secret in UserDefaults for authenticating location callbacks.
class RegistrationManager {

    static let shared = RegistrationManager()

    private let defaults = UserDefaults.standard

    private enum Keys {
        static let deviceId = "allardtrac_device_id"
        static let secret = "allardtrac_secret"
        static let serverUrl = "allardtrac_server_url"
        static let apnsToken = "allardtrac_apns_token"
    }

    var deviceId: String? {
        get { defaults.string(forKey: Keys.deviceId) }
        set { defaults.set(newValue, forKey: Keys.deviceId) }
    }

    var secret: String? {
        get { defaults.string(forKey: Keys.secret) }
        set { defaults.set(newValue, forKey: Keys.secret) }
    }

    var serverUrl: String? {
        get { defaults.string(forKey: Keys.serverUrl) }
        set { defaults.set(newValue, forKey: Keys.serverUrl) }
    }

    var apnsToken: String? {
        get { defaults.string(forKey: Keys.apnsToken) }
        set { defaults.set(newValue, forKey: Keys.apnsToken) }
    }

    var isRegistered: Bool {
        return deviceId != nil && secret != nil
    }

    /// Register with the server. Calls completion on main thread.
    func register(
        serverUrl: String,
        deviceId: String,
        name: String?,
        completion: @escaping (Result<String, Error>) -> Void
    ) {
        guard let token = apnsToken else {
            completion(.failure(LocateError.noToken))
            return
        }

        let url = URL(string: "\(serverUrl)/api/register")!
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        var body: [String: Any] = [
            "id": deviceId,
            "token": token,
            "platform": "ios"
        ]
        if let name = name, !name.isEmpty {
            body["name"] = name
        }

        request.httpBody = try? JSONSerialization.data(withJSONObject: body)

        URLSession.shared.dataTask(with: request) { data, response, error in
            DispatchQueue.main.async {
                if let error = error {
                    completion(.failure(error))
                    return
                }

                guard let http = response as? HTTPURLResponse,
                      http.statusCode == 201,
                      let data = data,
                      let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                      let secret = json["secret"] as? String
                else {
                    completion(.failure(LocateError.registrationFailed))
                    return
                }

                self.deviceId = deviceId
                self.secret = secret
                self.serverUrl = serverUrl

                completion(.success(secret))
            }
        }.resume()
    }

    func clear() {
        defaults.removeObject(forKey: Keys.deviceId)
        defaults.removeObject(forKey: Keys.secret)
        defaults.removeObject(forKey: Keys.serverUrl)
    }
}

enum LocateError: LocalizedError {
    case noToken
    case registrationFailed
    case locationUnavailable

    var errorDescription: String? {
        switch self {
        case .noToken: return "No APNs token available"
        case .registrationFailed: return "Server registration failed"
        case .locationUnavailable: return "Could not get location"
        }
    }
}
