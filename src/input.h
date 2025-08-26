#pragma once
#include <stdint.h>

int32_t takeEncoderDelta();
void inputUpdate();  // ruft intern M5Dial.update(), verarbeitet Touch/Encoder/BtnA
void startEncoderSampler();
void stopEncoderSampler();