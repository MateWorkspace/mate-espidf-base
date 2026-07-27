#ifndef COMPOSITION_TEST_SEED_SEED_H
#define COMPOSITION_TEST_SEED_SEED_H

#ifdef __cplusplus
extern "C" {
#endif

/* Only built into app_main() when CONFIG_MATE_TEST_SEED_NVS_ON_BOOT is
   enabled - see main/Kconfig.projbuild. Writes fixed test WiFi/MQTT
   credentials into the "mate" NVS namespace, logs the result, and returns
   (app_main() then idles - there is nothing else to run in this build). */
void cmp_test_seed_run(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSITION_TEST_SEED_SEED_H */
