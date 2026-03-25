#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct roberta_axis_client roberta_axis_client_t;

typedef struct roberta_axis_snapshot_t {
    uint32_t axis_id;
    uint32_t lifecycle;
    uint32_t status_word;
    uint32_t fault_word;
    uint32_t capability_word;
    uint32_t last_error_code;
    int32_t current_position_steps;
    int32_t target_position_steps;
    int32_t current_velocity_steps_per_second;
    int32_t step_count;
    int32_t min_steps;
    int32_t max_steps;
    int32_t velocity_steps_per_second;
    int32_t accel_steps_per_second_squared;
    int32_t home_velocity_steps_per_second;
    double current_position_deg;
    double target_position_deg;
    uint8_t outputs_enabled;
    uint8_t motor_enabled;
    uint8_t homed;
    uint8_t moving;
    uint8_t homing;
    uint8_t limit_home;
    uint8_t limit_a;
    uint8_t limit_b;
} roberta_axis_snapshot_t;

typedef struct roberta_axis_register_value_t {
    uint16_t address;
    uint32_t value;
} roberta_axis_register_value_t;

roberta_axis_client_t* roberta_axis_client_new(const char* device_path, int baud_rate);
void roberta_axis_client_free(roberta_axis_client_t* client);

uint8_t roberta_axis_client_connect(
    roberta_axis_client_t* client,
    char* error_buffer,
    size_t error_buffer_len);
void roberta_axis_client_disconnect(roberta_axis_client_t* client);
uint8_t roberta_axis_client_is_connected(const roberta_axis_client_t* client);

uint8_t roberta_axis_client_snapshot(
    roberta_axis_client_t* client,
    roberta_axis_snapshot_t* out_snapshot,
    char* error_buffer,
    size_t error_buffer_len);

uint8_t roberta_axis_client_enable_drive(
    roberta_axis_client_t* client,
    char* error_buffer,
    size_t error_buffer_len);
uint8_t roberta_axis_client_disable_drive(
    roberta_axis_client_t* client,
    char* error_buffer,
    size_t error_buffer_len);
uint8_t roberta_axis_client_jog_relative_degrees(
    roberta_axis_client_t* client,
    double delta_degrees,
    char* error_buffer,
    size_t error_buffer_len);
uint8_t roberta_axis_client_clear_faults(
    roberta_axis_client_t* client,
    char* error_buffer,
    size_t error_buffer_len);
uint8_t roberta_axis_client_set_zero(
    roberta_axis_client_t* client,
    char* error_buffer,
    size_t error_buffer_len);

size_t roberta_axis_register_count(void);
uint16_t roberta_axis_register_at(size_t index);
const char* roberta_axis_register_name(uint16_t reg_address);
uint8_t roberta_axis_register_is_writable(uint16_t reg_address);

uint8_t roberta_axis_client_read_all_registers(
    roberta_axis_client_t* client,
    roberta_axis_register_value_t* out_values,
    size_t capacity,
    size_t* out_written,
    char* error_buffer,
    size_t error_buffer_len);

#ifdef __cplusplus
}
#endif
