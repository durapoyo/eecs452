# EECS452 Fall 2025 Team 3/4: Gesture-Controlled DJ Board

## Overview

Traditional audio system interfaces like knobs, sliders, switches, and foot pedals offer precise control but limit a performer's freedom and flexibility. Our project explores an alternate approach: using hand gestures as the primary method for manipulating audio effects in real time. The **Gesture-Controlled DJ Board** is an interactive audio system that lets a user control music playback, volume, pitch, effects, and mixing through hand gestures, enablimg a more expressive and visually engaging performances.

Using the Teensy 4.1 + Audio Shield, BNO055 IMU accelerometer, and a RaspberryPi, the board provides a cost-savvy, seamless, touch-free experience for those looking to manipulate and create live audio mixes without an actual DJ controller. Similar gesture-based DJ and music controllers have been used in live sets, multimedia performances, adn experimental installations, showing that hands-free control can enhance stage presence and audience engagement.

This project integrates:
- Real-time DSP (granular, pitch shifting, filtering)
- Embedded motion sensing
- Custom OpenCV + IMU hand gesture classification logic
- Scalable architecture for new effects and motion patterns

## Key Features
1. Touch-Free Gesture Control
    Control audio using hand movements:
    - **Volume:** Roll wrist left and right
    - **Pitch:** Pitch wrist up and down
    - **Stop/Start:** Closed Fist/Open Palm
    - **Skip track:** Swipe right

2. Teensy Audio
    - Low-latency granular synthesis
    - Pitch shifting
    - Adjustable effects
    - Preloaded audio playback into SD card

3. Raspberry Pi
    - OpenCV for hand gesture classification

4. Modularity
Each major component is modular:
    - Motion/gesture interpretation
    - DSP pipeline
    - Effect modules
    - GUI display
    - Communication layer

    This allows extendability for new filters, effects, or gestures

## Use cases
1. Live DJ and Electronic Performance
    - Performers can manipulate audio with expressive arm and hand movements, turning body motion into a part of the show.

2. Accessibility and Alternative Control
    - Gesture-based music interfaces have been explored as accessbiel instruments for users who may not be able to operate traditional controllers or instruments, enabling music creation through larger, more comfortable motions.
    - The touch-free DJ board can support users with limited fine motor control, alighning with recent work on accessible digital music interfaces.
  
   
