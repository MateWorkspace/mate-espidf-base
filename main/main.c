#include "composition/main/launcher.h"
#include "sdkconfig.h"

#if CONFIG_MATE_TEST_SEED_NVS_ON_BOOT
#include "composition/test_seed/seed.h"
#endif

void app_main(void) {
#if CONFIG_MATE_TEST_SEED_NVS_ON_BOOT
    cmp_test_seed_run();
#else
    cmp_main_launcher();
#endif
}
