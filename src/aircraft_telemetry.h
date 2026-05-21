#ifndef AIRCRAFT_TELEMETRY_H
#define AIRCRAFT_TELEMETRY_H

#include <math.h>
#include <stdint.h>

#define AIRCRAFT_TELEMETRY_START_LAT_E7 81005000
#define AIRCRAFT_TELEMETRY_START_LON_E7 989841639
#define AIRCRAFT_TELEMETRY_DEST_LAT_E7 462286000
#define AIRCRAFT_TELEMETRY_DEST_LON_E7 144542000
#define AIRCRAFT_TELEMETRY_ARRIVAL_RADIUS_M 5000.0f

#define AIRCRAFT_TELEMETRY_AIRCRAFT_ID 0xA1B2C3D4
#define AIRCRAFT_TELEMETRY_INITIAL_SPEED_KPH_X10 2900U

struct aircraft_telemetry_state
{
  uint32_t aircraft_id;
  int32_t latitude_e7;
  int32_t longitude_e7;
  int16_t altitude_m;
  int16_t rate_of_climb_mps_x10;
  uint16_t speed_kph_x10;
  uint16_t heading_deg_x10;
  uint8_t battery_percent;
  uint32_t timestamp_ms;
};

struct aircraft_telemetry_runtime
{
  uint32_t update_count;
};

static inline float aircraft_telemetry_degrees_to_radians(float degrees)
{
  return degrees * 0.017453292519943295f;
}

static inline float aircraft_telemetry_normalize_heading(float heading_deg)
{
  while (heading_deg < 0.0f)
  {
    heading_deg += 360.0f;
  }

  while (heading_deg >= 360.0f)
  {
    heading_deg -= 360.0f;
  }

  return heading_deg;
}

static inline float aircraft_telemetry_target_speed_kph(float altitude_m)
{
  if (altitude_m < 1000.0f)
  {
    return 280.0f;
  }
  if (altitude_m < 3000.0f)
  {
    return 350.0f;
  }
  if (altitude_m < 6000.0f)
  {
    return 450.0f;
  }
  if (altitude_m < 9000.0f)
  {
    return 600.0f;
  }

  return 850.0f;
}

static inline float aircraft_telemetry_rate_of_climb_mps(float altitude_m)
{
  if (altitude_m < 1000.0f)
  {
    return 12.0f;
  }
  if (altitude_m < 3000.0f)
  {
    return 10.0f;
  }
  if (altitude_m < 6000.0f)
  {
    return 7.0f;
  }
  if (altitude_m < 9000.0f)
  {
    return 4.0f;
  }
  if (altitude_m < 11000.0f)
  {
    return 1.5f;
  }

  return 0.0f;
}

static inline float aircraft_telemetry_haversine_distance_m(int32_t lat1_e7, int32_t lon1_e7,
                                                            int32_t lat2_e7, int32_t lon2_e7)
{
  float lat1 = aircraft_telemetry_degrees_to_radians(lat1_e7 / 1e7f);
  float lat2 = aircraft_telemetry_degrees_to_radians(lat2_e7 / 1e7f);
  float dlat = lat2 - lat1;
  float dlon = aircraft_telemetry_degrees_to_radians((lon2_e7 - lon1_e7) / 1e7f);
  float a = sinf(dlat / 2.0f) * sinf(dlat / 2.0f) +
            cosf(lat1) * cosf(lat2) * sinf(dlon / 2.0f) * sinf(dlon / 2.0f);
  float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
  return 6371000.0f * c;
}

static inline float aircraft_telemetry_bearing_deg(int32_t lat1_e7, int32_t lon1_e7,
                                                   int32_t lat2_e7, int32_t lon2_e7)
{
  float lat1 = aircraft_telemetry_degrees_to_radians(lat1_e7 / 1e7f);
  float lat2 = aircraft_telemetry_degrees_to_radians(lat2_e7 / 1e7f);
  float dlon = aircraft_telemetry_degrees_to_radians((lon2_e7 - lon1_e7) / 1e7f);

  float y = sinf(dlon) * cosf(lat2);
  float x = cosf(lat1) * sinf(lat2) -
            sinf(lat1) * cosf(lat2) * cosf(dlon);
  float bearing = atan2f(y, x) * 57.29577951308232f;

  return aircraft_telemetry_normalize_heading(bearing);
}

static inline void aircraft_telemetry_init(struct aircraft_telemetry_state *state,
                                           struct aircraft_telemetry_runtime *runtime)
{
  *state = (struct aircraft_telemetry_state){
      .aircraft_id = AIRCRAFT_TELEMETRY_AIRCRAFT_ID,
      .latitude_e7 = AIRCRAFT_TELEMETRY_START_LAT_E7,
      .longitude_e7 = AIRCRAFT_TELEMETRY_START_LON_E7,
      .altitude_m = 0,
      .rate_of_climb_mps_x10 = 0,
      .speed_kph_x10 = AIRCRAFT_TELEMETRY_INITIAL_SPEED_KPH_X10,
      .heading_deg_x10 = (uint16_t)(aircraft_telemetry_bearing_deg(AIRCRAFT_TELEMETRY_START_LAT_E7,
                                                                   AIRCRAFT_TELEMETRY_START_LON_E7,
                                                                   AIRCRAFT_TELEMETRY_DEST_LAT_E7,
                                                                   AIRCRAFT_TELEMETRY_DEST_LON_E7) *
                                    10.0f),
      .battery_percent = 100,
      .timestamp_ms = 0,
  };

  *runtime = (struct aircraft_telemetry_runtime){
      .update_count = 0U,
  };
}

static inline void aircraft_telemetry_update(struct aircraft_telemetry_state *state,
                                             struct aircraft_telemetry_runtime *runtime,
                                             uint32_t timestamp_ms)
{
  runtime->update_count++;

  float altitude_m = (float)state->altitude_m;
  float target_speed_kph = aircraft_telemetry_target_speed_kph(altitude_m);
  float current_speed_kph = state->speed_kph_x10 / 10.0f;
  float speed_delta_kph = target_speed_kph - current_speed_kph;
  float max_speed_change_kph = 6.0f;

  if (speed_delta_kph > max_speed_change_kph)
  {
    speed_delta_kph = max_speed_change_kph;
  }
  if (speed_delta_kph < -max_speed_change_kph)
  {
    speed_delta_kph = -max_speed_change_kph;
  }

  current_speed_kph += speed_delta_kph;
  if (current_speed_kph < 260.0f)
  {
    current_speed_kph = 260.0f;
  }
  state->speed_kph_x10 = (uint16_t)lroundf(current_speed_kph * 10.0f);

  float roc_mps = aircraft_telemetry_rate_of_climb_mps(altitude_m);
  altitude_m += roc_mps;
  if (altitude_m > 11000.0f)
  {
    altitude_m = 11000.0f;
  }
  state->altitude_m = (int16_t)lroundf(altitude_m);
  state->rate_of_climb_mps_x10 = (int16_t)lroundf(roc_mps * 10.0f);

  float dist_to_dest_m = aircraft_telemetry_haversine_distance_m(
      state->latitude_e7, state->longitude_e7,
      AIRCRAFT_TELEMETRY_DEST_LAT_E7, AIRCRAFT_TELEMETRY_DEST_LON_E7);
  if (dist_to_dest_m <= AIRCRAFT_TELEMETRY_ARRIVAL_RADIUS_M)
  {
    state->timestamp_ms = timestamp_ms;
    return;
  }
  state->heading_deg_x10 = (uint16_t)lroundf(aircraft_telemetry_bearing_deg(
                                                 state->latitude_e7, state->longitude_e7,
                                                 AIRCRAFT_TELEMETRY_DEST_LAT_E7, AIRCRAFT_TELEMETRY_DEST_LON_E7) *
                                             10.0f);

  float speed_mps = current_speed_kph / 3.6f;
  float distance_m = speed_mps;
  float heading_deg = state->heading_deg_x10 / 10.0f;
  float heading_rad = aircraft_telemetry_degrees_to_radians(heading_deg);
  float earth_radius_m = 6371000.0f;
  float lat_rad = aircraft_telemetry_degrees_to_radians(state->latitude_e7 / 1e7f);
  float lon_rad = aircraft_telemetry_degrees_to_radians(state->longitude_e7 / 1e7f);
  float cos_lat = cosf(lat_rad);
  if (cos_lat < 0.000001f)
  {
    cos_lat = 0.000001f;
  }
  if (cos_lat > -0.000001f && cos_lat < 0.000001f)
  {
    cos_lat = 0.000001f;
  }

  float next_lat_rad = lat_rad + (distance_m / earth_radius_m) * cosf(heading_rad);
  float next_lon_rad = lon_rad + (distance_m / (earth_radius_m * cos_lat)) * sinf(heading_rad);
  state->latitude_e7 = (int32_t)lroundf(next_lat_rad * 57.29577951308232f * 1e7f);
  state->longitude_e7 = (int32_t)lroundf(next_lon_rad * 57.29577951308232f * 1e7f);

  state->timestamp_ms = timestamp_ms;
}

#endif