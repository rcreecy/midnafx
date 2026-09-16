#pragma once
#include <mods/service.hpp>
#include <mods/svc/config.h>
#include <mods/svc/log.h>
#include <mods/svc/ui.h>

typedef struct GfxService GfxService;

extern ModContext* mod_ctx;
extern const GfxService* svc_gfx;
extern const ConfigService* svc_config;
extern const UiService* svc_ui;
extern const LogService* svc_log;
