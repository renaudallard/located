import UIKit

/// Simple registration UI — server URL, device ID, name, register button.
class RegistrationViewController: UIViewController {

    private let serverUrlField = UITextField()
    private let deviceIdField = UITextField()
    private let deviceNameField = UITextField()
    private let registerButton = UIButton(type: .system)
    private let statusLabel = UILabel()

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "allardtrac"
        view.backgroundColor = .systemBackground
        setupUI()
        updateStatus()
    }

    private func setupUI() {
        let stack = UIStackView()
        stack.axis = .vertical
        stack.spacing = 16
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)

        NSLayoutConstraint.activate([
            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor,
                                      constant: 24),
            stack.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -24)
        ])

        for (field, placeholder) in [
            (serverUrlField, "Server URL (e.g. http://192.168.1.100:8080)"),
            (deviceIdField, "Device ID"),
            (deviceNameField, "Device name (optional)")
        ] {
            field.placeholder = placeholder
            field.borderStyle = .roundedRect
            field.autocapitalizationType = .none
            field.autocorrectionType = .no
            stack.addArrangedSubview(field)
        }

        serverUrlField.keyboardType = .URL

        registerButton.setTitle("Register", for: .normal)
        registerButton.titleLabel?.font = .boldSystemFont(ofSize: 18)
        registerButton.addTarget(self, action: #selector(registerTapped),
                                 for: .touchUpInside)
        stack.addArrangedSubview(registerButton)

        statusLabel.numberOfLines = 0
        statusLabel.textColor = .secondaryLabel
        stack.addArrangedSubview(statusLabel)

        // Restore saved values
        let reg = RegistrationManager.shared
        if let url = reg.serverUrl { serverUrlField.text = url }
        if let id = reg.deviceId { deviceIdField.text = id }
    }

    private func updateStatus() {
        let reg = RegistrationManager.shared
        if reg.isRegistered {
            statusLabel.text = "Registered as: \(reg.deviceId ?? "")"
            registerButton.setTitle("Re-register", for: .normal)
        } else {
            statusLabel.text = "Not registered"
        }
    }

    @objc private func registerTapped() {
        guard let serverUrl = serverUrlField.text?.trimmingCharacters(in: .whitespaces),
              !serverUrl.isEmpty,
              let deviceId = deviceIdField.text?.trimmingCharacters(in: .whitespaces),
              !deviceId.isEmpty
        else {
            statusLabel.text = "Server URL and Device ID required"
            return
        }

        let name = deviceNameField.text?.trimmingCharacters(in: .whitespaces)
        registerButton.isEnabled = false
        statusLabel.text = "Registering..."

        RegistrationManager.shared.register(
            serverUrl: serverUrl.trimmingCharacters(in: CharacterSet(charactersIn: "/")),
            deviceId: deviceId,
            name: name
        ) { [weak self] result in
            self?.registerButton.isEnabled = true
            switch result {
            case .success:
                self?.updateStatus()
            case .failure(let error):
                self?.statusLabel.text = "Failed: \(error.localizedDescription)"
            }
        }
    }
}
