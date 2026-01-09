#ifndef AUDIO_CONTROL_H
#define AUDIO_CONTROL_H

#include <Audio.h>

// Global declarations
extern AudioControlSGTL5000 sgtl5000_1;
extern float volumeLevel;

// Function declarations
void volumeUp();
void volumeDown();

// can add more filters/effects down here

#endif
