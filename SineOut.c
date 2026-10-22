#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"

#define PWM_PIN 16          // Target GPIO pin
#define CARRIER_FREQ 100000 // 100 kHz high-frequency PWM carrier
//#define SINE_FREQ 60.0      // Target low-frequency sine wave (10 Hz)
#define SAMPLE_RATE 2000    // Update duty cycle 2000 times per second

static float wave_freq = 50.0; // Default sine wave frequency
static uint32_t sample_idx = 0;

uint16_t pwm_wrap_val;
uint slice_num;
uint chan;
enum pwm_output{
    PWM_SINE =0,
    PWM_SQUARE,
    PWM_SAWTOOTH,
    PWM_TRIANGLE,
    PWM_COMPOSITE        

} this_pwm_output = PWM_SINE;

// Timer callback to update the duty cycle
bool timer_callback(repeating_timer_t *rt) {

    float sine_val, square_val, sawtooth_val, triangle_val, composite_val;
    // Calculate current angle
    float time = (float)sample_idx / SAMPLE_RATE;
    float pwm_output = 0.0f;
    sine_val = sinf(2.0f * M_PI * wave_freq * time);
    if(sine_val < 0.0f) {
        square_val = 1.0f; // Ensure non-negative for PWM
    } else { square_val = 0.0f; }
    sawtooth_val = 2.0f * (time * wave_freq - floorf(time * wave_freq + 0.5f)); // Sawtooth from -1 to 1
    triangle_val = 2.0f * fabsf(sawtooth_val) - 1.0f; // Triangle from -1 to 1
    composite_val = sine_val + 0.1f * (sinf(4.0f * M_PI * wave_freq * time) + 
                    sinf(6.0f * M_PI * wave_freq * time) +
                    sinf(10.0f * M_PI * wave_freq * time) +
                    sinf(14.0f * M_PI * wave_freq * time) +
                    sinf(50.0f * M_PI * wave_freq * time) +
                    sinf(70.0f * M_PI * wave_freq * time) +
                    sinf(100.0f * M_PI * wave_freq * time));  
    
    // Scale sine wave (-1.0 to 1.0) to PWM duty cycle (0 to pwm_wrap_val)
    switch(this_pwm_output) {
        case PWM_SINE:
            pwm_output = (sine_val + 1.0f) * 0.5f; // Scale to 0..1
            break;
        case PWM_SQUARE:
            pwm_output = square_val; // Already 0 or 1
            break;
        case PWM_SAWTOOTH:
            pwm_output = (sawtooth_val + 1.0f) * 0.5f; // Scale to 0..1
            break;
        case PWM_TRIANGLE:
            pwm_output = (triangle_val + 1.0f) * 0.5f; // Scale to 0..1
            break;
        case PWM_COMPOSITE:
            pwm_output = (composite_val + 1.0f) * 0.5f; // Scale to 0..1
            break;
    }

    uint16_t level = (uint16_t)(pwm_output * pwm_wrap_val);
    
    // Update the hardware channel level
    pwm_set_chan_level(slice_num, chan, level);
    
    sample_idx++;
    if (sample_idx >= SAMPLE_RATE) sample_idx = 0; // Prevent overflow
    
    return true; // Keep timer running
}

int main() {
    stdio_init_all();
     if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // 1. Setup PWM Pin & Slice
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(PWM_PIN);
    chan = pwm_gpio_to_channel(PWM_PIN);

    // 2. Calculate Clock Divider and Wrap for 100 kHz carrier frequency
    uint32_t sys_clk = clock_get_hz(clk_sys);
    
    // Let's use a standard wrap of 1000 for good duty cycle resolution
    pwm_wrap_val = 1000; 
    float clk_div = (float)sys_clk / (CARRIER_FREQ * (pwm_wrap_val + 1));
    
    pwm_set_clkdiv(slice_num, clk_div);
    pwm_set_wrap(slice_num, pwm_wrap_val);
    pwm_set_enabled(slice_num, true);

    // 3. Setup a Repeating Timer for changing the duty cycle
    repeating_timer_t timer;
    int32_t delay_us = -(1000000 / SAMPLE_RATE); // Negative value ensures strict timing
    add_repeating_timer_us(delay_us, timer_callback, NULL, &timer);

    printf("Generating %g Hz sine wave on pin %d\n", wave_freq, PWM_PIN);
    printf("Press 1 for sine, 2 for square, 3 for sawtooth, 4 for triangle, 5 for composite\n");
    printf("Press 0 to change frequency (enter new frequency in Hz)\n");
    int i=0;

    while (1) {
        int ch = getchar_timeout_us(0);
        if (ch >= '1' && ch <= '5') {
            switch (ch) {
                case '1': this_pwm_output = PWM_SINE; printf("Generating %g Hz sine wave on pin %d\n", wave_freq, PWM_PIN);break;
                case '2': this_pwm_output = PWM_SQUARE; printf("Generating %g Hz square wave on pin %d\n", wave_freq, PWM_PIN);break;
                case '3': this_pwm_output = PWM_SAWTOOTH; printf("Generating %g Hz sawtooth wave on pin %d\n", wave_freq, PWM_PIN);break;
                case '4': this_pwm_output = PWM_TRIANGLE; printf("Generating %g Hz triangle wave on pin %d\n", wave_freq, PWM_PIN);break;
                case '5': this_pwm_output = PWM_COMPOSITE; printf("Generating %g Hz composite wave on pin %d\n", wave_freq, PWM_PIN);break;
            }
            printf("Press 1 for sine, 2 for square, 3 for sawtooth, 4 for triangle, 5 for composite\n");
            printf("Press 0 to change frequency (enter new frequency in Hz)\n");
 
        } else if (ch == '0') {
            printf("Enter new frequency in Hz: ");
            float new_freq;
            scanf("%f", &new_freq);
            wave_freq = new_freq;
            printf("Frequency changed to %g Hz\n", wave_freq);
            printf("Press 1 for sine, 2 for square, 3 for sawtooth, 4 for triangle, 5 for composite\n");
            printf("Press 0 to change frequency (enter new frequency in Hz)\n");

        }
        sleep_ms(100);
        i++;
        if(i==10){
            i=0;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN));
        }
        
    }
}
