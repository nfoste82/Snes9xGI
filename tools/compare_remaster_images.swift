// Run: swift tools/compare_remaster_images.swift BEFORE_OUTPUT AFTER_OUTPUT
// Decode all seven benchmark PNGs; report differences in 8-bit output channels.
import AppKit

guard CommandLine.arguments.count == 3 else {
    fatalError("Usage: compare_remaster_images.swift BEFORE_OUTPUT AFTER_OUTPUT")
}
let dirs = CommandLine.arguments.dropFirst()
for c in 1...7 {
    let images = try dirs.map { directory -> NSBitmapImageRep in
        let url = URL(fileURLWithPath: "\(directory)/case-\(String(format: "%02d", c)).png")
        guard let image = NSBitmapImageRep(data: try Data(contentsOf: url)) else {
            fatalError("Could not decode \(url.path)")
        }
        return image
    }
    let a = images[0], b = images[1]
    precondition(a.pixelsWide == b.pixelsWide && a.pixelsHigh == b.pixelsHigh)
    var channels = 0, maxDelta = 0, total = 0
    for y in 0..<a.pixelsHigh {
        for x in 0..<a.pixelsWide {
            var p = [Int](repeating: 0, count: 4), q = p
            a.getPixel(&p, atX: x, y: y)
            b.getPixel(&q, atX: x, y: y)
            for k in 0..<4 {
                let delta = abs(p[k] - q[k])
                channels += delta > 0 ? 1 : 0
                maxDelta = max(maxDelta, delta)
                total += delta
            }
        }
    }
    print("case \(c): differing channels=\(channels), max byte difference=\(maxDelta), total absolute difference=\(total)")
}
