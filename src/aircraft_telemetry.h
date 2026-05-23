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

/* runtime holds internal floats and RNG state for smoothing/noise */
struct aircraft_telemetry_runtime
{
  uint32_t update_count;
  uint32_t rng_seed;
  uint32_t last_timestamp_ms;
  float prev_dist_to_dest_m;

  /* internal smoothed floats (not part of public state) */
  float current_altitude_m;
  float current_speed_kph;
  float current_roc_mps;
  float current_heading_deg;

  /* low-pass noise states */
  float speed_noise;
  float roc_noise;
  float heading_noise;
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

/* A320neo / 737 MAX8 climb profile.
   Speeds are approximate ground-speed proxies (no wind model).

    Phase 1  sea level  -> FL100 (~3 048 m)   : initial climb
      Speed : ~300 kph, accelerates quickly toward ~420 kph / 250 KIAS band
      ROC   : 14 m/s (~2 750 ft/min) tapering smoothly to 11 m/s

   Phase 2  FL100      -> FL240 (~7 315 m)   : mid-climb
     Speed : ~420 kph accelerating to ~556 kph (300 kt IAS)
     ROC   : 11 m/s tapering to 8 m/s

   Phase 3  FL240      -> FL410 (~12 497 m)  : upper / Mach climb
     Speed : ~556 kph ramping up to ~840 kph (Mach 0.78 TAS at cruise)
     ROC   : 8 m/s tapering to 3 m/s near the service ceiling              */
static inline void climb_profile_targets(float altitude_m, float *target_speed_kph, float *target_roc_mps)
{
  const float ft_to_m  = 0.3048f;
  const float FL100_m  = 10000.0f * ft_to_m;  /* ~3 048 m */
  const float FL240_m  = 24000.0f * ft_to_m;  /* ~7 315 m */
  const float FL410_m  = 41000.0f * ft_to_m;  /* ~12 497 m – A320neo/737 MAX8 service ceiling */

  if (altitude_m < FL100_m)
  {
    /* Phase 1: initial climb below FL100 */
    float t = altitude_m / FL100_m;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    *target_speed_kph = 300.0f + (420.0f - 300.0f) * powf(t, 0.55f); /* faster early accel */
    *target_roc_mps   = 14.0f  - (14.0f  - 11.0f) * powf(t, 0.80f);  /* smooth ROC taper   */
  }
  else if (altitude_m < FL240_m)
  {
    /* Phase 2: mid-climb FL100 to FL240 */
    float t = (altitude_m - FL100_m) / (FL240_m - FL100_m);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    *target_speed_kph = 420.0f + t * (556.0f - 420.0f); /* 420 -> 556 kph */
    *target_roc_mps   = 11.0f  + t * (8.0f   - 11.0f);  /* 11  ->  8 m/s  */
  }
  else
  {
    /* Phase 3: upper / Mach climb FL240 to FL410 */
    float span = FL410_m - FL240_m;
    float t = (altitude_m - FL240_m) / (span > 0.0f ? span : 1.0f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    *target_speed_kph = 556.0f + t * (840.0f - 556.0f); /* 556 -> 840 kph (Mach 0.78 TAS) */
    *target_roc_mps   = 8.0f   + t * (3.0f   - 8.0f);   /*   8 ->   3 m/s               */
  }
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

/* RNG helpers */
static inline float aircraft_telemetry_rand_unit(uint32_t *seed)
{
  *seed = (*seed * 1664525u + 1013904223u);
  return (float)(*seed & 0x00FFFFFFu) / 16777216.0f; // 0..1
}

static inline float aircraft_telemetry_rand_range(uint32_t *seed, float min, float max)
{
  return min + (max - min) * aircraft_telemetry_rand_unit(seed);
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
      .heading_deg_x10 = (uint16_t)(aircraft_telemetry_bearing_deg(
                                        AIRCRAFT_TELEMETRY_START_LAT_E7,
                                        AIRCRAFT_TELEMETRY_START_LON_E7,
                                        AIRCRAFT_TELEMETRY_DEST_LAT_E7,
                                        AIRCRAFT_TELEMETRY_DEST_LON_E7) *
                                    10.0f),
      .battery_percent = 100,
      .timestamp_ms = 0,
  };

  /* initialize runtime floats from state */
  *runtime = (struct aircraft_telemetry_runtime){
      .update_count = 0U,
      .rng_seed = 0x12345678u,
      .last_timestamp_ms = 0U,
      .prev_dist_to_dest_m = 1e9f,
      .current_altitude_m = 0.0f,
      .current_speed_kph = (float)AIRCRAFT_TELEMETRY_INITIAL_SPEED_KPH_X10 / 10.0f,
      .current_roc_mps = 0.0f,
      .current_heading_deg = (float)state->heading_deg_x10 / 10.0f,
      .speed_noise = 0.0f,
      .roc_noise = 0.0f,
      .heading_noise = 0.0f,
  };
}

/* Main update: dt-aware, smoothed acceleration, ROC affects acceleration,
   low-pass noise, safe arrival check. */
static inline void aircraft_telemetry_update(struct aircraft_telemetry_state *state,
                                             struct aircraft_telemetry_runtime *runtime,
                                             uint32_t timestamp_ms)
{
  runtime->update_count++;

  /* 0) dt (seconds), clamped to reasonable range */
  float dt;
  if (runtime->last_timestamp_ms == 0U)
  {
    dt = 1.0f; /* first update assume 1s */
  }
  else
  {
    uint32_t delta_ms = timestamp_ms - runtime->last_timestamp_ms;
    dt = (float)delta_ms / 1000.0f;
    if (dt < 0.02f) dt = 0.02f; /* 20 ms */
    if (dt > 1.0f) dt = 1.0f;
  }

  float altitude_m = runtime->current_altitude_m;

  /* 1) compute altitude-dependent targets */
  float target_speed_kph;
  float target_roc_mps;
  climb_profile_targets(altitude_m, &target_speed_kph, &target_roc_mps);

  /* 2) compute acceleration capability and ROC penalty
     - when ROC is high (climbing hard) acceleration capability is reduced
     - penalty is proportional to fraction of max ROC for that phase */
  const float max_roc_for_penalty = 12.0f; /* m/s, reference */
  float roc_fraction = fabsf(runtime->current_roc_mps) / max_roc_for_penalty;
  if (roc_fraction > 1.0f) roc_fraction = 1.0f;

  /* base acceleration capability (kph per second) */
  const float base_accel_kph_per_s = 20.0f;
  /* reduce accel when climbing hard (up to 60% reduction) */
  const float max_accel_reduction = 0.35f;
  float accel_reduction = roc_fraction * max_accel_reduction;
  float effective_accel_kph_per_s = base_accel_kph_per_s * (1.0f - accel_reduction);
  float max_speed_change_kph = effective_accel_kph_per_s * dt;

  /* 3) update speed: approach target but limited by effective accel */
  float current_speed_kph = runtime->current_speed_kph;
  float speed_delta = target_speed_kph - current_speed_kph;
  if (speed_delta > max_speed_change_kph) speed_delta = max_speed_change_kph;
  if (speed_delta < -max_speed_change_kph) speed_delta = -max_speed_change_kph;
  current_speed_kph += speed_delta;

  /* small smooth noise on speed (±2 kph) */
  float speed_noise_target = aircraft_telemetry_rand_range(&runtime->rng_seed, -3.0f, 3.0f);
  runtime->speed_noise = 0.94f * runtime->speed_noise + 0.06f * speed_noise_target;
  current_speed_kph += runtime->speed_noise;

  /* enforce reasonable lower bound (but allow lower than 260 for early phases if desired) */
  if (current_speed_kph < 100.0f) current_speed_kph = 100.0f;

  /* store back to runtime and public state */
  runtime->current_speed_kph = current_speed_kph;
  state->speed_kph_x10 = (uint16_t)lroundf(current_speed_kph * 10.0f);

  /* 4) update ROC: smoothly approach target ROC, with noise */
  float roc_mps = runtime->current_roc_mps;
  const float max_roc_change_mps_per_s = 6.0f;
  float max_roc_change_mps = max_roc_change_mps_per_s * dt;
  float roc_delta = target_roc_mps - roc_mps;
  if (roc_delta > max_roc_change_mps) roc_delta = max_roc_change_mps;
  if (roc_delta < -max_roc_change_mps) roc_delta = -max_roc_change_mps;
  roc_mps += roc_delta;

  /* low-pass noise on ROC (±0.6 m/s) */
  float roc_noise_target = aircraft_telemetry_rand_range(&runtime->rng_seed, -1.0f, 1.0f);
  runtime->roc_noise = 0.94f * runtime->roc_noise + 0.06f * roc_noise_target;
  roc_mps += runtime->roc_noise;

  /* update altitude using dt */
  altitude_m += roc_mps * dt;
  if (altitude_m > 12497.0f) altitude_m = 12497.0f; /* FL410 – A320neo / 737 MAX8 service ceiling */
  if (altitude_m < 0.0f) altitude_m = 0.0f;

  runtime->current_altitude_m = altitude_m;
  runtime->current_roc_mps = roc_mps;
  state->altitude_m = (int16_t)lroundf(altitude_m);
  state->rate_of_climb_mps_x10 = (int16_t)lroundf(roc_mps * 10.0f);

  /* 5) distance to destination and safe arrival check */
  float dist_to_dest_m = aircraft_telemetry_haversine_distance_m(
      state->latitude_e7, state->longitude_e7,
      AIRCRAFT_TELEMETRY_DEST_LAT_E7, AIRCRAFT_TELEMETRY_DEST_LON_E7);

  if (dist_to_dest_m <= AIRCRAFT_TELEMETRY_ARRIVAL_RADIUS_M &&
      dist_to_dest_m < runtime->prev_dist_to_dest_m)
  {
    state->timestamp_ms = timestamp_ms;
    runtime->prev_dist_to_dest_m = dist_to_dest_m;
    runtime->last_timestamp_ms = timestamp_ms;
    return;
  }
  runtime->prev_dist_to_dest_m = dist_to_dest_m;

  /* 6) heading: base bearing + smoothed wobble */
  float base_heading_deg = aircraft_telemetry_bearing_deg(
      state->latitude_e7, state->longitude_e7,
      AIRCRAFT_TELEMETRY_DEST_LAT_E7, AIRCRAFT_TELEMETRY_DEST_LON_E7);

  float heading_noise_target = aircraft_telemetry_rand_range(&runtime->rng_seed, -1.2f, 1.2f);
  runtime->heading_noise = 0.94f * runtime->heading_noise + 0.06f * heading_noise_target;
  float heading_deg = aircraft_telemetry_normalize_heading(base_heading_deg + runtime->heading_noise);

  runtime->current_heading_deg = heading_deg;
  state->heading_deg_x10 = (uint16_t)lroundf(heading_deg * 10.0f);

  /* 7) move along heading using dt */
  float speed_mps = current_speed_kph / 3.6f;
  float distance_m = speed_mps * dt;

  float heading_rad = aircraft_telemetry_degrees_to_radians(heading_deg);
  float earth_radius_m = 6371000.0f;
  float lat_rad = aircraft_telemetry_degrees_to_radians(state->latitude_e7 / 1e7f);
  float lon_rad = aircraft_telemetry_degrees_to_radians(state->longitude_e7 / 1e7f);

  float cos_lat = cosf(lat_rad);
  if (cos_lat < 0.000001f && cos_lat > -0.000001f)
    cos_lat = 0.000001f;

  float next_lat_rad = lat_rad + (distance_m / earth_radius_m) * cosf(heading_rad);
  float next_lon_rad = lon_rad + (distance_m / (earth_radius_m * cos_lat)) * sinf(heading_rad);

  state->latitude_e7 = (int32_t)lroundf(next_lat_rad * 57.29577951308232f * 1e7f);
  state->longitude_e7 = (int32_t)lroundf(next_lon_rad * 57.29577951308232f * 1e7f);

  state->timestamp_ms = timestamp_ms;
  runtime->last_timestamp_ms = timestamp_ms;
}

#endif
