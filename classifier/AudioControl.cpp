#include "AudioControl.h"

// Define the functions
void volumeUp() {
    volumeLevel = min(volumeLevel + 0.05, 1.0);
    sgtl5000_1.volume(volumeLevel);
}

void volumeDown() {
    volumeLevel = max(volumeLevel - 0.05, 0.0);
    sgtl5000_1.volume(volumeLevel);
}
