import CoreLocation
import Foundation

/// Gets a single high-accuracy GPS fix on demand.
class LocationFetcher: NSObject, CLLocationManagerDelegate {

    private let manager = CLLocationManager()
    private var completion: ((CLLocation?) -> Void)?
    private var timer: Timer?

    private static let timeoutInterval: TimeInterval = 10.0

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBest
    }

    /// Request a single location fix. Calls completion exactly once.
    func fetch(completion: @escaping (CLLocation?) -> Void) {
        self.completion = completion

        if CLLocationManager.authorizationStatus() == .notDetermined {
            manager.requestAlwaysAuthorization()
        }

        manager.requestLocation()

        // Timeout fallback
        timer = Timer.scheduledTimer(withTimeInterval: Self.timeoutInterval,
                                     repeats: false) { [weak self] _ in
            self?.finish(location: nil)
        }
    }

    func locationManager(_ manager: CLLocationManager,
                         didUpdateLocations locations: [CLLocation]) {
        finish(location: locations.last)
    }

    func locationManager(_ manager: CLLocationManager,
                         didFailWithError error: Error) {
        print("Location error: \(error)")
        finish(location: nil)
    }

    private func finish(location: CLLocation?) {
        timer?.invalidate()
        timer = nil
        let cb = completion
        completion = nil
        cb?(location)
    }
}
