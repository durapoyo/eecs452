void classifyGesture(sensors_event_t* gyroData, unsigned long currentTime) {
  static String rollState = "IDLE";
  static unsigned long lastGestureTime = 0;

  float rollRate = gyroData->gyro.x;  // rad/s

  // --- Tunable parameters ---
  const float smallPrepThresh = 2.0;     // small prep threshold (both directions)
  const float bigPushThresh   = 4.0;     // strong push threshold
  const float stopThresh      = 0.3;     // near-zero motion
  const unsigned long debounce = 150;    // ms between gestures
  const unsigned long comboWindow = 600; // ms between prep + push

  // --- Gesture State Machine ---
  if (rollState == "IDLE") {
    if (rollRate < -smallPrepThresh) {
      // small roll to the LEFT → prepare for increase tempo
      rollState = "PREP_LEFT";
      lastGestureTime = currentTime;
      Serial.println("Prep: small roll LEFT");
    } 
    else if (rollRate > smallPrepThresh) {
      // small roll to the RIGHT → prepare for decrease tempo
      rollState = "PREP_RIGHT";
      lastGestureTime = currentTime;
      Serial.println("Prep: small roll RIGHT");
    }
  }

  else if (rollState == "PREP_LEFT") {
    // wait for follow-up strong RIGHT roll
    if (rollRate > bigPushThresh && (currentTime - lastGestureTime < comboWindow)) {
      Serial.println("🎵 Increase tempo! (Left + Right combo)");
      rollState = "IDLE";
      lastGestureTime = currentTime;
    } 
    else if (currentTime - lastGestureTime > comboWindow) {
      // timeout: reset if too long
      rollState = "IDLE";
    }
  }

  else if (rollState == "PREP_RIGHT") {
    // wait for follow-up strong LEFT roll
    if (rollRate < -bigPushThresh && (currentTime - lastGestureTime < comboWindow)) {
      Serial.println("🎵 Decrease tempo! (Right + Left combo)");
      rollState = "IDLE";
      lastGestureTime = currentTime;
    } 
    else if (currentTime - lastGestureTime > comboWindow) {
      rollState = "IDLE";
    }
  }

  // --- Small deadband reset to idle ---
  if (fabs(rollRate) < stopThresh && (currentTime - lastGestureTime > debounce)) {
    if (rollState != "IDLE") {
      rollState = "IDLE";
    }
  }
}
