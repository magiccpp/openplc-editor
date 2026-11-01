// SPDX-License-Identifier: GPL-3.0-only
#include <stdlib.h>
extern "C" {
#include "openplc.h"
}
#include "Arduino.h"
#include "defines.h"

//OpenPLC HAL for Raspberry Pi Pico 2 / Pico 2 W with the RP2350
//
//Differences from the stock rp2040pico.cpp HAL:
// - Digital inputs use the internal pull-up. RP2350 erratum E9 makes a
//   floating or pull-down input latch high, so a switch to GND with a
//   pull-up is the only reliable wiring (pressed = FALSE).
// - GP25 is not touched. On the Pico 2 W it is the CYW43 chip select;
//   the onboard LED is LED_BUILTIN (pin 64), driven through the CYW43.
// - Analog outputs use the core's analogWrite PWM instead of the
//   RP2040_PWM library, so no RP2040-only library is needed.

/******************PINOUT CONFIGURATION***********************
Set in the editor pin mapping (devices/pin-mapping.json).
Onboard LED: GP25 on the Pico 2, pin 64 (LED_BUILTIN via CYW43) on the Pico 2 W.
**************************************************************/

#define PWM_FREQ 10000
#define PWM_RANGE 65535

uint8_t pinMask_DIN[] = {PINMASK_DIN};
uint8_t pinMask_AIN[] = {PINMASK_AIN};
uint8_t pinMask_DOUT[] = {PINMASK_DOUT};
uint8_t pinMask_AOUT[] = {PINMASK_AOUT};

bool PWM_block[NUM_ANALOG_OUTPUT + 1]; //track which pins use the PWM_CONTROLLER block

//this call is required for the C-based PWM block on the Editor
extern "C" uint8_t set_hardware_pwm(uint8_t, float, float);

// called by the PWM_CONTROLLER block; the frequency is shared by all channels
uint8_t set_hardware_pwm(uint8_t ch, float freq, float duty)
{
    if (ch >= NUM_ANALOG_OUTPUT) return 0;
    if (freq < 1) return 0;

    duty = duty < 0 ? 0 : duty > 100 ? 100 : duty;

    analogWriteFreq((uint32_t)freq);
    analogWrite(pinMask_AOUT[ch], (int)(duty * PWM_RANGE / 100.0f));
    PWM_block[ch] = true;
    return 1;
}

void hardwareInit()
{
    analogReadResolution(12);
    analogWriteFreq(PWM_FREQ);
    analogWriteRange(PWM_RANGE);

    for (int i = 0; i < NUM_DISCRETE_INPUT; i++)
    {
        pinMode(pinMask_DIN[i], INPUT_PULLUP);
    }
    for (int i = 0; i < NUM_ANALOG_INPUT; i++)
    {
        pinMode(pinMask_AIN[i], INPUT);
    }
    for (int i = 0; i < NUM_DISCRETE_OUTPUT; i++)
    {
        pinMode(pinMask_DOUT[i], OUTPUT);
        digitalWrite(pinMask_DOUT[i], LOW);
    }
    for (int i = 0; i < NUM_ANALOG_OUTPUT; i++)
    {
        PWM_block[i] = false; //assume no blocks used
        analogWrite(pinMask_AOUT[i], 0);
    }
}

void updateInputBuffers()
{
    for (int i = 0; i < NUM_DISCRETE_INPUT; i++)
    {
        if (bool_input[i/8][i%8] != NULL)
            *bool_input[i/8][i%8] = digitalRead(pinMask_DIN[i]);
    }
    for (int i = 0; i < NUM_ANALOG_INPUT; i++)
    {
        if (int_input[i] != NULL)
            *int_input[i] = analogRead(pinMask_AIN[i]);
    }
}

void updateOutputBuffers()
{
    for (int i = 0; i < NUM_DISCRETE_OUTPUT; i++)
    {
        if (bool_output[i/8][i%8] != NULL)
            digitalWrite(pinMask_DOUT[i], *bool_output[i/8][i%8]);
    }
    for (int i = 0; i < NUM_ANALOG_OUTPUT; i++)
    {
        //pins driven by the PWM_CONTROLLER block keep their own setting
        if (int_output[i] != NULL && PWM_block[i] == false)
            analogWrite(pinMask_AOUT[i], *int_output[i]);
    }
}
