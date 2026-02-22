import UIKit
import CoreImage

extension UIImage {
    /// Extracts the dominant/average color from the image using Core Image.
    /// Returns nil if extraction fails.
    func dominantColor() -> UIColor? {
        guard let inputImage = CIImage(image: self) else { return nil }

        let extentVector = CIVector(x: inputImage.extent.origin.x,
                                    y: inputImage.extent.origin.y,
                                    z: inputImage.extent.size.width,
                                    w: inputImage.extent.size.height)

        guard let filter = CIFilter(name: "CIAreaAverage",
                                    parameters: [kCIInputImageKey: inputImage,
                                               kCIInputExtentKey: extentVector]) else { return nil }
        guard let outputImage = filter.outputImage else { return nil }

        var bitmap = [UInt8](repeating: 0, count: 4)
        let context = CIContext(options: [.workingColorSpace: kCFNull as Any])
        context.render(outputImage,
                      toBitmap: &bitmap,
                      rowBytes: 4,
                      bounds: CGRect(x: 0, y: 0, width: 1, height: 1),
                      format: .RGBA8,
                      colorSpace: nil)

        return UIColor(red: CGFloat(bitmap[0]) / 255,
                      green: CGFloat(bitmap[1]) / 255,
                      blue: CGFloat(bitmap[2]) / 255,
                      alpha: CGFloat(bitmap[3]) / 255)
    }

    /// Returns a darkened version of the dominant color suitable for backgrounds.
    /// Darkens by reducing brightness to 30% of original.
    func dominantBackgroundColor() -> UIColor? {
        guard let color = dominantColor() else { return nil }

        var hue: CGFloat = 0
        var saturation: CGFloat = 0
        var brightness: CGFloat = 0
        var alpha: CGFloat = 0

        color.getHue(&hue, saturation: &saturation, brightness: &brightness, alpha: &alpha)

        // Darken the color for background use (30% brightness)
        return UIColor(hue: hue, saturation: saturation, brightness: brightness * 0.3, alpha: 1.0)
    }
}
