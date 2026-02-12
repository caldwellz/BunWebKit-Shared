// Copyright (C) 2025 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import Foundation

typealias String = Swift.String
typealias URL = Foundation.URL

struct UncheckedSendableKeyPathBox<Root, Value>: @unchecked Sendable {
    let keyPath: KeyPath<Root, Value>
}

extension Comparable {
    /// Returns this comparable value clamped to the given limiting range.
    ///
    /// - Parameter limits: The range to clamp the bounds of this value.
    /// - Returns: A value guaranteed to be in the range `[limits.lowerBound, limits.upperBound]`
    func clamped(to limits: ClosedRange<Self>) -> Self {
        min(max(self, limits.lowerBound), limits.upperBound)
    }
}

extension CGPoint {
    /// Returns the euclidean distance between this point and `point`.
    ///
    /// - Parameter point: A point to compute the distance to.
    /// - Returns: The distance between this point and `point`.
    func distance(to point: CGPoint) -> Double {
        let distanceSquared = (x - point.x) * (x - point.x) + (y - point.y) * (y - point.y)
        return distanceSquared.squareRoot()
    }
}

extension CGRect {
    /// Determines the closest distance between this rect and `point` using whichever edge is nearest.
    ///
    /// - Parameter point: A point to compute the distance to.
    /// - Returns: The closest distance between this rect and `point`. If `point` is contained within this rect, `0` is returned.
    func distance(to point: CGPoint) -> Double {
        // Clamp the point coordinates to the rectangle's bounds
        let closestX = point.x.clamped(to: minX...maxX)
        let closestY = point.y.clamped(to: minY...maxY)

        let closestPoint = CGPoint(x: closestX, y: closestY)
        return point.distance(to: closestPoint)
    }
}
