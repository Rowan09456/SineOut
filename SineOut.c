#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"

#define PWM_PIN     15
#define TOP         1023          // 10-bit resolution, ~146 kHz carrier
#define TABLE_SIZE  256
#define F_OUT       1000.0        // sine frequency in Hz

static uint16_t sine_table[TABLE_SIZE];
static uint slice;
static uint32_t phase = 0, phase_inc;

// Runs once per PWM cycle; kept in RAM so it never waits on flash
static void __not_in_flash_func(on_pwm_wrap)(void) {
    pwm_clear_irq(slice);
    phase += phase_inc;                                   // wraps naturally at 2^32
    pwm_set_gpio_level(PWM_PIN, sine_table[phase >> 24]); // top 8 bits = table index
}




int main()
{
    // Sine table scaled to 0..TOP, centred on TOP/2
    for (int i = 0; i < TABLE_SIZE; i++)
        sine_table[i] = (uint16_t)((sin(2 * M_PI * i / TABLE_SIZE) + 1.0) * 0.5 * TOP + 0.5);

    stdio_init_all();

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    printf("System Clock Frequency is %d Hz\n", clock_get_hz(clk_sys));
    printf("USB Clock Frequency is %d Hz\n", clock_get_hz(clk_usb));
    // For more examples of clocks use see https://github.com/raspberrypi/pico-examples/tree/master/clocks

    // Example to turn on the Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(PWM_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 1.0f);
    pwm_config_set_wrap(&cfg, TOP);
    pwm_init(slice, &cfg, false);

    double fs = clock_get_hz(clk_sys) / (double)(TOP + 1);   // sample rate
    phase_inc = (uint32_t)(F_OUT / fs * 4294967296.0);       // F_OUT / fs × 2^32

    pwm_clear_irq(slice);
    pwm_set_irq_enabled(slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_set_enabled(slice, true);


    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN));
    }
}
