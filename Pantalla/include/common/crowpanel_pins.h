#ifndef CROWPANEL_PINS_H
#define CROWPANEL_PINS_H

// =============================================================================
// CrowPanel 5.0" ESP32-S3 Pin Definitions
// =============================================================================


// Display Interface (RGB Parallel) - Reserved, do not use
#define TFT_DE_PIN          40
#define TFT_VSYNC_PIN       41
#define TFT_HSYNC_PIN       39
#define TFT_PCLK_PIN        0
// RGB Data pins (R0-R4, G0-G5, B0-B4) - pins 1,3,4,5,6,7,8,9,14,15,16,21,45,46,47,48

// Touch Interface (I2C) - Reserved, do not use
#define TOUCH_SDA_PIN       19  // GT911 I2C SDA
#define TOUCH_SCL_PIN       20  // GT911 I2C SCL
#define TOUCH_INT_PIN       -1  // Touch interrupt (if used)
#define TOUCH_RST_PIN       -1  // Touch reset (if used)

// Backlight Control
#define TFT_BL_PIN          2   // PWM backlight control

// Available GPIO pins for external connections
// These pins are available on the CrowPanel expansion connectors

// Primary GPIO expansion (high priority usage)
#define GPIO_AVAILABLE_1    1   // Available for LED strip or buttons
#define GPIO_AVAILABLE_2    3   // Available for LED strip or buttons  
#define GPIO_AVAILABLE_3    8   // Available for buttons
#define GPIO_AVAILABLE_4    9   // Available for buttons
#define GPIO_AVAILABLE_5    10  // Available for buttons
// #define GPIO_AVAILABLE_6    11  // Available for 
#define GPIO_AVAILABLE_7    12  // Available for buttons
#define GPIO_AVAILABLE_8    13  // Available for buttons

// Secondary GPIO expansion (if more pins needed)
#define GPIO_AVAILABLE_9    17  // Alternative GPIO
#define GPIO_AVAILABLE_10   18  // Alternative GPIO
#define GPIO_AVAILABLE_11   33  // Alternative GPIO
#define GPIO_AVAILABLE_12   34  // Alternative GPIO
#define GPIO_AVAILABLE_13   35  // Alternative GPIO
#define GPIO_AVAILABLE_14   36  // Alternative GPIO
#define GPIO_AVAILABLE_15   37  // Alternative GPIO
// #define GPIO_AVAILABLE_16   38  // Alternative GPIO

// Power pins
#define POWER_3V3           3.3  // 3.3V supply
#define POWER_5V            5.0  // 5V supply
#define POWER_GND           0    // Ground

// =============================================================================
// Application-Specific Pin Assignments
// =============================================================================

// LED Strip (WS2812B)
#define LED_STRIP_PIN       GPIO_AVAILABLE_1  // GPIO 1

// Wheel Buttons - Main buttons
#define WHEEL_B1_PIN        GPIO_AVAILABLE_2  // GPIO 3
#define WHEEL_B2_PIN        GPIO_AVAILABLE_3  // GPIO 8
#define WHEEL_B3_PIN        GPIO_AVAILABLE_4  // GPIO 9
#define WHEEL_B4_PIN        GPIO_AVAILABLE_5  // GPIO 10

// Wheel Button LEDs
#define WHEEL_B1_LED_PIN    GPIO_AVAILABLE_6  // GPIO 11
#define WHEEL_B2_LED_PIN    GPIO_AVAILABLE_7  // GPIO 12
#define WHEEL_B3_LED_PIN    GPIO_AVAILABLE_8  // GPIO 13
#define WHEEL_B4_LED_PIN    GPIO_AVAILABLE_9  // GPIO 17

// Paddle Shifters
#define PADDLE_LEFT_PIN     GPIO_AVAILABLE_10 // GPIO 18
#define PADDLE_RIGHT_PIN    GPIO_AVAILABLE_11 // GPIO 33

// Rotary Encoders
#define ENCODER1_A_PIN      GPIO_AVAILABLE_12 // GPIO 34
#define ENCODER1_B_PIN      GPIO_AVAILABLE_13 // GPIO 35
#define ENCODER1_BTN_PIN    GPIO_AVAILABLE_14 // GPIO 36

#define ENCODER2_A_PIN      GPIO_AVAILABLE_15 // GPIO 37
#define ENCODER2_B_PIN      GPIO_AVAILABLE_16 // GPIO 38
#define ENCODER2_BTN_PIN    GPIO_AVAILABLE_1  // Reuse if needed

// =============================================================================
// Hardware Validation
// =============================================================================

// Ensure critical pins are not conflicting
#if LED_STRIP_PIN == CAN_TX_PIN || LED_STRIP_PIN == CAN_RX_PIN
#error "LED Strip pin conflicts with CAN interface"
#endif

#if TOUCH_SDA_PIN == WHEEL_B1_PIN || TOUCH_SCL_PIN == WHEEL_B1_PIN
#error "Wheel button pins conflict with touch interface"
#endif

#endif // CROWPANEL_PINS_H 