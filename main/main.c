#include "core.h"
#include "sdkconfig.h"
#include "server.h"

void app_main(void) {
  init_led();

  init_littlefs();

#if SOC_TEMP_SENSOR_SUPPORTED
  initialize_temperature_sensor();
#endif

  register_wifi_callback(start_webserver);

  init_wifi();
}
