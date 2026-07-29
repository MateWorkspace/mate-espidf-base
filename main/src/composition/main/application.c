#include "composition/main/application.h"

#include "application/internal/messaging_callbacks/impl.h"
#include "application/internal/ota/impl.h"
#include "application/internal/settings/impl.h"
#include "application/internal/wifi_manager/impl.h"

dom_models_error_t cmp_main_application_init(cmp_main_launcher_t* launcher) {
    if (!launcher) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    app_internal_ota_impl_cfg_t ota_cfg = {
        .logger           = launcher->infrastructure.logger,
        .system_update    = launcher->infrastructure.system_update,
        .system_restart   = launcher->infrastructure.system_restart,
        .restart_delay_ms = 0,
    };
    launcher->application.ota = app_internal_ota_impl_new(&ota_cfg);
    if (!launcher->application.ota) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    dom_models_error_t err = app_internal_ota_impl_init(launcher->application.ota);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    app_internal_settings_impl_cfg_t settings_cfg = {
        .logger               = launcher->infrastructure.logger,
        .preloaded_repository = launcher->infrastructure.preloaded_repository,
        .system_info          = launcher->infrastructure.system_info,
        .system_restart       = launcher->infrastructure.system_restart,
    };
    launcher->application.settings = app_internal_settings_impl_new(&settings_cfg);
    if (!launcher->application.settings) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    app_internal_wifi_manager_impl_cfg_t wifi_manager_cfg = {
        .logger               = launcher->infrastructure.logger,
        .wifi                 = launcher->infrastructure.wifi,
        .wifi_repository      = launcher->infrastructure.wifi_repository,
        .preloaded_repository = launcher->infrastructure.preloaded_repository,
    };
    launcher->application.wifi_manager = app_internal_wifi_manager_impl_new(&wifi_manager_cfg);
    if (!launcher->application.wifi_manager) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    err = app_internal_wifi_manager_impl_init(launcher->application.wifi_manager);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    err = launcher->application.wifi_manager->start(launcher->application.wifi_manager);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        return err;
    }

    app_internal_messaging_callbacks_impl_cfg_t messaging_callbacks_cfg = {
        .logger               = launcher->infrastructure.logger,
        .def_pub              = launcher->infrastructure.def_pub,
        .def_sub              = launcher->infrastructure.def_sub,
        .system_restart       = launcher->infrastructure.system_restart,
        .preloaded_repository = launcher->infrastructure.preloaded_repository,
        .system_info          = launcher->infrastructure.system_info,
    };
    launcher->application.messaging_callbacks = app_internal_messaging_callbacks_impl_new(&messaging_callbacks_cfg);
    if (!launcher->application.messaging_callbacks) {
        return DOMAIN_MODELS_ERROR_MALLOC_FAILED;
    }

    return DOMAIN_MODELS_ERROR_OK;
}

void cmp_main_application_deinit(cmp_main_launcher_t* launcher) {
    if (!launcher) {
        return;
    }

    if (launcher->application.messaging_callbacks) {
        app_internal_messaging_callbacks_impl_delete(launcher->application.messaging_callbacks);
        launcher->application.messaging_callbacks = NULL;
    }

    if (launcher->application.wifi_manager) {
        (void)launcher->application.wifi_manager->stop(launcher->application.wifi_manager);
        app_internal_wifi_manager_impl_deinit(launcher->application.wifi_manager);
        app_internal_wifi_manager_impl_delete(launcher->application.wifi_manager);
        launcher->application.wifi_manager = NULL;
    }

    if (launcher->application.settings) {
        app_internal_settings_impl_delete(launcher->application.settings);
        launcher->application.settings = NULL;
    }

    if (launcher->application.ota) {
        app_internal_ota_impl_deinit(launcher->application.ota);
        app_internal_ota_impl_delete(launcher->application.ota);
        launcher->application.ota = NULL;
    }
}
