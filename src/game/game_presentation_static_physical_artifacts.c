#include "game/game_presentation_static_physical_artifacts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int path_usable(const char *path) {
    return path && path[0] && strlen(path) < MAX_PATH;
}

static int ensure_directory_tree(const char *directory) {
    char path[MAX_PATH];
    size_t length;
    size_t i;
    if (!path_usable(directory)) return 0;
    length = strlen(directory);
    memcpy(path, directory, length + 1);
    for (i = 0; i < length; i++) {
        DWORD error;
        if (path[i] != '/' && path[i] != '\\') continue;
        if (i == 0 || (i == 2 && path[1] == ':')) continue;
        path[i] = '\0';
        if (!CreateDirectoryA(path, NULL)) {
            error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) return 0;
        }
        path[i] = directory[i];
    }
    if (CreateDirectoryA(path, NULL)) return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

const char *static_physical_probe_artifact_dir(void) {
    const char *configured = getenv(PRESENTATION_PROBE_DIR_ENV);
    return path_usable(configured) ? configured : PRESENTATION_PROBE_DEFAULT_DIR;
}

int static_physical_probe_prepare_artifact_dir(void) {
    return ensure_directory_tree(static_physical_probe_artifact_dir());
}

int static_physical_probe_join_path(char *out, size_t out_size,
                                    const char *directory, const char *name) {
    int written;
    if (!out || out_size == 0 || !path_usable(directory) || !name || !name[0]) return 0;
    written = snprintf(out, out_size, "%s/%s", directory, name);
    return written > 0 && (size_t)written < out_size && written < MAX_PATH;
}

static int ensure_parent_directory(const char *path) {
    char parent[MAX_PATH];
    char *slash;
    char *backslash;
    size_t length;
    if (!path_usable(path)) return 0;
    length = strlen(path);
    memcpy(parent, path, length + 1);
    slash = strrchr(parent, '/');
    backslash = strrchr(parent, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
    if (!slash) return 1;
    *slash = '\0';
    return parent[0] ? ensure_directory_tree(parent) : 1;
}

int static_physical_probe_summary_path(char *out, size_t out_size) {
    const char *configured = getenv(PRESENTATION_PROBE_SUMMARY_ENV);
    int ok;
    if (!out || out_size == 0) return 0;
    if (path_usable(configured)) {
        size_t length = strlen(configured);
        if (length < out_size) {
            memcpy(out, configured, length + 1);
            return ensure_parent_directory(out);
        }
    }
    ok = static_physical_probe_join_path(out, out_size,
                                         static_physical_probe_artifact_dir(),
                                         "summary.txt");
    return ok && ensure_parent_directory(out);
}

int static_physical_probe_canvas_open(StaticPhysicalProbeCanvas *canvas,
                                      int width, int height) {
    HDC screen;
    if (!canvas || width <= 0 || height <= 0) return 0;
    memset(canvas, 0, sizeof(*canvas));
    canvas->width = width;
    canvas->height = height;
    canvas->info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    canvas->info.bmiHeader.biWidth = width;
    canvas->info.bmiHeader.biHeight = -height;
    canvas->info.bmiHeader.biPlanes = 1;
    canvas->info.bmiHeader.biBitCount = 32;
    canvas->info.bmiHeader.biCompression = BI_RGB;
    screen = GetDC(NULL);
    if (!screen) return 0;
    canvas->dc = CreateCompatibleDC(screen);
    canvas->bitmap = CreateDIBSection(screen, &canvas->info, DIB_RGB_COLORS,
                                      (void **)&canvas->pixels, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!canvas->dc || !canvas->bitmap || !canvas->pixels) {
        static_physical_probe_canvas_close(canvas);
        return 0;
    }
    canvas->previous = (HBITMAP)SelectObject(canvas->dc, canvas->bitmap);
    return 1;
}

void static_physical_probe_canvas_close(StaticPhysicalProbeCanvas *canvas) {
    if (!canvas) return;
    if (canvas->dc && canvas->previous) SelectObject(canvas->dc, canvas->previous);
    if (canvas->bitmap) DeleteObject(canvas->bitmap);
    if (canvas->dc) DeleteDC(canvas->dc);
    memset(canvas, 0, sizeof(*canvas));
}

void static_physical_probe_canvas_clear(StaticPhysicalProbeCanvas *canvas) {
    if (canvas && canvas->pixels) {
        if (canvas->dc) GdiFlush();
        memset(canvas->pixels, 0,
               (size_t)canvas->width * (size_t)canvas->height * sizeof(*canvas->pixels));
    }
}

int static_physical_probe_canvas_write(const StaticPhysicalProbeCanvas *canvas,
                                       const char *directory, const char *name) {
    BITMAPFILEHEADER header = {0};
    char path[MAX_PATH];
    FILE *file;
    size_t bytes;
    int ok;
    if (!canvas || !canvas->pixels || !ensure_directory_tree(directory) ||
        !static_physical_probe_join_path(path, sizeof(path), directory, name)) return 0;
    file = fopen(path, "wb");
    if (!file) return 0;
    bytes = (size_t)canvas->width * (size_t)canvas->height * sizeof(*canvas->pixels);
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + (DWORD)bytes;
    ok = fwrite(&header, sizeof(header), 1, file) == 1 &&
         fwrite(&canvas->info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file) == 1 &&
         fwrite(canvas->pixels, bytes, 1, file) == 1;
    fclose(file);
    return ok;
}

uint64_t static_physical_probe_canvas_hash(const StaticPhysicalProbeCanvas *canvas) {
    uint64_t hash = UINT64_C(1469598103934665603);
    int i;
    int count;
    if (!canvas || !canvas->pixels) return 0;
    count = canvas->width * canvas->height;
    for (i = 0; i < count; i++) {
        hash ^= canvas->pixels[i] & UINT32_C(0x00ffffff);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
