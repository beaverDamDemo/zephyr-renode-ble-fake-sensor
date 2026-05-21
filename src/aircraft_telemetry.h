#ifndef AIRCRAFT_TELEMETRY_H
#define AIRCRAFT_TELEMETRY_H

#include <math.h>
#include <stdint.h>

#define AIRCRAFT_TELEMETRY_START_LAT_E7 81005000
#define AIRCRAFT_TELEMETRY_START_LON_E7 989841639
#define AIRCRAFT_TELEMETRY_DEST_LAT_E7 462237000
#define AIRCRAFT_TELEMETRY_DEST_LON_E7 144576000

#define AIRCRAFT_TELEMETRY_AIRCRAFT_ID 0xA1B2C3D4
#define AIRCRAFT_TELEMETRY_INITIAL_SPEED_KPH_X10 2900U

struct aircraft_telemetry_state
{
  uint32_t aircraft_id;
  int32_t latitude_e7;
  int32_t longitude_e7;
  int16_t altitude_m;
  uint16_t speed_kph_x10;
  uint16_t heading_deg_x10;
  uint8_t battery_percent;
  uint32_t timestamp_ms;
};

struct aircraft_telemetry_runtime
{
  float progress;
  uint32_t update_count;
  float total_distance_m;
};

typedef uint32_t (*aircraft_random32_fn)(void *user_data);

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

static inline float aircraft_telemetry_distance_m(int32_t lat1_e7, int32_t lon1_e7,
                                                  int32_t lat2_e7, int32_t lon2_e7)
{
  const float earth_radius_m = 6371000.0f;
  float lat1 = aircraft_telemetry_degrees_to_radians(lat1_e7 / 1e7f);
  float lat2 = aircraft_telemetry_degrees_to_radians(lat2_e7 / 1e7f);
  float dlat = aircraft_telemetry_degrees_to_radians((lat2_e7 - lat1_e7) / 1e7f);
  float dlon = aircraft_telemetry_degrees_to_radians((lon2_e7 - lon1_e7) / 1e7f);
  float sin_dlat = sinf(dlat * 0.5f);
  float sin_dlon = sinf(dlon * 0.5f);
  float a = sin_dlat * sin_dlat + cosf(lat1) * cosf(lat2) * sin_dlon * sin_dlon;
  float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

  return earth_radius_m * c;
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

static inline void aircraft_telemetry_set_position(struct aircraft_telemetry_state *state,
                                                   int32_t latitude_e7,
                                                   int32_t longitude_e7)
{
  state->latitude_e7 = latitude_e7;
  state->longitude_e7 = longitude_e7;
}

static inline void aircraft_telemetry_init(struct aircraft_telemetry_state *state,
                                           struct aircraft_telemetry_runtime *runtime)
{
  *state = (struct aircraft_telemetry_state){
      .aircraft_id = AIRCRAFT_TELEMETRY_AIRCRAFT_ID,
      .latitude_e7 = AIRCRAFT_TELEMETRY_START_LAT_E7,
      .longitude_e7 = AIRCRAFT_TELEMETRY_START_LON_E7,
      .altitude_m = 0,
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
      .progress = 0.0f,
      .update_count = 0U,
      .total_distance_m = aircraft_telemetry_distance_m(AIRCRAFT_TELEMETRY_START_LAT_E7,
                                                        AIRCRAFT_TELEMETRY_START_LON_E7,
                                                        AIRCRAFT_TELEMETRY_DEST_LAT_E7,
                                                        AIRCRAFT_TELEMETRY_DEST_LON_E7),
  };
}

static inline uint32_t aircraft_telemetry_next_random(aircraft_random32_fn random32,
                                                      void *user_data)
{
  if (random32 == NULL)
  {
    return 0U;
  }

  return random32(user_data);
}

static inline void aircraft_telemetry_update(struct aircraft_telemetry_state *state,
                                             struct aircraft_telemetry_runtime *runtime,
                                             uint32_t timestamp_ms,
                                             aircraft_random32_fn random32,
                                             void *random_user_data)
{
  runtime->update_count++;

  float current_lat_deg = state->latitude_e7 / 1e7f;
  float current_lon_deg = state->longitude_e7 / 1e7f;
  float destination_lat_deg = AIRCRAFT_TELEMETRY_DEST_LAT_E7 / 1e7f;
  float destination_lon_deg = AIRCRAFT_TELEMETRY_DEST_LON_E7 / 1e7f;
  float distance_to_destination_m = aircraft_telemetry_distance_m(state->latitude_e7,
                                                                  state->longitude_e7,
                                                                  AIRCRAFT_TELEMETRY_DEST_LAT_E7,
                                                                  AIRCRAFT_TELEMETRY_DEST_LON_E7);
  float current_speed_kph = state->speed_kph_x10 / 10.0f;
  float travel_distance_m = current_speed_kph * (1000.0f / 3600.0f);
  float heading_deg = state->heading_deg_x10 / 10.0f;
  float heading_rad = aircraft_telemetry_degrees_to_radians(heading_deg);

  if (travel_distance_m >= distance_to_destination_m)
  {
    current_lat_deg = destination_lat_deg;
    current_lon_deg = destination_lon_deg;
    state->latitude_e7 = AIRCRAFT_TELEMETRY_DEST_LAT_E7;
    state->longitude_e7 = AIRCRAFT_TELEMETRY_DEST_LON_E7;
    runtime->progress = 1.0f;
  }
  else
  {
    float meters_per_degree_lat = 111320.0f;
    float meters_per_degree_lon = 111320.0f * cosf(aircraft_telemetry_degrees_to_radians(current_lat_deg));
    if (meters_per_degree_lon < 1.0f)
    {
      meters_per_degree_lon = 1.0f;
    }

    float delta_north_m = travel_distance_m * cosf(heading_rad);
    float delta_east_m = travel_distance_m * sinf(heading_rad);

    current_lat_deg += delta_north_m / meters_per_degree_lat;
    current_lon_deg += delta_east_m / meters_per_degree_lon;

    state->latitude_e7 = (int32_t)lroundf(current_lat_deg * 1e7f);
    state->longitude_e7 = (int32_t)lroundf(current_lon_deg * 1e7f);

    distance_to_destination_m = aircraft_telemetry_distance_m(state->latitude_e7,
                                                              state->longitude_e7,
                                                              AIRCRAFT_TELEMETRY_DEST_LAT_E7,
                                                              AIRCRAFT_TELEMETRY_DEST_LON_E7);
    if (runtime->total_distance_m > 0.0f)
    {
      runtime->progress = 1.0f - (distance_to_destination_m / runtime->total_distance_m);
    }
    if (runtime->progress < 0.0f)
    {
      runtime->progress = 0.0f;
    }
    if (runtime->progress > 1.0f)
    {
      runtime->progress = 1.0f;
    }
  }

  float progress = runtime->progress;
  float target_speed_kph;

  if (progress < 0.08f)
  {
    target_speed_kph = 290.0f;
  }
  else if (progress < 0.12f)
  {
    target_speed_kph = 300.0f;
  }
  else if (progress < 0.85f)
  {
    target_speed_kph = 880.0f;
  }
  else if (progress < 0.95f)
  {
    target_speed_kph = 350.0f;
  }
  else
  {
    target_speed_kph = 260.0f;
  }

  float max_turn_rate_deg = 8.0f;
  float desired_heading_deg = aircraft_telemetry_bearing_deg(state->latitude_e7,
                                                             state->longitude_e7,
                                                             AIRCRAFT_TELEMETRY_DEST_LAT_E7,
                                                             AIRCRAFT_TELEMETRY_DEST_LON_E7);
  float heading_delta_deg = desired_heading_deg - heading_deg;

  while (heading_delta_deg > 180.0f)
  {
    heading_delta_deg -= 360.0f;
  }

  while (heading_delta_deg < -180.0f)
  {
    heading_delta_deg += 360.0f;
  }

  if (heading_delta_deg > max_turn_rate_deg)
  {
    heading_delta_deg = max_turn_rate_deg;
  }
  if (heading_delta_deg < -max_turn_rate_deg)
  {
    heading_delta_deg = -max_turn_rate_deg;
  }

  heading_deg = aircraft_telemetry_normalize_heading(heading_deg + heading_delta_deg);
  state->heading_deg_x10 = (uint16_t)(heading_deg * 10.0f);

  float max_accel = 6.0f;
  float max_decel = 6.0f;
  float delta = target_speed_kph - current_speed_kph;

  if (delta > max_accel)
  {
    delta = max_accel;
  }
  if (delta < -max_decel)
  {
    delta = -max_decel;
  }

  current_speed_kph += delta;
  if (current_speed_kph < 260.0f)
  {
    current_speed_kph = 260.0f;
  }
  state->speed_kph_x10 = (uint16_t)(current_speed_kph * 10.0f);

  if (progress < 0.1f)
  {
    state->altitude_m = 1000 + (int16_t)(progress * 9000.0f);
  }
  else if (progress < 0.9f)
  {
    state->altitude_m = 10000 + (int16_t)(aircraft_telemetry_next_random(random32, random_user_data) % 500U);
  }
  else
  {
    float p = (progress - 0.9f) / 0.1f;
    state->altitude_m = 10000 - (int16_t)(p * 9000.0f);
  }

  if (progress < 0.1f)
  {
    state->speed_kph_x10 = 200 + (uint16_t)(progress * 6000.0f);
  }
  else if (progress < 0.9f)
  {
    state->speed_kph_x10 = 7500 + (uint16_t)(aircraft_telemetry_next_random(random32, random_user_data) % 200U);
  }
  else
  {
    float p = (progress - 0.9f) / 0.1f;
    state->speed_kph_x10 = 7500 - (uint16_t)(p * 6000.0f);
  }

  if (state->speed_kph_x10 < 2600U)
  {
    state->speed_kph_x10 = 2600U;
  }

  float dLon = (AIRCRAFT_TELEMETRY_DEST_LON_E7 - AIRCRAFT_TELEMETRY_START_LON_E7) / 1e7f;
  float y = sinf(dLon) * cosf(AIRCRAFT_TELEMETRY_DEST_LAT_E7 / 1e7f);
  float x = cosf(AIRCRAFT_TELEMETRY_START_LAT_E7 / 1e7f) * sinf(AIRCRAFT_TELEMETRY_DEST_LAT_E7 / 1e7f) -
            sinf(AIRCRAFT_TELEMETRY_START_LAT_E7 / 1e7f) * cosf(AIRCRAFT_TELEMETRY_DEST_LAT_E7 / 1e7f) * cosf(dLon);
  float bearing = atan2f(y, x) * 180.0f / 3.1415926f;
  if (bearing < 0.0f)
  {
    bearing += 360.0f;
  }

  state->heading_deg_x10 = (uint16_t)(bearing * 10.0f);

  if (state->battery_percent > 0U && (runtime->update_count % 30U) == 0U)
  {
    state->battery_percent--;
  }

  state->timestamp_ms = timestamp_ms;
}

#endif