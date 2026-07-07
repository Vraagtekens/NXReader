//
//  E_readerApp.swift
//  E-reader
//
//  Created by Dylan Jansen on 06/07/2026.
//

import SwiftUI
import UIKit

@main
struct E_readerApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}

final class AppDelegate: NSObject, UIApplicationDelegate {
    func application(
        _ application: UIApplication,
        supportedInterfaceOrientationsFor window: UIWindow?
    ) -> UIInterfaceOrientationMask {
        OrientationController.shared.supportedOrientations
    }
}

final class OrientationController {
    static let shared = OrientationController()

    private(set) var supportedOrientations: UIInterfaceOrientationMask = .allButUpsideDown

    func setRotationLocked(_ isLocked: Bool) {
        supportedOrientations = isLocked ? .portrait : .allButUpsideDown

        guard let scene = UIApplication.shared.connectedScenes.first as? UIWindowScene else {
            return
        }

        if #available(iOS 16.0, *) {
            scene.requestGeometryUpdate(
                .iOS(interfaceOrientations: supportedOrientations)
            )
            scene.windows.first?.rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
        } else {
            UIViewController.attemptRotationToDeviceOrientation()
        }
    }
}
