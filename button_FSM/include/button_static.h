/**
 * @file    button_static.h
 * @author  datngyB
 * @brief   Basic button press actions include pressing,long pressing, and holding.
 * @version 0.1.0
 * @date    2026-02-11
 * * @copyright Copyright (c) 2026
 */

#ifndef BUTTON_STATIC_H
#define BUTTON_STATIC_H

#include <stdint.h>
#include <stdbool.h>

#define BUTTON_DEBOUNCE_TICKS       50   
#define BUTTON_LONG_PRESS_TICKS     1000 
#define BUTTON_HOLD_TICKS           50 
#define BUTTON_SUPER_LONG_PRESS_TICKS   5000

/* Defines the electrical */
typedef enum {
    BUTTON_ACTIVE_LOW = 0,  /*Pull up */
    BUTTON_ACTIVE_HIGH = 1, /* Pull down */
    BUTTON_ACTIVE_MAX   /* parameter validation */

} button_active_level_t;

/* Button event, please see comments for each event */
typedef enum {
    BUTTON_EVENT_NONE = 0,         /* NO EVENT */
    BUTTON_EVENT_PRESSED,          /* BUTTON IS PRESSED LESS THAN REPEAT_TICKS AND MORE THAN DEBOUNCE_TICKS */
    BUTTON_EVENT_RELEASED,         /* BUTTON IS UNPRESSED */
    BUTTON_EVENT_LONG_PRESSED,     /* BUTTON IS PRESSED MORE THAN LONG_PRESS_TICKS */
    BUTTON_EVENT_HOLD,           /* BUTTON IS PRESSED MORE THAN LONG_PRESS_TICKS + HOLD_TICKS */
    BUTTON_EVENT_SUPER_LONG_PRESSED, /* BUTTON IS PRESSED MORE THAN SUPER_LONG_PRESS_TICKS */
    BUTTON_EVENT_MAX               /* parameter validation. */
} button_event_t;

/* Physical state of the input general button*/
typedef enum {
    STATE_IDLE,
    STATE_DEBOUNCE,
    STATE_PRESSED,
    STATE_LONG_PRESSED
} button_state_t;


    /* Configuration for a single stage in a multi-stage long press sequence.
 * This structure is typically stored in Flash to save RAM */
typedef struct {
    uint32_t threshold;     // Time in ticks to trigger
    button_event_t event;   // Event to dispatch
} button_stage_config_t;

/* 2. Multi-stage manager inside button_t */
// this place keeping array and variable runtime (RAM)
typedef struct {
    const button_stage_config_t *configs; // Pointer to array in flash (Flash)
    bool *latches;                       // Pointer to RAM 
    uint8_t count;                       // Number of stages in this button
} button_stage_manager_t;


/* Hardware API */
typedef void (*button_callback_fn)(button_event_t event, void* context);
typedef bool (*button_read_gpio_fn)(uint32_t pin_mask);
typedef uint32_t (*get_tick_fn)(void);

typedef struct {
    /* Timing tracking */
    uint32_t last_change_tick;      /**< Timestamp of the last state transition or hold pulse */
    uint32_t press_start_tick;      /**< Absolute timestamp when the button was first validated as pressed */
    uint32_t last_hold_tick;       /**< Timestamp of the last dispatched HOLD event for repeat logic */

    /* Hardware configuration */
    uint32_t gpio_num;                   /**< Physical GPIO identifier assigned to this button instance */
    button_active_level_t active_level; /**< Electrical logic level representing the 'Pressed' state */
    
    /* State Machine internal variables */
    button_state_t last_state;      /**< Current internal state of the Finite State Machine (FSM) */
    button_event_t last_event;      /**< The most recently dispatched event to the application layer */
    bool is_long_pressed_triggered; /**< One-time latch flag to prevent multiple Long Press triggers per cycle */
    
    /* Application Abstraction Layer */
    void* context;                  /**< Pointer to user-defined data passed back via callback (for reentrancy) */
    button_callback_fn callback;    /**< Application-level function pointer for asynchronous event notification */
    button_read_gpio_fn read_pin_func; /**< Function pointer to the Low-Level Driver (LLD) GPIO read routine */
    get_tick_fn get_tick_func;        /**< Function pointer to the system tick retrieval routine */

    /* Multi-stage Long Press Support */
    button_stage_manager_t stages; /**< Multi-stage long press manager */
} button_t;

typedef enum {
    BUTTON_OK = 0,   //
    BUTTON_ERR_INVALID_ARG,   // NULL pointer or invalid GPIO/ID
    BUTTON_ERR_HW_FAIL,     // Hardware error
    BUTTON_ERR_INVALID_STAGES,   // Initialization error
    BUTTON_ERR_NOT_INIT,   // Not initialized
    BUTTON_ERR_UNKNOWN,  // Unknown error
} button_error_t;

// API 
button_error_t Button_Init(button_t* button, uint32_t gpio_num, button_active_level_t level, button_read_gpio_fn read_fn, get_tick_fn tick_fn);
button_error_t Button_ConfigStages(button_t* button, const button_stage_config_t* configs, bool* latches, uint8_t count);
button_error_t Button_Update(button_t* button);   
button_error_t Button_RegisterHandler(button_t* button, button_callback_fn callback, void* context);
button_error_t Button_UnregisterHandler(button_t* button);
button_error_t Button_Deinit(button_t* button);

#endif // BUTTON_STATIC_H