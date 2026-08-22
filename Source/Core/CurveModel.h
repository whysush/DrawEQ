#pragma once

#include "CurveSnapshot.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace graphite
{

/**
    The drawn curve. Owned by the message thread; read by the worker.

    Storage is dB on the fixed log-frequency grid - never pixels, so the same
    stroke means the same thing at any window size, sample rate or zoom.

    Thread safety: a plain mutex guards the grid. That is legal here precisely
    because the two parties are the message thread and the worker thread. The
    audio thread never touches this class - it only ever sees a FilterState the
    worker published (CONTEXT.md 5).
*/
class CurveModel
{
public:
    enum class Brush
    {
        draw,     // pull toward the stroke's dB
        smooth,   // pull toward the local neighbourhood mean
        erase     // pull toward 0 dB
    };

    CurveModel();

    /** Flat 0 dB. */
    void reset();

    // --- editing -----------------------------------------------------------
    //
    // A gesture is one undo entry. Mouse-down calls beginGesture, every drag
    // point calls strokeTo, mouse-up calls endGesture - so a whole stroke
    // collapses into a single Ctrl+Z (CONTEXT.md 9.6).

    void beginGesture();
    void endGesture();

    /** Anchors the stroke without painting, so the first segment has a start. */
    void startStroke (float hz, float db);

    /** Paints from the previous point to this one, interpolating along the log
        axis so a fast drag does not leave gaps. */
    void strokeTo (float hz, float db, float radiusOctaves, float pressure, Brush brush);

    /** Straight segment in log/dB space - the Line tool. */
    void drawLine (float hz0, float db0, float hz1, float db1, float radiusOctaves);

    /** Double-click behaviour: flatten a window centred on hz. */
    void flattenWindow (float hz, float octaves);

    void setCurve (const CurveArray& c);
    CurveArray getCurve() const;

    /** Linear-in-dB interpolation across the grid; holds its end values outside
        [kFMin, kFMax]. */
    float dbAtHz (float hz) const;

    // --- cross-thread ------------------------------------------------------

    /** Fills the raw curve and version. Macro fields are the caller's job. */
    void fillSnapshot (CurveSnapshot& s) const;

    /** Monotonic edit counter. The worker compares it against the version it
        last built from, and goes back to sleep if nothing changed. */
    std::uint64_t version() const noexcept { return ver.load (std::memory_order_acquire); }

    // --- undo --------------------------------------------------------------

    static constexpr int kUndoDepth = 64;

    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;

    // --- serialisation -----------------------------------------------------
    //
    // Versioned binary, base64'd into the plugin state by the processor. The
    // version int is what lets a future grid size migrate instead of loading
    // as garbage (CONTEXT.md 8.1).

    static constexpr std::uint16_t kCurveVersion = 1;

    std::vector<std::uint8_t> serialise() const;
    bool deserialise (const std::uint8_t* data, std::size_t size);

private:
    void touch();                                     // bump version, caller holds lock
    void pushUndoLocked();
    void applyPointLocked (float index, float targetDb, float radiusBins,
                           float amount, Brush brush, const CurveArray& source);
    void paintSegmentLocked (float idx0, float db0, float idx1, float db1,
                             float radiusBins, float pressure, Brush brush);

    mutable std::mutex lock;
    CurveArray grid {};
    std::atomic<std::uint64_t> ver { 1 };

    // Stroke state, message thread only.
    float lastIndex = 0.0f;
    float lastDb    = 0.0f;
    bool  strokeOpen = false;

    // The span this gesture has already swept. A segment's feather must not
    // reach back into it: those bins were placed correctly by an earlier
    // segment, and feathering them again is what makes a stroke lag the cursor.
    float sweptLo = 0.0f, sweptHi = 0.0f;
    float sweptLoDb = 0.0f, sweptHiDb = 0.0f;   // the stroke's value at each extreme
    bool  hasSwept = false;

    std::vector<CurveArray> undoStack, redoStack;
    bool gestureOpen = false;
};

} // namespace graphite
