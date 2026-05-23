#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "aircraft_telemetry.h"

LOG_MODULE_REGISTER(aircraft_broadcaster, LOG_LEVEL_INF);

static const char *build_time = __DATE__ " " __TIME__;
static struct aircraft_telemetry_state aircraft_state;
static struct aircraft_telemetry_runtime aircraft_runtime;

int main(void)
{
  LOG_INF("Starting aircraft telemetry broadcaster");
  printk("\x1b[0m\x1b[33mFirmware build time: %s\x1b[0m\n", build_time);
  aircraft_telemetry_init(&aircraft_state, &aircraft_runtime);

  while (1)
  {
    aircraft_telemetry_update(&aircraft_state, &aircraft_runtime, k_uptime_get_32());

    LOG_INF("Aircraft %08x: lat=%d lon=%d alt=%d m roc=%d.%d m/s speed=%u.%u kph heading=%u.%u timestamp=%u ms",
            aircraft_state.aircraft_id,
            aircraft_state.latitude_e7, aircraft_state.longitude_e7,
            aircraft_state.altitude_m,
            aircraft_state.rate_of_climb_mps_x10 / 10, abs(aircraft_state.rate_of_climb_mps_x10 % 10),
            aircraft_state.speed_kph_x10 / 10, aircraft_state.speed_kph_x10 % 10,
            aircraft_state.heading_deg_x10 / 10, aircraft_state.heading_deg_x10 % 10,
            aircraft_state.timestamp_ms);

    k_busy_wait(1000000);
  }

  return 0;
}
