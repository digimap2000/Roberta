#pragma once

#if defined(PLATFORM_STM32F4)
  #include "platform/stm32f4/platform_init.h"
#elif defined(PLATFORM_NRF52)
  #include "platform/nrf52/platform_init.h"
#elif defined(PLATFORM_PICO2W)
  #include "platform/pico2w/platform_init.h"
#else
  #error "No platform defined!"
#endif