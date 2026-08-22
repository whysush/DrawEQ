#include "CurveModel.h"
#include "CurveShaping.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace graphite
{

CurveModel::CurveModel()
{
    undoStack.reserve (kUndoDepth);
    redoStack.reserve (kUndoDepth);
    grid.fill (0.0f);
}

void CurveModel::reset()
{
    const std::lock_guard<std::mutex> g (lock);
    pushUndoLocked();
    grid.fill (0.0f);
    touch();
}

void CurveModel::touch()
{
    ver.fetch_add (1, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

void CurveModel::beginGesture()
{
    const std::lock_guard<std::mutex> g (lock);

    if (! gestureOpen)
    {
        pushUndoLocked();
        gestureOpen = true;
        gestureActive.store (true, std::memory_order_release);
    }
}

void CurveModel::endGesture()
{
    const std::lock_guard<std::mutex> g (lock);
    const bool wasOpen = gestureOpen;

    gestureOpen = false;
    strokeOpen  = false;
    hasSwept    = false;
    gestureActive.store (false, std::memory_order_release);

    if (wasOpen)
        gesturesCompleted.fetch_add (1, std::memory_order_release);
}

void CurveModel::startStroke (float hz, float db)
{
    const std::lock_guard<std::mutex> g (lock);
    lastIndex  = LogGrid::clampIndex (LogGrid::hzToIndex (hz));
    lastDb     = LogGrid::clampDb (db);
    strokeOpen = true;
    hasSwept   = false;
}

void CurveModel::strokeTo (float hz, float db, float radiusOctaves, float pressure, Brush brush)
{
    const std::lock_guard<std::mutex> g (lock);

    const float idx = LogGrid::clampIndex (LogGrid::hzToIndex (hz));
    const float tgt = LogGrid::clampDb (db);

    if (! strokeOpen)
    {
        lastIndex  = idx;
        lastDb     = tgt;
        strokeOpen = true;
        hasSwept   = false;
    }

    const float radiusBins = std::max (1.0f, LogGrid::octavesToBins (radiusOctaves));
    paintSegmentLocked (lastIndex, lastDb, idx, tgt, radiusBins,
                        std::clamp (pressure, 0.0f, 1.0f), brush);

    lastIndex = idx;
    lastDb    = tgt;
    touch();
}

void CurveModel::drawLine (float hz0, float db0, float hz1, float db1, float radiusOctaves)
{
    const std::lock_guard<std::mutex> g (lock);

    const float i0 = LogGrid::clampIndex (LogGrid::hzToIndex (hz0));
    const float i1 = LogGrid::clampIndex (LogGrid::hzToIndex (hz1));
    const float radiusBins = std::max (1.0f, LogGrid::octavesToBins (radiusOctaves));

    paintSegmentLocked (i0, LogGrid::clampDb (db0), i1, LogGrid::clampDb (db1),
                        radiusBins, 1.0f, Brush::draw);
    touch();
}

void CurveModel::flattenWindow (float hz, float octaves)
{
    const std::lock_guard<std::mutex> g (lock);

    pushUndoLocked();

    const float centre = LogGrid::clampIndex (LogGrid::hzToIndex (hz));
    const float radius = std::max (1.0f, LogGrid::octavesToBins (octaves * 0.5f));

    const CurveArray source = grid;
    applyPointLocked (centre, 0.0f, radius, 1.0f, Brush::erase, source);
    touch();
}

/** One brush dab. `source` is the state the dab reads from, which for the
    smooth brush must be a copy taken before the dab started - otherwise each
    bin's new value feeds the next bin's mean and the blur runs away in the
    direction of travel. */
void CurveModel::applyPointLocked (float index, float targetDb, float radiusBins,
                                   float amount, Brush brush, const CurveArray& source)
{
    const int lo = std::max (0, int (std::floor (index - radiusBins)));
    const int hi = std::min (LogGrid::kSize - 1, int (std::ceil (index + radiusBins)));

    for (int i = lo; i <= hi; ++i)
    {
        const float d = std::abs (float (i) - index) / radiusBins;

        if (d >= 1.0f)
            continue;

        // Raised cosine: 1 at the centre, 0 at the rim, zero slope at both ends
        // so overlapping dabs do not leave visible ridges.
        const float w = 0.5f * (1.0f + std::cos (3.14159265358979f * d)) * amount;

        float target = targetDb;

        if (brush == Brush::erase)
        {
            target = 0.0f;
        }
        else if (brush == Brush::smooth)
        {
            const int   n0 = std::max (0, i - int (radiusBins));
            const int   n1 = std::min (LogGrid::kSize - 1, i + int (radiusBins));
            float sum = 0.0f;

            for (int n = n0; n <= n1; ++n)
                sum += source[size_t (n)];

            target = sum / float (n1 - n0 + 1);
        }

        grid[size_t (i)] = LogGrid::clampDb (grid[size_t (i)] + w * (target - grid[size_t (i)]));
    }
}

void CurveModel::paintSegmentLocked (float idx0, float db0, float idx1, float db1,
                                     float radiusBins, float pressure, Brush brush)
{
    const CurveArray source = grid;

    const float lo = std::min (idx0, idx1);
    const float hi = std::max (idx0, idx1);

    // The stroke's dB at each end of this segment, in index order.
    const float dbAtLo = idx0 <= idx1 ? db0 : db1;
    const float dbAtHi = idx0 <= idx1 ? db1 : db0;

    // Is this segment breaking new ground, or moving back over ground the
    // gesture has already covered? Each end is judged separately, because a
    // stroke that reverses does both at once.
    const bool extendsLow  = ! hasSwept || lo <= sweptLo;
    const bool extendsHigh = ! hasSwept || hi >= sweptHi;

    const int first = std::max (0, int (std::floor (lo - radiusBins)));
    const int last  = std::min (LogGrid::kSize - 1, int (std::ceil (hi + radiusBins)));

    const float span = idx1 - idx0;

    // One pass over the affected bins, projecting each onto the segment, rather
    // than a run of overlapping dabs.
    //
    // The dab version lagged the cursor. Each dab pulled its neighbours toward
    // its own target, so a bin painted early was dragged most of the way to
    // whatever the stroke did next, and a steep gesture came out flattened and
    // trailing. Here a bin the stroke passes directly over is written to the
    // stroke's value at that exact frequency, once.
    for (int i = first; i <= last; ++i)
    {
        const float pos = float (i);

        // Projection onto the segment. In index space the segment is an
        // interval, so this is a clamp, and `t` says where along the stroke the
        // bin sits.
        const float nearest = std::clamp (pos, lo, hi);
        const float t = std::abs (span) > 1.0e-6f
                      ? std::clamp ((nearest - idx0) / span, 0.0f, 1.0f)
                      : 0.0f;

        const float d = std::abs (pos - nearest) / radiusBins;

        if (d >= 1.0f)
            continue;

        float target = db0 + t * (db1 - db0);

        if (d > 0.0f && hasSwept)
        {
            // Ground the gesture has already passed over keeps what that pass
            // gave it. Re-feathering finished bins is what made the stroke lag.
            if (pos >= sweptLo && pos <= sweptHi)
                continue;

            // Outside the swept span, the skirt aims at whichever value the
            // stroke really has on that side: the segment's own endpoint where
            // the stroke is advancing, and the value recorded at the gesture's
            // extreme where it is not. Using the segment's endpoint on the
            // trailing side would drag the start of the stroke a little further
            // with every mouse move; using the recorded extreme on the leading
            // side would leave the newest ground one segment stale.
            if (pos < lo)
                target = extendsLow ? dbAtLo : sweptLoDb;
            else
                target = extendsHigh ? dbAtHi : sweptHiDb;
        }

        // Raised cosine: 1 under the stroke, 0 at the rim, zero slope at both
        // ends so overlapping strokes leave no ridge.
        const float w = 0.5f * (1.0f + std::cos (3.14159265358979f * d)) * pressure;

        if (brush == Brush::erase)
        {
            target = 0.0f;
        }
        else if (brush == Brush::smooth)
        {
            const int n0 = std::max (0, i - int (radiusBins));
            const int n1 = std::min (LogGrid::kSize - 1, i + int (radiusBins));
            float sum = 0.0f;

            for (int n = n0; n <= n1; ++n)
                sum += source[size_t (n)];

            target = sum / float (n1 - n0 + 1);
        }

        grid[size_t (i)] = LogGrid::clampDb (grid[size_t (i)] + w * (target - grid[size_t (i)]));
    }

    if (extendsLow)  { sweptLo = lo; sweptLoDb = dbAtLo; }
    if (extendsHigh) { sweptHi = hi; sweptHiDb = dbAtHi; }

    hasSwept = true;
}

// ---------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------

void CurveModel::setCurve (const CurveArray& c)
{
    const std::lock_guard<std::mutex> g (lock);
    pushUndoLocked();

    for (size_t i = 0; i < c.size(); ++i)
        grid[i] = LogGrid::clampDb (c[i]);

    touch();
}

CurveArray CurveModel::getCurve() const
{
    const std::lock_guard<std::mutex> g (lock);
    return grid;
}

float CurveModel::dbAtHz (float hz) const
{
    const std::lock_guard<std::mutex> g (lock);

    const float c  = LogGrid::clampIndex (LogGrid::hzToIndex (hz));
    const int   i0 = int (c);
    const int   i1 = std::min (i0 + 1, LogGrid::kSize - 1);
    return grid[size_t (i0)] + (c - float (i0)) * (grid[size_t (i1)] - grid[size_t (i0)]);
}

void CurveModel::fillSnapshot (CurveSnapshot& s) const
{
    const std::lock_guard<std::mutex> g (lock);
    s.raw     = grid;
    s.version = ver.load (std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Undo
// ---------------------------------------------------------------------------

void CurveModel::pushUndoLocked()
{
    if (undoStack.size() == kUndoDepth)
        undoStack.erase (undoStack.begin());

    undoStack.push_back (grid);
    redoStack.clear();
}

bool CurveModel::undo()
{
    const std::lock_guard<std::mutex> g (lock);

    if (undoStack.empty())
        return false;

    redoStack.push_back (grid);
    grid = undoStack.back();
    undoStack.pop_back();
    touch();
    return true;
}

bool CurveModel::redo()
{
    const std::lock_guard<std::mutex> g (lock);

    if (redoStack.empty())
        return false;

    undoStack.push_back (grid);
    grid = redoStack.back();
    redoStack.pop_back();
    touch();
    return true;
}

bool CurveModel::canUndo() const { const std::lock_guard<std::mutex> g (lock); return ! undoStack.empty(); }
bool CurveModel::canRedo() const { const std::lock_guard<std::mutex> g (lock); return ! redoStack.empty(); }

// ---------------------------------------------------------------------------
// Serialisation
//
// Layout: 'G','R','P','H' | uint16 version | uint16 gridSize | gridSize float32
// Little-endian throughout. Every target platform is little-endian; if that
// ever stops being true this is the one place that needs byte swapping.
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> CurveModel::serialise() const
{
    const std::lock_guard<std::mutex> g (lock);

    std::vector<std::uint8_t> out (8 + sizeof (float) * LogGrid::kSize);

    out[0] = 'G'; out[1] = 'R'; out[2] = 'P'; out[3] = 'H';

    const std::uint16_t v = kCurveVersion;
    const std::uint16_t n = std::uint16_t (LogGrid::kSize);
    std::memcpy (out.data() + 4, &v, 2);
    std::memcpy (out.data() + 6, &n, 2);
    std::memcpy (out.data() + 8, grid.data(), sizeof (float) * LogGrid::kSize);

    return out;
}

bool CurveModel::deserialise (const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < 8)
        return false;

    if (data[0] != 'G' || data[1] != 'R' || data[2] != 'P' || data[3] != 'H')
        return false;

    std::uint16_t v = 0, n = 0;
    std::memcpy (&v, data + 4, 2);
    std::memcpy (&n, data + 6, 2);

    if (v > kCurveVersion || n == 0)
        return false;

    if (size < 8 + sizeof (float) * std::size_t (n))
        return false;

    std::vector<float> stored (n);
    std::memcpy (stored.data(), data + 8, sizeof (float) * std::size_t (n));

    CurveArray loaded {};

    if (n == LogGrid::kSize)
    {
        std::copy (stored.begin(), stored.end(), loaded.begin());
    }
    else
    {
        // A different grid size is a format migration, not an error: resample
        // it. Both grids are log-spaced over the same range, so the mapping is
        // a straight linear stretch of the index.
        for (int i = 0; i < LogGrid::kSize; ++i)
        {
            const float pos = float (i) * float (n - 1) / float (LogGrid::kSize - 1);
            const int   j0  = std::clamp (int (pos), 0, n - 1);
            const int   j1  = std::min (j0 + 1, n - 1);
            loaded[size_t (i)] = stored[size_t (j0)]
                               + (pos - float (j0)) * (stored[size_t (j1)] - stored[size_t (j0)]);
        }
    }

    for (auto& x : loaded)
    {
        if (! std::isfinite (x))
            return false;

        x = LogGrid::clampDb (x);
    }

    {
        const std::lock_guard<std::mutex> g (lock);
        grid = loaded;
        undoStack.clear();
        redoStack.clear();
        touch();
    }

    return true;
}

} // namespace graphite
