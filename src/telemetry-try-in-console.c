#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>

#include "aircraft_telemetry.h"

static struct aircraft_telemetry_state aircraft_state;
static struct aircraft_telemetry_runtime aircraft_runtime;

static void update_fake_aircraft_data(void)
{
  aircraft_telemetry_update(&aircraft_state, &aircraft_runtime,
                            (uint32_t)(aircraft_runtime.update_count * 1000U));
}

int main(void)
{
  printf("Starting console aircraft telemetry simulator\n");

  aircraft_telemetry_init(&aircraft_state, &aircraft_runtime);

  printf("Aircraft %08X | Lat: %.7f | Lon: %.7f | Alt: %dm | "
         "ROC: %d.%d m/s | Speed: %u.%u kph | Heading: %u.%u deg | t=%u ms\n",
         aircraft_state.aircraft_id,
         aircraft_state.latitude_e7 / 1e7f,
         aircraft_state.longitude_e7 / 1e7f,
         aircraft_state.altitude_m,
         aircraft_state.rate_of_climb_mps_x10 / 10,
         abs(aircraft_state.rate_of_climb_mps_x10 % 10),
         aircraft_state.speed_kph_x10 / 10,
         aircraft_state.speed_kph_x10 % 10,
         aircraft_state.heading_deg_x10 / 10,
         aircraft_state.heading_deg_x10 % 10,
         aircraft_state.timestamp_ms);

  while (1)
  {
    update_fake_aircraft_data();

    printf("Aircraft %08X | Lat: %.7f | Lon: %.7f | Alt: %dm | "
           "ROC: %d.%d m/s | Speed: %u.%u kph | Heading: %u.%u deg | t=%u ms\n",
           aircraft_state.aircraft_id,
           aircraft_state.latitude_e7 / 1e7f,
           aircraft_state.longitude_e7 / 1e7f,
           aircraft_state.altitude_m,
           aircraft_state.rate_of_climb_mps_x10 / 10,
           abs(aircraft_state.rate_of_climb_mps_x10 % 10),
           aircraft_state.speed_kph_x10 / 10,
           aircraft_state.speed_kph_x10 % 10,
           aircraft_state.heading_deg_x10 / 10,
           aircraft_state.heading_deg_x10 % 10,
           aircraft_state.timestamp_ms);

    usleep(1000 * 1000); // 1 second
  }
  return 0;
}
