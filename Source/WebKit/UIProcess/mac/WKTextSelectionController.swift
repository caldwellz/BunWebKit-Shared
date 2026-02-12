// Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)

import Foundation
internal import WebKit_Internal
import AppKit
internal import WebCore_Private
private import CxxStdlib

@objc
@implementation
extension WKTextSelectionController {
    private weak let view: WKWebView?

    init(view: WKWebView) {
        self.view = view
        super.init()
    }

    func addTextSelectionManager() {
        guard let view, let page = view._protectedPage().get() else {
            return
        }

        guard page.preferences().useAppKitGestures() else {
            return
        }

        Logger.viewGestures.log("Creating a text selection manager for view \(view)")

        let manager = NSTextSelectionManager()
        manager._webkitDelegate = self
        view.textSelectionManager = manager

        for case let gestureRecognizer as NSPressGestureRecognizer in manager.gesturesForFailureRequirements {
            gestureRecognizer.buttonMask = 0
        }
    }
}

@objc(NSTextSelectionManagerDelegate)
@implementation
extension WKTextSelectionController {
    // Chosen to match the rest of the system.
    private static let nearCaretDistance: Double = 40

    @objc(isTextSelectedAtPoint:)
    func isTextSelected(at point: NSPoint) -> Bool {
        // The `point` location is relative to the view.

        // FIXME: Address warning "Cannot infer ownership of foreign reference value returned by 'get()'"
        guard let page = view?._protectedPage().get() else {
            return false
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Checking if text is selected at point \(point.debugDescription)...")

        let editorState = unsafe page.editorState
        let hasSelection = unsafe !editorState.selectionIsNone

        if unsafe !hasSelection || !editorState.hasPostLayoutAndVisualData() {
            Logger.viewGestures.log(
                "[pageProxyID=\(page.logIdentifier())] Editor state has no selection, post layout data, or visual data"
            )
            return false
        }

        let isRange = unsafe editorState.selectionIsRange
        let isContentEditable = unsafe editorState.isContentEditable

        if !isContentEditable && !isRange {
            Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Selection is neither contenteditable nor a range")
            return false
        }

        // FIXME: If the state's selection is not a range, is the number of selection geometries always zero?
        // If so, then the rest of the logic in this function can be elided in that case.

        var selectionRects: [WKTextSelectionRect] = []
        let selectionGeometries = unsafe editorState.visualData.pointee.selectionGeometries

        // FIXME: `WTF::Vector` should be able to be used as a Swift `Sequence`.
        for i in unsafe 0..<selectionGeometries.size() {
            let selectionGeometry = unsafe selectionGeometries.__atUnsafe(i).pointee
            selectionRects.append(.init(selectionGeometry: selectionGeometry, delegate: nil))
        }

        let result = selectionRects.contains { $0.rect.contains(point) }
        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Text is selected => \(result)")

        return result
    }

    @objc(moveInsertionCursorToPoint:)
    func moveInsertionCursor(to point: NSPoint) {
        guard let page = view?._protectedPage().get() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Moving insertion cursor to point \(point.debugDescription)...")
    }

    @objc(handleClickAtPoint:)
    func handleClick(at point: NSPoint) {
        Task.immediate {
            await handleClickInternal(at: point)
        }
    }

    @MainActor
    private func handleClickInternal(at point: NSPoint) async {
        // The `point` location is relative to the view.

        guard let view, let page = view._protectedPage().get() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Handling click at point \(point.debugDescription)...")

        defer {
            Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Done handling click.")
        }

        let previousState = unsafe page.editorState
        let previousVisualData = unsafe Optional(fromCxx: previousState.visualData)

        // Move the insertion point to the nearest word granularity boundary.

        await page.selectWithGesture(
            at: WebCore.IntPoint(point),
            type: .OneFingerTap,
            state: .Ended,
            isInteractingWithFocusedElement: true, // FIXME: Properly handle the case where this isn't actually true.
        )

        // If the click was near where the caret selection was, or the selection did not change, show context menu.

        let newState = unsafe page.editorState
        let newVisualData = unsafe Optional(fromCxx: newState.visualData)

        guard let previousVisualData = unsafe previousVisualData, let newVisualData = unsafe newVisualData else {
            return
        }

        // FIXME: Reduce duplication of this logic.

        let distance = unsafe CGRect(previousVisualData.caretRectAtStart).distance(to: point)
        let clickLocationIsNearCaret = unsafe !previousState.selectionIsRange && distance < Self.nearCaretDistance
        let caretLocationIsSame = unsafe previousVisualData.caretRectAtStart == newVisualData.caretRectAtStart

        Logger.viewGestures.log(
            "[pageProxyID=\(page.logIdentifier())] Click near insertion point: \(clickLocationIsNearCaret); Caret location is same: \(caretLocationIsSame)"
        )

        if clickLocationIsNearCaret || caretLocationIsSame {
            let pointInGlobalCoordinateSpace = view.convert(point, to: nil)
            showContextMenu(at: pointInGlobalCoordinateSpace)
        }
    }

    @objc(showContextMenuAtPoint:)
    func showContextMenu(at point: NSPoint) {
        // The `point` location is relative to the window.

        guard let page = view?._protectedPage().get(), let impl = unsafe view?._impl() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Showing context menu at point \(point.debugDescription)...")

        let timestamp = GetCurrentEventTime()
        let windowNumber = unsafe impl.windowNumber()

        let mouseDown = NSEvent.mouseEvent(
            with: .rightMouseDown,
            location: point,
            modifierFlags: [],
            timestamp: timestamp,
            windowNumber: windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: 1,
            pressure: 1
        )
        let mouseUp = NSEvent.mouseEvent(
            with: .rightMouseUp,
            location: point,
            modifierFlags: [],
            timestamp: timestamp,
            windowNumber: windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: 1,
            pressure: 0
        )

        unsafe impl.mouseDown(mouseDown, .Automation)
        unsafe impl.mouseUp(mouseUp, .Automation)
    }

    @objc(dragSelectionWithGesture:completionHandler:)
    func dragSelection(withGesture gesture: NSGestureRecognizer, completionHandler: @escaping @Sendable (NSDraggingSession) -> Void) {
    }

    @objc(beginRangeSelectionAtPoint:withGranularity:)
    func beginRangeSelection(at point: NSPoint, with granularity: NSTextSelection.Granularity) {
    }

    @objc(continueRangeSelectionAtPoint:)
    func continueRangeSelection(at point: NSPoint) {
    }

    @objc(endRangeSelectionAtPoint:)
    func endRangeSelection(at point: NSPoint) {
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)
