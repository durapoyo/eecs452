import random
import time

class MockHardware:
    def __init__(self):
        self.state = {
            "volume": 0.4,
            "pitch_a": 1.0,
            "pitch_b": 1.0,
            "deck_a_playing": False,
            "deck_b_playing": False,
            "mixer_mode": False,
            "echo_enabled": False,
            "eq_bands": [0.5, 0.5, 0.5],  # Low, Mid, High (0.0 - 1.0)
            "song_a": "song1.wav",
            "song_b": "song2.wav"
        }
        self.last_update = time.time()

    def update(self):
        """Simulate hardware changes and sensor noise."""
        current_time = time.time()
        
        # Only update periodically to simulate real world polling or events
        # But for the GUI loop, we call this often.
        
        # Randomly toggle mixer mode (rarely)
        if random.random() < 0.005:
            self.state["mixer_mode"] = not self.state["mixer_mode"]
            
        # Randomly toggle echo (rarely)
        if random.random() < 0.005:
            self.state["echo_enabled"] = not self.state["echo_enabled"]

        # Simulate sensor noise/movement for Volume
        if not self.state["mixer_mode"]:
            # Volume drift
            change = random.uniform(-0.02, 0.02)
            self.state["volume"] = max(0.0, min(1.0, self.state["volume"] + change))
            
            # Pitch drift
            change_a = random.uniform(-0.01, 0.01)
            self.state["pitch_a"] = max(0.3, min(2.5, self.state["pitch_a"] + change_a))
            
            change_b = random.uniform(-0.01, 0.01)
            self.state["pitch_b"] = max(0.3, min(2.5, self.state["pitch_b"] + change_b))

        # Simulate EQ changes
        for i in range(3):
            change = random.uniform(-0.01, 0.01)
            self.state["eq_bands"][i] = max(0.0, min(1.0, self.state["eq_bands"][i] + change))

        # Randomly start/stop decks
        if random.random() < 0.002:
            self.state["deck_a_playing"] = not self.state["deck_a_playing"]
        if random.random() < 0.002:
            self.state["deck_b_playing"] = not self.state["deck_b_playing"]

    def get_state(self):
        return self.state
