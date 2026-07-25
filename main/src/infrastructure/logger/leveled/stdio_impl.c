#include "infrastructure/logger/leveled/stdio_impl.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "domain/contracts/logger/leveled.h"
#include "domain/models/logger.h"
#include "infrastructure/logger/leveled/stdio_impl_types.h"

#define LOG_MSG_MAX_LEN 512

/* Helper Function Prototypes */

static void print_log(inf_logger_leveled_stdio_impl_ctx_t* ctx, dom_models_logger_level_t level, const char* tag, const char* format, va_list args);
static void run_callbacks(inf_logger_leveled_stdio_impl_ctx_t* ctx, const char* msg, size_t msg_len);

/* Contract Function Prototypes */

static void error_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
);
static void warn_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
);
static void info_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
);
static void debug_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
);
static void add_callback_impl(
    dom_contracts_logger_leveled_t* self,
    void*                           cb_ctx,
    dom_contracts_logger_leveled_cb cb_func
);
static void remove_callback_impl(
    dom_contracts_logger_leveled_t* self,
    dom_contracts_logger_leveled_cb cb_func
);

/* Constructor and Destructor */

dom_contracts_logger_leveled_t* inf_logger_leveled_stdio_impl_new(const inf_logger_leveled_stdio_impl_cfg_t* cfg) {
    inf_logger_leveled_stdio_impl_ctx_t* ctx = (inf_logger_leveled_stdio_impl_ctx_t*)calloc(1, sizeof(inf_logger_leveled_stdio_impl_ctx_t));
    if (!ctx) {
        return NULL;
    }

    if (!cfg) {
        inf_logger_leveled_stdio_impl_cfg_t default_cfg = INF_LOGGER_LEVELED_STDIO_IMPL_CFG_DEFAULT();
        memcpy(&ctx->cfg, &default_cfg, sizeof(inf_logger_leveled_stdio_impl_cfg_t));
    } else {
        memcpy(&ctx->cfg, cfg, sizeof(inf_logger_leveled_stdio_impl_cfg_t));
    }

    if (ctx->cfg.cb_max_cnt > 0) {
        ctx->cb_funcs = (dom_contracts_logger_leveled_cb*)calloc(ctx->cfg.cb_max_cnt, sizeof(dom_contracts_logger_leveled_cb));
        ctx->cb_ctxs  = (void**)calloc(ctx->cfg.cb_max_cnt, sizeof(void*));
        if (!ctx->cb_funcs || !ctx->cb_ctxs) {
            free(ctx->cb_funcs);
            free(ctx->cb_ctxs);
            free(ctx);
            return NULL;
        }
    }

    dom_contracts_logger_leveled_t* self = dom_contracts_logger_leveled_new(ctx);
    if (!self) {
        free(ctx->cb_funcs);
        free(ctx->cb_ctxs);
        free(ctx);
        return NULL;
    }

    self->error           = error_impl;
    self->warn            = warn_impl;
    self->info            = info_impl;
    self->debug           = debug_impl;
    self->add_callback    = add_callback_impl;
    self->remove_callback = remove_callback_impl;

    return self;
}

void inf_logger_leveled_stdio_impl_delete(dom_contracts_logger_leveled_t* self) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    free(ctx->cb_funcs);
    free(ctx->cb_ctxs);
    free(ctx);
    dom_contracts_logger_leveled_delete(self);
}

/* Contract Function Implementations */

static void error_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    va_list args;
    va_start(args, format);
    print_log(ctx, DOMAIN_MODELS_LOGGER_LEVEL_ERROR, tag, format, args);
    va_end(args);
}

static void warn_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    va_list args;
    va_start(args, format);
    print_log(ctx, DOMAIN_MODELS_LOGGER_LEVEL_WARN, tag, format, args);
    va_end(args);
}

static void info_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    va_list args;
    va_start(args, format);
    print_log(ctx, DOMAIN_MODELS_LOGGER_LEVEL_INFO, tag, format, args);
    va_end(args);
}

static void debug_impl(
    dom_contracts_logger_leveled_t* self,
    const char*                     tag,
    const char*                     format,
    ...
) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    va_list args;
    va_start(args, format);
    print_log(ctx, DOMAIN_MODELS_LOGGER_LEVEL_DEBUG, tag, format, args);
    va_end(args);
}

static void add_callback_impl(
    dom_contracts_logger_leveled_t* self,
    void*                           cb_ctx,
    dom_contracts_logger_leveled_cb cb_func
) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    if (ctx->cb_idx >= ctx->cfg.cb_max_cnt) {
        return;
    }

    if (!ctx->cb_funcs || !ctx->cb_ctxs || !cb_func) {
        return;
    }

    ctx->cb_funcs[ctx->cb_idx] = cb_func;
    ctx->cb_ctxs[ctx->cb_idx]  = cb_ctx;
    ctx->cb_idx += 1;
}

static void remove_callback_impl(
    dom_contracts_logger_leveled_t* self,
    dom_contracts_logger_leveled_cb cb_func
) {
    if (!self || !self->ctx) {
        return;
    }

    inf_logger_leveled_stdio_impl_ctx_t* ctx = self->ctx;

    if (!ctx->cb_funcs || !ctx->cb_ctxs || !cb_func || ctx->cb_idx == 0) {
        return;
    }

    for (unsigned int i = 0; i < ctx->cb_idx; i++) {
        if (ctx->cb_funcs[i] != cb_func) {
            continue;
        }

        unsigned int last_idx = ctx->cb_idx - 1;

        ctx->cb_funcs[i] = NULL;
        ctx->cb_ctxs[i]  = NULL;

        if (i != last_idx) {
            ctx->cb_funcs[i] = ctx->cb_funcs[last_idx];
            ctx->cb_ctxs[i]  = ctx->cb_ctxs[last_idx];

            ctx->cb_funcs[last_idx] = NULL;
            ctx->cb_ctxs[last_idx]  = NULL;
        }

        ctx->cb_idx -= 1;
        return;
    }
}

/* Helper Function Implementations */

static void print_log(inf_logger_leveled_stdio_impl_ctx_t* ctx, dom_models_logger_level_t level, const char* tag, const char* format, va_list args) {
    if (ctx->cfg.level < level) {
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm timeinfo;
    localtime_r(&tv.tv_sec, &timeinfo);

    const char* level_str = dom_models_logger_level_str(level);
    const char* safe_tag  = tag ? tag : "";
    const char* safe_fmt  = format ? format : "";
    char        msg[LOG_MSG_MAX_LEN];
    size_t      remaining_len = sizeof(msg);
    size_t      msg_len       = 0;

    int prefix_len = snprintf(
        msg,
        remaining_len,
        "%02d/%02d/%04d %02d:%02d:%02d.%03ld [%s] [%s] ",
        timeinfo.tm_mday,
        timeinfo.tm_mon + 1,
        timeinfo.tm_year + 1900,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec,
        tv.tv_usec / 1000,
        level_str,
        safe_tag
    );
    if (prefix_len < 0) {
        return;
    }

    if ((size_t)prefix_len >= remaining_len) {
        msg_len = remaining_len - 1;
    } else {
        msg_len = (size_t)prefix_len;
        remaining_len -= msg_len;

        int body_len = vsnprintf(msg + msg_len, remaining_len, safe_fmt, args);
        if (body_len < 0) {
            return;
        }

        if ((size_t)body_len >= remaining_len) {
            msg_len = sizeof(msg) - 1;
        } else {
            msg_len += (size_t)body_len;
        }
    }

    fputs(msg, stdout);
    fputc('\n', stdout);
    run_callbacks(ctx, msg, msg_len);
}

static void run_callbacks(inf_logger_leveled_stdio_impl_ctx_t* ctx, const char* msg, size_t msg_len) {
    if (!ctx || !ctx->cb_funcs || !ctx->cb_ctxs || !msg) {
        return;
    }

    for (unsigned int i = 0; i < ctx->cb_idx; i++) {
        if (ctx->cb_funcs[i]) {
            ctx->cb_funcs[i](ctx->cb_ctxs[i], msg, msg_len);
        }
    }
}
