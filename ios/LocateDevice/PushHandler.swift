import CoreLocation
import Foundation

/// Handles incoming silent push notifications.
/// Extracts request_id, gets a GPS fix, and POSTs location back to the server.
class PushHandler {

    // Keep a strong reference so it lives long enough for the callback
    private static var activeFetcher: LocationFetcher?

    static func handle(
        userInfo: [AnyHashable: Any],
        completion: @escaping (UIBackgroundFetchResult) -> Void
    ) {
        guard let requestId = userInfo["request_id"] as? String,
              let serverUrl = userInfo["server_url"] as? String,
              let deviceId = userInfo["device_id"] as? String
        else {
            print("PushHandler: missing fields in push payload")
            completion(.noData)
            return
        }

        guard let secret = RegistrationManager.shared.secret else {
            print("PushHandler: no device secret stored")
            completion(.failed)
            return
        }

        print("PushHandler: locate request \(requestId) for \(deviceId)")

        let fetcher = LocationFetcher()
        activeFetcher = fetcher

        fetcher.fetch { location in
            activeFetcher = nil

            guard let location = location else {
                print("PushHandler: could not get location")
                completion(.failed)
                return
            }

            postLocation(
                serverUrl: serverUrl,
                deviceId: deviceId,
                requestId: requestId,
                secret: secret,
                location: location,
                completion: completion
            )
        }
    }

    private static func postLocation(
        serverUrl: String,
        deviceId: String,
        requestId: String,
        secret: String,
        location: CLLocation,
        completion: @escaping (UIBackgroundFetchResult) -> Void
    ) {
        let urlString = "\(serverUrl)/api/location/\(deviceId)"
        guard let url = URL(string: urlString) else {
            completion(.failed)
            return
        }

        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue(secret, forHTTPHeaderField: "X-Device-Secret")

        let body: [String: Any] = [
            "request_id": requestId,
            "lat": location.coordinate.latitude,
            "lng": location.coordinate.longitude,
            "accuracy": location.horizontalAccuracy
        ]

        request.httpBody = try? JSONSerialization.data(withJSONObject: body)

        URLSession.shared.dataTask(with: request) { _, response, error in
            if let error = error {
                print("PushHandler: post error: \(error)")
                completion(.failed)
                return
            }

            if let http = response as? HTTPURLResponse, http.statusCode == 200 {
                print("PushHandler: location posted successfully")
                completion(.newData)
            } else {
                print("PushHandler: post failed")
                completion(.failed)
            }
        }.resume()
    }
}
