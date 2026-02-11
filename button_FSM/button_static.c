#include    <stdbool.h>
#include    <stdint.h>
#include    "button_static.h"

static void handle_state_idle(button_t* button, bool is_pressed, uint32_t current_tick);
static void handle_state_debounce(button_t* button, bool is_pressed, uint32_t current_tick);
static void handle_state_pressed(button_t* button, bool is_pressed, uint32_t current_tick);
static void handle_state_long(button_t* button, bool is_pressed, uint32_t current_tick);
static bool validate_stages(const button_stage_config_t *cfg, uint8_t count);


button_error_t Button_Init(button_t* button, uint32_t gpio_num, button_active_level_t level, 
                button_read_gpio_fn read_fn, get_tick_fn tick_fn) {

    if (!button || !read_fn || !tick_fn || level >= BUTTON_ACTIVE_MAX) return BUTTON_ERR_INVALID_ARG;
    
    uint32_t now = tick_fn();
                    
    *button = (button_t){
        .gpio_num = gpio_num,
        .active_level = level,
        .last_state = STATE_IDLE,
        .read_pin_func = read_fn,    
        .get_tick_func = tick_fn,    
        .last_change_tick = now,
        .press_start_tick = now,
        .last_hold_tick = now,
        .callback = NULL,        
        .context = NULL,
        .stages = { .configs = NULL, .latches = NULL, .count = 0 }
    };

    return BUTTON_OK; 
}

button_error_t Button_ConfigStages(button_t* button, const button_stage_config_t* configs, bool* latches, uint8_t count) {
    if (!button || !configs || !latches || count == 0) return BUTTON_ERR_INVALID_ARG;
    if (!validate_stages(configs, count)) return BUTTON_ERR_INVALID_STAGES;
    button->stages.configs = configs;
    button->stages.latches = latches;
    button->stages.count = count;
    return BUTTON_OK;
}

button_error_t Button_Update(button_t* button) {
    if (!button || !button->read_pin_func || !button->get_tick_func) return BUTTON_ERR_INVALID_ARG;

    bool pin_state = (bool)button->read_pin_func(button->gpio_num);
    bool is_pressed = (button->active_level == BUTTON_ACTIVE_LOW) ? (pin_state == 0) : (pin_state != 0);
    uint32_t current_tick = button->get_tick_func();    

    switch (button->last_state) {
        case STATE_IDLE:
             handle_state_idle(button, is_pressed, current_tick);    
             break;
        case STATE_DEBOUNCE: 
            handle_state_debounce(button, is_pressed, current_tick); 
            break;
        case STATE_PRESSED:  
            handle_state_pressed(button, is_pressed, current_tick);  
            break;
        case STATE_LONG_PRESSED:     
            handle_state_long(button, is_pressed, current_tick);     
            break;
        default:             
            button->last_state = STATE_IDLE;                
            break;
    }
    return BUTTON_OK;
}

static void handle_state_idle(button_t* button, bool is_pressed, uint32_t current_tick) {
    if (is_pressed) {
        button->last_state = STATE_DEBOUNCE;
        button->last_change_tick = current_tick;
    }
}

static void handle_state_debounce(button_t* button, bool is_pressed, uint32_t current_tick) {
    uint32_t diff = current_tick - button->last_change_tick;
    if (diff >= BUTTON_DEBOUNCE_TICKS) {
        if (is_pressed) {
            button->last_state = STATE_PRESSED;
            button->last_change_tick = current_tick;
            if (button->callback) {
                button->callback(BUTTON_EVENT_PRESSED, button->context);
            }
        } else {
            button->last_state = STATE_IDLE;
        }
    }
}

static void handle_state_pressed(button_t* button, bool is_pressed, uint32_t current_tick) {
    uint32_t diff = current_tick - button->last_change_tick;

    if (!is_pressed) {
        button->last_state = STATE_IDLE;
        if (button->callback != NULL) {
            button->callback(BUTTON_EVENT_RELEASED, button->context);
        }
    } 
    else if (diff >= BUTTON_LONG_PRESS_TICKS) {
        button->last_state = STATE_LONG_PRESSED;
        button->last_change_tick = current_tick;
        button->press_start_tick = current_tick;
        button->last_hold_tick   = current_tick;

        if (button->callback != NULL) {
            button->callback(BUTTON_EVENT_LONG_PRESSED, button->context);
        }
    }
    else {
        
    }
}

static void handle_state_long(button_t* button, bool is_pressed, uint32_t current_tick) {

    if (!is_pressed) {
        button->last_state = STATE_IDLE;
        /* Clear RAM reset the cycle*/
        if (button->stages.latches != NULL) {
            for (uint8_t i = 0; i < button->stages.count; i++) {
                button->stages.latches[i] = false;
            }
        }
  
        if (button->callback) {
            button->callback(BUTTON_EVENT_RELEASED, button->context);
        }
        return;
    }



    uint32_t total_pressed_time = current_tick - button->press_start_tick;
    if(button->stages.configs != NULL && button->stages.latches != NULL) {
        for(uint8_t i = 0; i < button->stages.count; i++) {
            if (total_pressed_time >= button->stages.configs[i].threshold && !button->stages.latches[i]) {
                button->stages.latches[i] = true; 
                if (button->callback) button->callback(button->stages.configs[i].event, button->context);
            }
        }
    }

     if (total_pressed_time >= BUTTON_LONG_PRESS_TICKS) {
        if ((current_tick - button->last_hold_tick) >= BUTTON_HOLD_TICKS) {
            button->last_hold_tick = current_tick; // Cập nhật mốc mới
            if (button->callback) {
                button->callback(BUTTON_EVENT_HOLD, button->context);
            }
        }    
    }
}

button_error_t Button_Deinit(button_t* button) {
    if (!button) return BUTTON_ERR_INVALID_ARG;

    *button = (button_t){0};
    return BUTTON_OK;
}

button_error_t Button_RegisterHandler(button_t* button, button_callback_fn callback, void* context)
{
    if (!button) return BUTTON_ERR_INVALID_ARG;

    button->callback = callback;
    button->context = context;
    return BUTTON_OK;
}

button_error_t Button_UnregisterHandler(button_t* button){
    if (!button) return BUTTON_ERR_INVALID_ARG;

    button->callback = NULL;
    button->context = NULL;
    return BUTTON_OK;
}

static bool validate_stages(const button_stage_config_t *cfg, uint8_t count) {
    if (!cfg || count == 0) return false;
    if (cfg[0].threshold == 0) return false;
    for (uint8_t i = 1; i < count; i++) {
        if (cfg[i].threshold <= cfg[i - 1].threshold) return false;
    }
    return true;
}