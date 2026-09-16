#pragma once
#include <mods/service.hpp>
#include <mods/svc/camera.h>
#include <mods/svc/config.h>
#include <mods/svc/gfx.h>
#include <mods/svc/hook.h>
#include <mods/svc/log.h>
#include <mods/svc/ui.h>

extern ModContext* mod_ctx;
extern const ConfigService* svc_config;
extern const CameraService* svc_camera;
extern const HookService* svc_hook;
extern const UiService* svc_ui;
extern const LogService* svc_log;
