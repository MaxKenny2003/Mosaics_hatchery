#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)    // Have to use macros and led0 is an alias
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);  // GPIO structure`
int main()
{
    int ret;

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);    // configure the gpio to be an output initially inactive
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_set_dt(&led, 1); // Set it to 1
    if (ret < 0) {
        return ret;
    }

    while(1)
    {
        gpio_pin_toggle_dt(&led);
        k_msleep(500);
    }


    return 0;
}