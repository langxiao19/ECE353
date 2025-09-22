/**
 * @author Joe Krachey (jkrachey@wisc.edu)
 * @brief
 * @version 0.1
 * @date 2025-07-10
 * @copyright Copyright (c) 2025
 */
#include "buzzer.h"

static cyhal_pwm_t s_buzzer_pwm;
static bool        s_buzzer_inited = false;
static float       s_buzzer_duty   = 50.0f;
static float       s_buzzer_freq   = 3500.0f;

cy_rslt_t buzzer_init(float duty_cycle, uint32_t frequency)
{
    cy_rslt_t rslt;

    if (!s_buzzer_inited) {
        rslt = cyhal_pwm_init(&s_buzzer_pwm, PIN_BUZZER, NULL);
        if (rslt != CY_RSLT_SUCCESS) {
            return rslt;
        }
        s_buzzer_inited = true;
    }

    // Clamp and store settings
    if (duty_cycle < 0.0f) duty_cycle = 0.0f;
    if (duty_cycle > 100.0f) duty_cycle = 100.0f;
    if (frequency == 0U) frequency = 3500U;

    s_buzzer_duty = duty_cycle;
    s_buzzer_freq = (float)frequency;

    // Program PWM but do not start yet
    return cyhal_pwm_set_duty_cycle(&s_buzzer_pwm, s_buzzer_duty, s_buzzer_freq);
}

void buzzer_on(void)
{
    if (!s_buzzer_inited) {
        (void)buzzer_init(50.0f, 3500U);
    }
    // Ensure current settings applied, then start
    (void)cyhal_pwm_set_duty_cycle(&s_buzzer_pwm, s_buzzer_duty, s_buzzer_freq);
    (void)cyhal_pwm_start(&s_buzzer_pwm);
}

void buzzer_off(void)
{
    if (!s_buzzer_inited) return;
    (void)cyhal_pwm_stop(&s_buzzer_pwm);
}