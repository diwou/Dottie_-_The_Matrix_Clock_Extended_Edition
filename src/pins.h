#pragma once

// Zentrale Hardwarebelegung. Die Werte koennen in platformio.ini per
// -D... ueberschrieben werden, sobald die finale Platinenbelegung feststeht.
#ifndef DISPLAY_DATA_PIN
  #define DISPLAY_DATA_PIN 11
#endif

#ifndef DISPLAY_CLK_PIN
  #define DISPLAY_CLK_PIN 12
#endif

#ifndef DISPLAY_CS_PIN
  #define DISPLAY_CS_PIN 10
#endif

#ifndef FACTORY_RESET_PIN
  #define FACTORY_RESET_PIN 9
#endif

#ifndef I2C_SDA_PIN
  #define I2C_SDA_PIN 38
#endif

#ifndef I2C_SCL_PIN
  #define I2C_SCL_PIN 39
#endif
