#include "render/sea_lane_dash_cache.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct {
    short point_index;
    int start_units;
    int end_units;
    int length_units;
} CachedDashSegment;

typedef struct {
    int valid;
    unsigned int key;
    int segment_count;
    int segment_capacity;
    CachedDashSegment *segments;
} DashCacheEntry;

static DashCacheEntry dash_cache[SEA_LANE_DASH_CACHE_ENTRY_COUNT];
static int dash_hits;
static int dash_misses;
static int dash_last_rebuild_ms;
static int dash_segments_drawn;
static int dash_miss_initial;
static int dash_miss_key;
static int dash_miss_overflow;
static const char *dash_last_reason = "none";

static unsigned int mix_key(unsigned int key, int value) {
    return key * 1000003u ^ (unsigned int)value;
}

static int world_segment_units(MapPoint a, MapPoint b) {
    int dx = abs(a.x - b.x);
    int dy = abs(a.y - b.y);
    int diagonal = dx < dy ? dx : dy;
    int straight = (dx > dy ? dx : dy) - diagonal;
    return diagonal * 14 + straight * 10;
}

static POINT interpolate_screen_point(POINT a, POINT b, int pos, int length) {
    POINT point;
    if (length <= 0) return a;
    point.x = a.x + (b.x - a.x) * pos / length;
    point.y = a.y + (b.y - a.y) * pos / length;
    return point;
}

static int dash_phase_for_path(const MapPoint *points, int count, int period) {
    int seed;
    if (count < 2 || period <= 0) return 0;
    seed = points[0].x * 31 + points[0].y * 47 +
           points[count - 1].x * 61 + points[count - 1].y * 89;
    if (seed < 0) seed = -seed;
    return seed % period;
}

static int ensure_segment_capacity(DashCacheEntry *entry, int needed) {
    int new_capacity;
    CachedDashSegment *new_segments;
    if (needed <= entry->segment_capacity) return 1;
    new_capacity = entry->segment_capacity > 0 ? entry->segment_capacity * 2 : 64;
    while (new_capacity < needed) new_capacity *= 2;
    new_segments = (CachedDashSegment *)realloc(entry->segments,
                                                (size_t)new_capacity * sizeof(entry->segments[0]));
    if (!new_segments) return 0;
    entry->segments = new_segments;
    entry->segment_capacity = new_capacity;
    return 1;
}

static int append_segment(DashCacheEntry *entry, int point_index,
                          int start_units, int end_units, int length_units) {
    CachedDashSegment *segment;
    if (!ensure_segment_capacity(entry, entry->segment_count + 1)) return 0;
    segment = &entry->segments[entry->segment_count++];
    segment->point_index = (short)point_index;
    segment->start_units = start_units;
    segment->end_units = end_units;
    segment->length_units = length_units;
    return 1;
}

static int rebuild_dash_segments(DashCacheEntry *entry, const MapPoint *map_points,
                                 int point_count, int dash_units, int gap_units) {
    int period = dash_units + gap_units;
    int pattern_pos;
    int i;
    entry->segment_count = 0;
    if (point_count < 2 || dash_units <= 0 || gap_units < 0 || period <= 0) return 1;
    pattern_pos = dash_phase_for_path(map_points, point_count, period) % period;
    if (pattern_pos < 0) pattern_pos += period;
    for (i = 1; i < point_count; i++) {
        int segment_len = world_segment_units(map_points[i - 1], map_points[i]);
        int pos = 0;
        if (segment_len <= 0) continue;
        while (pos < segment_len) {
            int in_dash = pattern_pos < dash_units;
            int remain = in_dash ? dash_units - pattern_pos : period - pattern_pos;
            int take = remain < segment_len - pos ? remain : segment_len - pos;
            if (in_dash && take > 0) {
                if (!append_segment(entry, i, pos, pos + take, segment_len)) return 0;
            }
            pos += take;
            pattern_pos = (pattern_pos + take) % period;
        }
    }
    return 1;
}

static void draw_cached_segments(HDC hdc, const POINT *screen_points,
                                 int point_count, const DashCacheEntry *entry) {
    int i;
    for (i = 0; i < entry->segment_count; i++) {
        const CachedDashSegment *segment = &entry->segments[i];
        POINT p;
        POINT q;
        int idx = segment->point_index;
        if (idx <= 0 || idx >= point_count || segment->length_units <= 0) continue;
        p = interpolate_screen_point(screen_points[idx - 1], screen_points[idx],
                                     segment->start_units, segment->length_units);
        q = interpolate_screen_point(screen_points[idx - 1], screen_points[idx],
                                     segment->end_units, segment->length_units);
        if (abs(q.x - p.x) < 2 && abs(q.y - p.y) < 2) continue;
        MoveToEx(hdc, p.x, p.y, NULL);
        LineTo(hdc, q.x, q.y);
        dash_segments_drawn++;
    }
}

void sea_lane_dash_cache_begin_frame(void) {
    dash_segments_drawn = 0;
}

int sea_lane_dash_cache_draw(HDC hdc, int cache_id, unsigned int route_key,
                             const MapPoint *map_points, const POINT *screen_points,
                             int point_count, int dash_units, int gap_units) {
    DashCacheEntry *entry;
    unsigned int key;
    DWORD start;
    if (cache_id < 0 || cache_id >= SEA_LANE_DASH_CACHE_ENTRY_COUNT ||
        !map_points || !screen_points || point_count < 2) return 0;
    entry = &dash_cache[cache_id];
    key = mix_key(mix_key(route_key, dash_units), gap_units);
    if (entry->valid && entry->key == key) {
        dash_hits++;
    } else {
        start = GetTickCount();
        if (!entry->valid) { dash_miss_initial++; dash_last_reason = "initial"; }
        else { dash_miss_key++; dash_last_reason = "key"; }
        entry->key = key;
        entry->valid = rebuild_dash_segments(entry, map_points, point_count,
                                             dash_units, gap_units);
        if (!entry->valid) {
            dash_miss_overflow++;
            dash_last_reason = "overflow";
            entry->segment_count = 0;
        }
        dash_last_rebuild_ms = (int)(GetTickCount() - start);
        dash_misses++;
    }
    draw_cached_segments(hdc, screen_points, point_count, entry);
    return entry->segment_count;
}

int sea_lane_dash_cache_hits(void) { return dash_hits; }
int sea_lane_dash_cache_misses(void) { return dash_misses; }
int sea_lane_dash_cache_last_rebuild_ms(void) { return dash_last_rebuild_ms; }
int sea_lane_dash_cache_segments_drawn(void) { return dash_segments_drawn; }
const char *sea_lane_dash_cache_last_reason(void) { return dash_last_reason; }
const char *sea_lane_dash_cache_reason_summary(void) {
    static char text[96];
    snprintf(text, sizeof(text), "init %d / key %d / overflow %d",
             dash_miss_initial, dash_miss_key, dash_miss_overflow);
    return text;
}
