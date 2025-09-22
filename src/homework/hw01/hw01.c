/**
 * @file hw01.c
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief 
 * @version 0.1
 * @date 2025-08-11
 * 
 * @copyright Copyright (c) 2025
 * 
 */

 #include "hw01.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "hw01-images.h"
#include "lcd-io.h"
#include "timer.h"
#include "buzzer.h"

#if defined(HW01)

// REMOVE the fallback state constants block entirely

// Remove incorrect forward declaration that conflicted with lcd-fonts.h
// void lcd_draw_time(uint16_t row, uint16_t col, uint8_t hours, uint8_t minutes, uint16_t f_color, uint16_t b_color);

// Weak stubs so we can link without modifying other files
__attribute__((weak)) void lcd_init(void) {}
__attribute__((weak)) void lcd_backlight_on(void) {}
__attribute__((weak)) void lcd_clear_screen(uint16_t color) { (void)color; }

// Remove the old conditional buzzer stubs block and replace with unconditional weak stubs
// __attribute__((weak)) cy_rslt_t buzzer_init(float duty_cycle, uint32_t frequency) { ... }
// __attribute__((weak)) void buzzer_on(void) {}
// __attribute__((weak)) void buzzer_off(void) {}

// If you truly need stubs, build with ECE353_BUZZER_STUB defined.
// #if defined(ECE353_BUZZER_STUB)
// __attribute__((weak)) cy_rslt_t buzzer_init(float duty_cycle, uint32_t frequency) { ... }
// __attribute__((weak)) void buzzer_on(void) {}
// __attribute__((weak)) void buzzer_off(void) {}
// #endif

// Provide weak buzzer stubs so linking succeeds when the driver isn't included.
// Real implementations (non-weak) will override these at link time.
__attribute__((weak)) cy_rslt_t buzzer_init(float duty_cycle, uint32_t frequency)
{
    (void)duty_cycle; (void)frequency;
    return CY_RSLT_SUCCESS;
}
__attribute__((weak)) void buzzer_on(void) {}
__attribute__((weak)) void buzzer_off(void) {}

char APP_DESCRIPTION[] = "ECE353 F25 HW01 -- Alarm Clock";

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
extern volatile ece353_events_t ECE353_Events;

static cyhal_timer_t s_alarm_timer;
static cyhal_timer_cfg_t s_alarm_timer_cfg;

static hw01_state_t g_state;
static bool         g_state_entry;      // set true when entering a new state

static uint8_t g_hours = 0;
static uint8_t g_minutes = 0;
static uint8_t g_seconds = 0;           // increments in Running only

static uint8_t g_alarm_hours = 0;
static uint8_t g_alarm_minutes = 0;

static bool    g_alarm_enabled = false;
static bool    g_alarm_buzzing = false; // true only while in Alarm Triggered

// Blink state trackers
static bool s_blink_clock_on = true;    // Set Time blinking
static bool s_blink_speaker_on = true;  // Set Alarm blinking

// Speed-up mode: advance minutes every 100ms in Running after Set Alarm->Running
static bool g_fast_mode = false;        // ADDED

// Local timer event flags (set by 100ms ISR)
static volatile uint8_t s_ev_100ms  = 0;
static volatile uint8_t s_ev_500ms  = 0;
static volatile uint8_t s_ev_5000ms = 0;

// Helpers
static inline void switch_state(hw01_state_t new_state)
{
    g_state = new_state;
    g_state_entry = true;
}

// Minimal helper to draw time in HH:MM using the provided lcd_draw_time(minutes, seconds)
static void lcd_draw_time_hhmm(uint8_t hh, uint8_t mm, uint16_t f_color)
{
    (void)f_color; // provided API does not accept color; ignore
    // Reuse minutes/seconds API to display HH:MM
    lcd_draw_time(hh, mm);
}

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*************************************************
* Handler used to time various events. 
*************************************************/
void handler_alarm_timer(void *arg, cyhal_timer_event_t event)
{
    (void)arg;
    (void)event;

    static uint32_t ticks_100ms = 0;

    s_ev_100ms = 1;
    ticks_100ms++;

    if ((ticks_100ms % 5U) == 0U) {
        s_ev_500ms = 1;
    }
    if ((ticks_100ms % 50U) == 0U) {
        s_ev_5000ms = 1;
    }

    if (ticks_100ms >= 600000U) // wrap occasionally to avoid overflow
    {
        ticks_100ms = 0;
    }
}

/*************************************************
 * @brief Set Time State
 ************************************************/
void hw01_state_set_time(
    alarm_clock_info_t *alarm_info,
    volatile ece353_events_t *events
)
{
    (void)alarm_info;

    if (g_state_entry) {
        g_state_entry = false;
        // Icons per FSM.ST-001/002
        erase_alarm_clock();
        draw_alarm_clock(LCD_COLOR_YELLOW);
        erase_speaker();
        draw_speaker(LCD_COLOR_GRAY);
        // Reset blink
        s_blink_clock_on = true;
        // Initial time
        lcd_draw_time_hhmm(g_hours, g_minutes, LCD_COLOR_WHITE);
    }

    // Blink clock every 500ms
    if (s_ev_500ms) {
        s_ev_500ms = 0;
        s_blink_clock_on = !s_blink_clock_on;
        if (s_blink_clock_on) {
            draw_alarm_clock(LCD_COLOR_YELLOW);
        } else {
            erase_alarm_clock();
        }
    }

    // SW1 increments hours (wrap 23->00)
    if (events->sw1) {
        events->sw1 = 0;
        g_hours = (uint8_t)((g_hours + 1U) % 24U);          // FSM.ST-007/005/009
        lcd_draw_time_hhmm(g_hours, g_minutes, LCD_COLOR_WHITE);
    }

    // SW2 increments minutes (wrap 59->00)
    if (events->sw2) {
        events->sw2 = 0;
        g_minutes = (uint8_t)((g_minutes + 1U) % 60U);      // FSM.ST-008/006/009
        lcd_draw_time_hhmm(g_hours, g_minutes, LCD_COLOR_WHITE);
    }

    // SW3 -> Set Alarm
    if (events->sw3) {
        events->sw3 = 0;
        // End blink, show clock green as time "set"
        draw_alarm_clock(LCD_COLOR_GREEN);
        switch_state(STATE_HW01_SET_ALARM);                 // CHANGED
        return;
    }

    // Keep time visible fresh (LCD-002)
    if (s_ev_100ms) {
        s_ev_100ms = 0;
        lcd_draw_time_hhmm(g_hours, g_minutes, LCD_COLOR_WHITE);
    }
}

/*************************************************
 * @brief Set Alarm State
 ************************************************/
void hw01_state_set_alarm(
    alarm_clock_info_t *alarm_info,
    volatile ece353_events_t *events
)
{
    (void)alarm_info;

    if (g_state_entry) {
        g_state_entry = false;
        // Icons per FSM.SA-001/002
        erase_alarm_clock();
        draw_alarm_clock(LCD_COLOR_GREEN);
        erase_speaker();
        draw_speaker(LCD_COLOR_YELLOW);
        // Reset blink for speaker
        s_blink_speaker_on = true;
        // Show ALARM time while setting so SW1/SW2 edits are visible
        lcd_draw_time_hhmm(g_alarm_hours, g_alarm_minutes, LCD_COLOR_WHITE); // CHANGED
    }

    // Blink speaker icon every 500ms
    if (s_ev_500ms) {
        s_ev_500ms = 0;
        s_blink_speaker_on = !s_blink_speaker_on;
        if (s_blink_speaker_on) {
            draw_speaker(LCD_COLOR_YELLOW);
        } else {
            erase_speaker();
        }
    }

    // SW1 increments alarm hours (wrap 23->00)
    if (events->sw1) {
        events->sw1 = 0;
        g_alarm_hours = (uint8_t)((g_alarm_hours + 1U) % 24U);
        lcd_draw_time_hhmm(g_alarm_hours, g_alarm_minutes, LCD_COLOR_WHITE); // CHANGED
    }

    // SW2 increments alarm minutes (wrap 59->00)
    if (events->sw2) {
        events->sw2 = 0;
        g_alarm_minutes = (uint8_t)((g_alarm_minutes + 1U) % 60U);
        lcd_draw_time_hhmm(g_alarm_hours, g_alarm_minutes, LCD_COLOR_WHITE); // CHANGED
    }

    // SW3 -> Running
    if (events->sw3) {
        events->sw3 = 0;
        // Stop blink, keep clock time as-is, alarm stays OFF by default
        erase_speaker();
        g_alarm_enabled = false;
        draw_speaker(LCD_COLOR_GRAY);

        g_fast_mode = true;

        switch_state(STATE_HW01_RUNNING);                   // CHANGED
        return;
    }

    if (s_ev_100ms) {
        s_ev_100ms = 0;
        // Keep showing ALARM time during this state
        lcd_draw_time_hhmm(g_alarm_hours, g_alarm_minutes, LCD_COLOR_WHITE); // CHANGED
    }
}

/*************************************************
 * @brief Running State
 ************************************************/
void hw01_state_running(
    alarm_clock_info_t *alarm_info,
    volatile ece353_events_t *events
)
{
    (void)alarm_info;
    static uint8_t accum_100ms = 0;

    if (g_state_entry) {
        g_state_entry = false;
        // Icons per FSM.RUN-001/002
        erase_alarm_clock();
        draw_alarm_clock(LCD_COLOR_GREEN);
        erase_speaker();
        draw_speaker(g_alarm_enabled ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);
        lcd_draw_time_hhmm(g_hours, g_minutes, LCD_COLOR_WHITE);
        accum_100ms = 0;
    }

    // SW1 toggles alarm enable
    if (events->sw1) {
        events->sw1 = 0;
        g_alarm_enabled = !g_alarm_enabled;                     // FSM.RUN-004
        erase_speaker();
        draw_speaker(g_alarm_enabled ? LCD_COLOR_GREEN : LCD_COLOR_GRAY); // RUN-005/006
    }

    // SW3 -> Set Time
    if (events->sw3) {
        events->sw3 = 0;
        switch_state(STATE_HW01_SET_TIME);                  // CHANGED
        return;
    }

    // Update display every 100ms (FSM.RUN-009 / LCD-002)
    if (s_ev_100ms) {
        s_ev_100ms = 0;
        lcd_draw_time_hhmm(g_hours, g_minutes, LCD_COLOR_WHITE);

        if (g_fast_mode) {
            // Fast simulation: advance one minute every 100ms
            g_minutes = (uint8_t)((g_minutes + 1U) % 60U);
            if (g_minutes == 0) {
                g_hours = (uint8_t)((g_hours + 1U) % 24U);
            }
        } else {
            // Real-time progression: 10 x 100ms = 1s
            accum_100ms++;
            if (accum_100ms >= 10) {
                accum_100ms = 0;
                g_seconds++;
                if (g_seconds >= 60) {
                    g_seconds = 0;
                    g_minutes = (uint8_t)((g_minutes + 1U) % 60U);
                    if (g_minutes == 0) {
                        g_hours = (uint8_t)((g_hours + 1U) % 24U);
                    }
                }
            }
        }
    }

    // If alarm enabled and time equal -> Alarm Triggered
    if (g_alarm_enabled &&
        (g_hours == g_alarm_hours) &&
        (g_minutes == g_alarm_minutes))
    {
        switch_state(STATE_HW01_ALARM_TRIGGERED);           // CHANGED
        return;
    }
}

/*************************************************
 * @brief Alarm Triggered State
 ************************************************/
void hw01_state_alarm_triggered(
    alarm_clock_info_t *alarm_info,
    volatile ece353_events_t *events
)
{
    (void)alarm_info;
    static uint8_t at_elapsed_100ms = 0;   // counts 100ms ticks since entry
    static uint8_t at_accum_100ms = 0;     // timekeeping during alarm

    if (g_state_entry) {
        g_state_entry = false;
        erase_alarm_clock();
        draw_alarm_clock(LCD_COLOR_GREEN);
        erase_speaker();
        draw_speaker(LCD_COLOR_RED);

        buzzer_on();
        g_alarm_buzzing = true;

        // Flash the speaker while alarm is active
        s_blink_speaker_on = true;
        s_ev_500ms = 0;

        // Keep whatever speed mode was active (do NOT force real-time)
        // g_fast_mode = false; // REMOVED

        // Start a fresh 5s window and reset time accumulator
        at_elapsed_100ms = 0;
        at_accum_100ms = 0;
    }

    // Blink speaker icon every 500ms while alarm active
    if (s_ev_500ms) {
        s_ev_500ms = 0;
        s_blink_speaker_on = !s_blink_speaker_on;
        if (s_blink_speaker_on) {
            draw_speaker(LCD_COLOR_RED);
        } else {
            erase_speaker(); // flash
        }
    }

    // Keep time text refreshed and advance clock
    if (s_ev_100ms) {
        s_ev_100ms = 0;

        if (g_fast_mode) {
            // Super fast: advance 1 minute every 100ms
            g_minutes = (uint8_t)((g_minutes + 1U) % 60U);
            if (g_minutes == 0) {
                g_hours = (uint8_t)((g_hours + 1U) % 24U);
            }
        } else {
            // Real-time: 10 x 100ms = 1s
            at_accum_100ms++;
            if (at_accum_100ms >= 10) {
                at_accum_100ms = 0;
                g_seconds++;
                if (g_seconds >= 60) {
                    g_seconds = 0;
                    g_minutes = (uint8_t)((g_minutes + 1U) % 60U);
                    if (g_minutes == 0) {
                        g_hours = (uint8_t)((g_hours + 1U) % 24U);
                    }
                }
            }
        }

        // Draw updated HH:MM
        lcd_draw_time_hhmm(g_hours, g_minutes, LCD_COLOR_WHITE);

        // Track elapsed real seconds for alarm duration
        if (at_elapsed_100ms < 255) {
            at_elapsed_100ms++;  // 50 ticks = 5 seconds
        }
    }

    // Stop after 5 real seconds or SW2 press
    if (events->sw2 || at_elapsed_100ms >= 50) {
        events->sw2 = 0;

        if (g_alarm_buzzing) {
            buzzer_off();
            g_alarm_buzzing = false;
        }

        erase_speaker();
        draw_speaker(LCD_COLOR_GRAY);
        g_alarm_enabled = false;

        switch_state(STATE_HW01_RUNNING);                   // CHANGED
        return;
    }
}

/*************************************************
 * @brief 
 * 
 * @param alarm_info 
 * @param events 
 ************************************************/
void hw01_state_error(
    alarm_clock_info_t *alarm_info,
    volatile ece353_events_t *events
)
{
    printf("State: Error \r\n");

    for(int i = 0; i < 100000; i++); // Delay
    CY_ASSERT(0);

    /* Will never reach this line*/
}

/*************************************************
 * @brief
 * This function will initialize all of the hardware resources for
 * the ICE
 ************************************************/
void app_init_hw(void)
{
    cy_rslt_t rslt;

    console_init();
    printf("\x1b[2J\x1b[;H");
    printf("**************************************************\n\r");
    printf("* %s\n\r", APP_DESCRIPTION);
    printf("* Date: %s\n\r", __DATE__);
    printf("* Time: %s\n\r", __TIME__);
    printf("* Name:%s\n\r", NAME);
    printf("**************************************************\n\r");

    // Initialize the LCD hardware using the driver (config GPIO + controller + clear)
    rslt = lcd_initialize();
    if (rslt != CY_RSLT_SUCCESS) {
        printf("lcd_initialize failed: 0x%08lX\n\r", (unsigned long)rslt);
        CY_ASSERT(0);
    }

    // Make background dark so icons/text are visible
    lcd_clear_screen(LCD_COLOR_BLACK);

    // Draw initial UI immediately so you see something even before the FSM/timer runs
    erase_alarm_clock();
    draw_alarm_clock(LCD_COLOR_YELLOW);  // blinking will start once FSM runs
    erase_speaker();
    draw_speaker(LCD_COLOR_GRAY);
    // Use existing font helper (minutes,seconds) to show HH:MM at startup
    lcd_draw_time(g_hours, g_minutes);

    // Buttons: GPIO + 5ms debounce timer
    rslt = buttons_init_gpio();
    if (rslt != CY_RSLT_SUCCESS) { printf("buttons_init_gpio failed: 0x%08lX\n\r", (unsigned long)rslt); CY_ASSERT(0); }
    rslt = buttons_init_timer();
    if (rslt != CY_RSLT_SUCCESS) { printf("buttons_init_timer failed: 0x%08lX\n\r", (unsigned long)rslt); CY_ASSERT(0); }

    // 100ms periodic timer
    rslt = timer_init(&s_alarm_timer, &s_alarm_timer_cfg, 10000000U, handler_alarm_timer);
    if (rslt != CY_RSLT_SUCCESS) { printf("alarm 100ms timer init failed: 0x%08lX\n\r", (unsigned long)rslt); CY_ASSERT(0); }

    // Initialize buzzer PWM (no startup beep)
    (void)buzzer_init(50.0f, 3500U);
}

/*****************************************************************************/
/* Application Code                                                          */
/*****************************************************************************/
/**
 * @brief
 * This function implements the behavioral requirements for the ICE
 */
void app_main(void)
{
    alarm_clock_info_t alarm_info = (alarm_clock_info_t){0};

    // Initial state and defaults
    g_state = STATE_HW01_INIT;
    g_state_entry = true;
    g_hours = 0;
    g_minutes = 0;
    g_seconds = 0;
    g_alarm_hours = 0;
    g_alarm_minutes = 0;
    g_alarm_enabled = false;
    g_alarm_buzzing = false;
    g_fast_mode = false;

    while (1)
    {
        switch (g_state)
        {
            case STATE_HW01_INIT:
                // Enter Set Time on boot
                switch_state(STATE_HW01_SET_TIME);
                break;

            case STATE_HW01_SET_TIME:
                hw01_state_set_time(&alarm_info, &ECE353_Events);
                break;

            case STATE_HW01_SET_ALARM:
                hw01_state_set_alarm(&alarm_info, &ECE353_Events);
                break;

            case STATE_HW01_RUNNING:
                hw01_state_running(&alarm_info, &ECE353_Events);
                break;

            case STATE_HW01_ALARM_TRIGGERED:
                hw01_state_alarm_triggered(&alarm_info, &ECE353_Events);
                break;

            case STATE_HW01_ERROR:
            default:
                hw01_state_error(&alarm_info, &ECE353_Events);
                break;
        }
    }
}

// Close HW01 compile guard
#endif // defined(HW01)