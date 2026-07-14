#include "game/game_plague_probe.h"

#include "game/game_plague_probe_internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>

#define PLAGUE_PROBE_DIR \
    "build/validation/named_plague_redesign_20260712/02_model_probe"
#define PLAGUE_PROBE_SUMMARY PLAGUE_PROBE_DIR "/summary.txt"

static int ensure_dir(const char *path) {
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int ensure_output_dirs(void) {
    return ensure_dir("build") &&
           ensure_dir("build/validation") &&
           ensure_dir("build/validation/named_plague_redesign_20260712") &&
           ensure_dir(PLAGUE_PROBE_DIR);
}

void plague_probe_check(PlagueProbeContext *context, const char *suite,
                        const char *name, int passed, const char *format, ...) {
    va_list args;
    if (!context || !context->output) return;
    context->checks++;
    if (!passed) context->failures++;
    fprintf(context->output, "suite=%s case=%s result=%s", suite, name,
            passed ? "PASS" : "FAIL");
    if (format && format[0]) {
        fputc(' ', context->output);
        va_start(args, format);
        vfprintf(context->output, format, args);
        va_end(args);
    }
    fputc('\n', context->output);
}

int run_plague_model_probe(void) {
    PlagueProbeContext context;
    FILE *output;
    if (!ensure_output_dirs()) return 2;
    output = fopen(PLAGUE_PROBE_SUMMARY, "w");
    if (!output) return 2;
    context.output = output;
    context.checks = 0;
    context.failures = 0;
    fprintf(output, "probe=named_plague_model deterministic=1\n");
    plague_probe_run_rules(&context);
    plague_probe_run_probability(&context);
    plague_probe_run_state(&context);
    plague_probe_run_spread(&context);
    plague_probe_run_population(&context);
    plague_probe_run_integration(&context);
    fprintf(output, "result=%s checks=%d failures=%d\n",
            context.failures == 0 ? "PASS" : "FAIL",
            context.checks, context.failures);
    fclose(output);
    return context.failures == 0 ? 0 : 1;
}
