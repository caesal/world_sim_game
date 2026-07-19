#include "render/render_world_physical_prewarm.h"

#include "core/render_snapshot.h"
#include "render/render_static_physical_cache.h"

int render_world_physical_prewarm_from_published(HWND hwnd) {
    const RenderSnapshot *snapshot = render_snapshot_acquire();
    HDC hdc;
    int ok = 0;
    if (!snapshot || !snapshot->world_generated) {
        if (snapshot) render_snapshot_release(snapshot);
        return 0;
    }
    hdc = GetDC(hwnd);
    if (hdc) {
        ok = render_static_physical_cache_prewarm(hdc, snapshot);
        ReleaseDC(hwnd, hdc);
    }
    render_snapshot_release(snapshot);
    return ok;
}
