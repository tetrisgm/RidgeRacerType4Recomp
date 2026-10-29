/* Included after r4_pass: workers execute that same draw recipe. */
#include <stdlib.h>
#include <stdio.h>
#include "draw_worker_pool.h"
#include "draw_service.h"
#include "present_thread.h"
#include "render_pass.h"
#include "psx_sdl.h"
#include "psx_video_timing.h"
#include "r4_draw_mods.h"
#include "r4_output_window.h"

typedef struct R4DrawFrame {
    unsigned refs; /* created and released only on main */
    PsxCtxSnapshot *snapshot;
    GpuReplayState *gpu;
    R4Blend blend;
} R4DrawFrame;
typedef struct R4DrawJob {
    R4DrawFrame *frame;
    R4DrawMods *mods;
    PsxExecCtx *context;
    RenderPassAsyncToken token;
    uint64_t elapsed_ns;
    size_t next_list, active_list, next_node;
} R4DrawJob;
static struct {
    DrawWorkerPool *pool;
    PsxExecCtx *contexts[PSX_CTX_MAX];
    unsigned count, pending;
    uint64_t session, window_epoch;
    R4OutputWindow window;
    uint64_t submitted, admitted, dropped, failures, elapsed_ns, executed;
    uint64_t captures, capture_ns;
    int64_t lead_min_ns, lead_max_ns;
    uint64_t lead_hist[8];     /* ticks by lead in whole windows (7 = 7+) */
    uint64_t extras_hist[4];   /* ticks by worker outputs reserved (3 = 3+) */
    uint64_t reanchors;        /* ticks skipped for lead (R4_OUTPUT_LEAD_MAX) */
    int lead_max_windows;      /* -1 until read */
    int initialized;
} workers;
static _Thread_local R4DrawJob *worker_job;
static void r4_workers_reset(void);

static uint64_t r4_worker_clock(void *unused) {
    (void)unused; return pt_clock_ns();
}
static void r4_frame_release(R4DrawFrame *frame) {
    if (--frame->refs) return;
    psx_ctx_snapshot_release(frame->snapshot);
    gpu_replay_release(frame->gpu);
    free(frame);
}
static void r4_job_dispose(void *payload) {
    R4DrawJob *job = payload;
    r4_draw_mods_destroy(job->mods);
    r4_frame_release(job->frame);
    free(job);
}
static int r4_worker_entry(CPUState *cpu, uint32_t address, void *unused) {
    (void)unused;
    if (!worker_job || worker_job->context != cpu->ctx) return 0;
    if (!r4_draw_mods_entry(worker_job->mods, cpu, address))
        psx_dctx_fault(cpu, PSX_CTX_FAULT_UNSUPPORTED, address, "private draw mod hook");
    return 0; /* run the original guest body after its entry hooks */
}
static int r4_worker_execute(void *unused, unsigned slot, void *payload) {
    (void)unused;
    R4DrawJob *job = payload;
    PsxExecCtx *ctx = workers.contexts[slot];
    uint64_t start = pt_clock_ns();
    job->context = ctx;
    if (!psx_ctx_begin(ctx, job->frame->snapshot)) return -3;
    worker_job = job;
    int ok = r4_pass(psx_ctx_cpu(ctx), &job->frame->blend, job->token.output.alpha_q16);
    worker_job = NULL;
    job->elapsed_ns = pt_clock_ns() - start;
    return ok ? DRAW_WORKER_COMPLETE : -3;
}
static int r4_worker_events(R4DrawJob *job, size_t index) {
    PsxExecCtx *ctx = job->context;
    for (;;) {
        if (job->active_list != SIZE_MAX) {
            const PsxCtxGpuList *list = &ctx->gpu_lists[job->active_list];
            while (job->next_node < list->node_end &&
                   ctx->gpu_nodes[job->next_node].word_offset <= index) {
                const PsxCtxGpuNode *node = &ctx->gpu_nodes[job->next_node++];
                if (!gpu_replay_list_node(node->address, node->header)) return 0;
            }
            if (index < list->word_end) return 1;
            if (!gpu_replay_list_end()) return 0;
            job->active_list = SIZE_MAX;
        }
        if (job->next_list == ctx->gpu_lists_n ||
            ctx->gpu_lists[job->next_list].word_begin > index) return 1;
        job->active_list = job->next_list++;
        const PsxCtxGpuList *list = &ctx->gpu_lists[job->active_list];
        job->next_node = list->node_begin;
        /* Tags precede the OT prepass. Its reads select this DMA's immutable
         * packet bytes even when a later draw reused their addresses. */
        if (!r4_draw_mods_replay_metadata(job->mods, index) ||
            !gpu_replay_list_begin(list->root)) return 0;
    }
}
static int r4_worker_word(void *user, size_t index, GpuReplayWord *out) {
    R4DrawJob *job = user;
    size_t count;
    const PsxCtxGpuWord *words = psx_ctx_gpu_words(job->context, &count);
    if (index >= count || !r4_worker_events(job, index) ||
        !r4_draw_mods_replay_metadata(job->mods, index)) return 0;
    const PsxCtxGpuWord *w = &words[index];
    memset(out, 0, sizeof *out);
    out->word = w->word; out->source = w->src;
    out->vertex.x16 = w->vertex.x16; out->vertex.y16 = w->vertex.y16;
    out->vertex.sz = w->vertex.sz; out->vertex.source = w->vertex.source;
    out->vertex.raw_x16 = w->vertex.raw_x16; out->vertex.raw_y16 = w->vertex.raw_y16;
    out->vertex.raw_sz = w->vertex.raw_sz; out->vertex.raw_valid = w->vertex.raw_valid;
    out->vertex.unclamped_depth = w->vertex.unclamped_depth;
    out->vertex.projection_valid = w->vertex.projection_valid;
    out->vertex.projection = w->vertex.projection;
    return 1;
}
static int r4_worker_read(void *user, uint32_t addr, unsigned width, uint32_t *out) {
    R4DrawJob *job = user;
    uint32_t phys = addr & 0x1FFFFFFFu;
    if ((width != 1 && width != 2 && width != 4) || (addr & (width - 1u))) return 0;
    if (phys >= 0x800000u && (phys < 0x1F800000u || phys + width > 0x1F800400u)) return 0;
    if (job->active_list != SIZE_MAX &&
        psx_ctx_gpu_list_read(job->context, job->active_list, addr, width, out)) return 1;
    *out = psx_ctx_read32(job->context, addr & ~3u) >> ((addr & 3u) * 8u);
    if (width < 4) *out &= (1u << (width * 8u)) - 1u;
    return 1;
}
static int r4_worker_metadata(void *user) {
    R4DrawJob *job = user;
    job->next_list = job->next_node = 0;
    job->active_list = SIZE_MAX;
    return 1; /* job creation already captured every supported active hook */
}
static int r4_worker_finish(void *user) {
    R4DrawJob *job = user;
    return r4_worker_events(job, job->context->gpu_n) &&
           r4_draw_mods_replay_metadata(job->mods, job->context->gpu_n);
}
static void r4_workers_poll(void *unused) {
    (void)unused;
    if (!workers.pool || g_psx_guest_time_frozen) return;
    if (r4_optional_draw_hooks_active() && workers.window.denominator) r4_workers_reset();
    DrawWorkerResult result;
    while (draw_worker_poll(workers.pool, &result)) {
        R4DrawJob *job = result.job.payload;
        int admitted = 0;
        if (draw_worker_current(workers.pool, &result, result.job.generation)) {
            GpuReplayProvider provider = {0};
            provider.user = job;
            (void)psx_ctx_gpu_words(job->context, &provider.count);
            provider.word = r4_worker_word; provider.read = r4_worker_read;
            provider.prepare_metadata = r4_worker_metadata;
            provider.finish = r4_worker_finish;
            admitted = render_pass_async_admit(&job->token, job->frame->gpu, &provider);
        }
        workers.elapsed_ns += job->elapsed_ns;
        if (job->elapsed_ns) workers.executed++;
        if (admitted) workers.admitted++; else workers.dropped++;
        if (result.executed && result.execution_status == -3 && workers.failures++ < 8) {
            uint32_t pc = 0; const char *what = NULL;
            PsxCtxStatus status = psx_ctx_status(job->context, &pc, &what);
            fprintf(stderr, "R4 worker draw failed: %s pc=%08X %s\n",
                    psx_ctx_status_name(status), pc, what ? what : "");
            CPUState *failed_cpu = psx_ctx_cpu(job->context);
            uint32_t trail[16], fault_ra = 0;
            int trail_count = psx_ctx_trail(job->context, trail, 16, &fault_ra);
            fprintf(stderr, "R4 worker fault: t1=%08X ra=%08X fault_ra=%08X trail=",
                    failed_cpu->gpr[9], failed_cpu->gpr[31], fault_ra);
            for (int ti = 0; ti < trail_count; ++ti) fprintf(stderr, "%08X,", trail[ti]);
            fprintf(stderr, "\n");
        }
        (void)draw_worker_release(workers.pool, &result);
        workers.pending--;
    }
    if (!workers.pending) draw_service_register(NULL, NULL);
}
static void r4_workers_shutdown(void) {
    draw_service_register(NULL, NULL);
    draw_worker_destroy(workers.pool);
    workers.pool = NULL;
    for (unsigned i = 0; i < workers.count; i++) psx_ctx_destroy(workers.contexts[i]);
    workers.count = 0;
    fprintf(stderr, "R4 workers: submitted=%llu admitted=%llu dropped=%llu failures=%llu executed=%llu draw_ms=%.3f capture_ms=%.3f\n",
            (unsigned long long)workers.submitted, (unsigned long long)workers.admitted,
            (unsigned long long)workers.dropped, (unsigned long long)workers.failures,
            (unsigned long long)workers.executed,
            workers.executed ? workers.elapsed_ns / (1000000.0 * workers.executed) : 0.0,
            workers.captures ? workers.capture_ns / (1000000.0 * workers.captures) : 0.0);
    fprintf(stderr, "R4 worker lead_ms: min=%.3f max=%.3f skipped_for_lead=%llu "
            "lead_windows=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu+ extras=%llu,%llu,%llu,%llu+\n",
            workers.lead_min_ns / 1000000.0, workers.lead_max_ns / 1000000.0,
            (unsigned long long)workers.reanchors,
            (unsigned long long)workers.lead_hist[0], (unsigned long long)workers.lead_hist[1],
            (unsigned long long)workers.lead_hist[2], (unsigned long long)workers.lead_hist[3],
            (unsigned long long)workers.lead_hist[4], (unsigned long long)workers.lead_hist[5],
            (unsigned long long)workers.lead_hist[6], (unsigned long long)workers.lead_hist[7],
            (unsigned long long)workers.extras_hist[0], (unsigned long long)workers.extras_hist[1],
            (unsigned long long)workers.extras_hist[2], (unsigned long long)workers.extras_hist[3]);
}
static void r4_workers_reset(void) {
    workers.window = (R4OutputWindow){0};
    if (++workers.session == 0) ++workers.session;
    if (workers.pool) {
        draw_worker_set_epoch(workers.pool, workers.session);
        gl_renderer_pass_async_invalidate();
    }
}
static int r4_workers_init(void) {
    if (workers.initialized) return workers.pool != NULL;
    const char *opt = getenv("R4_DRAW_WORKERS");
    if ((opt && !strcmp(opt, "0")) || !psx_ctx_available()) return 0;
    workers.initialized = 1;
    workers.lead_max_windows = -1;
    int count = opt && *opt ? atoi(opt) : 4;
    if (count <= 0) {
#ifdef PSX_SDL3
        count = SDL_GetNumLogicalCPUCores();
#else
        count = SDL_GetCPUCount();
#endif
        count = count > 4 ? count - 3 : 1;
    }
    if (count > 8) count = 8;
    if (!r4_draw_mods_register_entries(r4_worker_entry, NULL)) return 0;
    for (int i = 0; i < count; i++) {
        PsxExecCtx *ctx = psx_ctx_create();
        if (!ctx) break;
        workers.contexts[workers.count++] = ctx;
    }
    workers.pool = draw_worker_create(workers.count, workers.session,
                                     r4_worker_clock, r4_worker_execute, NULL);
    if (!workers.pool) { r4_workers_shutdown(); return 0; }
    atexit(r4_workers_shutdown);
    return 1;
}
static int r4_workers_submit(CPUState *cpu, const R4Blend *blend, const PSXModRenderPass *rect) {
    if (!r4_workers_init()) return 0;
    if (psx_mod_render_pass_status() != PSX_MOD_RENDER_PASS_READY) {
        if (workers.window.denominator) r4_workers_reset();
        return 0;
    }
    r4_workers_poll(NULL);
    if (r4_optional_draw_hooks_active()) return 0;
    /* Main reaches the same point once per simulation tick. The output grid
     * remains continuous across ticks; output rate never changes this span. */
    uint64_t now = pt_clock_ns();
    if (workers.window.denominator &&
        (workers.window_epoch != gl_renderer_present_epoch() ||
         workers.window.numerator != (uint64_t)g_psx_vblank_cycles * 2000000000ull))
        r4_workers_reset();
    if (!workers.window.denominator) {
        r4_output_start(&workers.window, now, g_psx_vblank_cycles);
        workers.window_epoch = gl_renderer_present_epoch();
    }
    r4_output_skip_elapsed(&workers.window, now);
    const uint64_t begin = r4_output_boundary(&workers.window, workers.window.index);
    const uint64_t end = r4_output_boundary(&workers.window, workers.window.index + 1);
    const uint64_t duration = end - begin;
    if (workers.lead_max_windows < 0) {
        const char *lm = getenv("R4_OUTPUT_LEAD_MAX");
        workers.lead_max_windows = lm && *lm ? atoi(lm) : 2;
    }
    /* Ticks that arrive faster than a window (the pacer catching up after a
     * stall, fast-forward) each push the grid one window ahead of now and
     * nothing pulls it back: every later output is scheduled that far in
     * the future, sits in a presenter slot until its deadline, and once the
     * slots are full the render thread waits for one per output. When the
     * grid is more than R4_OUTPUT_LEAD_MAX windows ahead (0 keeps the
     * ratchet), this tick reserves nothing and leaves the grid where it is:
     * the outputs already reserved cover the coming windows, the grid stays
     * aligned with the output timeline, and the lead shrinks by one window
     * per skipped tick. */
    if (r4_output_lead_excess(&workers.window, now, workers.lead_max_windows)) {
        workers.reanchors++;
        return 1;
    }
    int64_t lead = (int64_t)begin - (int64_t)now;
    if (!workers.submitted || lead < workers.lead_min_ns) workers.lead_min_ns = lead;
    if (!workers.submitted || lead > workers.lead_max_ns) workers.lead_max_ns = lead;
    {
        int64_t w = lead > 0 ? lead / (int64_t)duration : 0;
        workers.lead_hist[w > 7 ? 7 : w]++;
    }
    workers.window.index++;
    RenderPassAsyncToken tokens[R4_MAX_PASSES];
    unsigned n = render_pass_async_reserve(2, begin, end,
        rect->x, rect->y, rect->w, rect->h, tokens, R4_MAX_PASSES);
    unsigned extras = 0;
    for (unsigned i = 0; i < n; i++) extras += tokens[i].output.alpha_q16 != 0;
    workers.extras_hist[extras > 3 ? 3 : extras]++;
    if (!extras) return 1;
    if (workers.pending >= workers.count) { workers.dropped += extras; return 1; }
    uint64_t capture_start = pt_clock_ns();
    R4DrawFrame *frame = calloc(1, sizeof *frame);
    if (!frame) { workers.dropped += extras; return 1; }
    frame->refs = 1; frame->blend = *blend;
    frame->snapshot = psx_ctx_snapshot_capture(cpu, PSX_CTX_SNAP_PGXP);
    frame->gpu = gpu_replay_capture();
    if (!frame->snapshot || !frame->gpu) {
        r4_frame_release(frame); workers.dropped += extras; return 1;
    }
    R4DrawMods *first_mods = r4_draw_mods_capture();
    if (!first_mods) { r4_frame_release(frame); workers.dropped += extras; return 1; }
    workers.capture_ns += pt_clock_ns() - capture_start;
    workers.captures++;
    for (unsigned i = 0; i < n; i++) {
        if (!tokens[i].output.alpha_q16) continue; /* phase zero is captured by reserve */
        R4DrawJob *job = calloc(1, sizeof *job);
        if (!job) continue;
        job->mods = first_mods ? first_mods : r4_draw_mods_capture();
        first_mods = NULL;
        if (!job->mods) { free(job); continue; }
        job->frame = frame; frame->refs++;
        job->token = tokens[i];
        DrawWorkerJob work = { workers.session, tokens[i].output.generation,
            tokens[i].output.index, tokens[i].output.deadline, tokens[i].output.expires,
            job, r4_job_dispose };
        if (draw_worker_submit(workers.pool, &work)) { workers.pending++; workers.submitted++; }
        else { workers.dropped++; r4_job_dispose(job); }
    }
    r4_draw_mods_destroy(first_mods);
    r4_frame_release(frame);
    if (workers.pending) draw_service_register(r4_workers_poll, NULL);
    /* Once the async path owns this tick, a full/late admission drops optional
     * output. Falling back to serial here would put that draw back on main. */
    return 1;
}
