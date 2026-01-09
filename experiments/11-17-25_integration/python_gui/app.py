import tkinter as tk
import ttkbootstrap as ttk
from ttkbootstrap.constants import *
from mock_hardware import MockHardware

class GestureAudioApp(ttk.Window):
    def __init__(self, hardware_interface=None):
        super().__init__(themename="cyborg")
        self.title("Gesture Audio Processor")
        self.geometry("800x600")
        
        if hardware_interface:
            self.hardware = hardware_interface
        else:
            self.hardware = MockHardware()
        
        # Configure grid layout
        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=1)
        self.rowconfigure(1, weight=1)

        self.create_header()
        self.create_deck_panels()
        self.create_mixer_panel()
        self.create_effects_panel()
        
        # Start update loop
        self.update_gui()

    def create_header(self):
        header_frame = ttk.Frame(self, padding=10)
        header_frame.grid(row=0, column=0, columnspan=2, sticky="ew")
        
        title_label = ttk.Label(
            header_frame, 
            text="Gesture Audio Processor", 
            font=("Helvetica", 24, "bold"),
            bootstyle="inverse-primary"
        )
        title_label.pack(pady=10, fill="x")

    def create_deck_panels(self):
        # Deck A
        self.deck_a_frame = ttk.Labelframe(self, text="Deck A", padding=10, bootstyle="info")
        self.deck_a_frame.grid(row=1, column=0, sticky="nsew", padx=10, pady=10)
        
        self.song_a_label = ttk.Label(self.deck_a_frame, text="Song: ...", font=("Helvetica", 12))
        self.song_a_label.pack(pady=5)
        
        self.status_a_label = ttk.Label(self.deck_a_frame, text="STOPPED", bootstyle="danger", font=("Helvetica", 14, "bold"))
        self.status_a_label.pack(pady=10)
        
        ttk.Label(self.deck_a_frame, text="Pitch").pack()
        self.pitch_a_meter = ttk.Meter(
            self.deck_a_frame,
            metersize=150,
            padding=5,
            amountused=0,
            metertype="semi",
            subtext="x Speed",
            interactive=False,
            bootstyle="info"
        )
        self.pitch_a_meter.pack(pady=10)

        # Deck B
        self.deck_b_frame = ttk.Labelframe(self, text="Deck B", padding=10, bootstyle="warning")
        self.deck_b_frame.grid(row=1, column=1, sticky="nsew", padx=10, pady=10)
        
        self.song_b_label = ttk.Label(self.deck_b_frame, text="Song: ...", font=("Helvetica", 12))
        self.song_b_label.pack(pady=5)
        
        self.status_b_label = ttk.Label(self.deck_b_frame, text="STOPPED", bootstyle="danger", font=("Helvetica", 14, "bold"))
        self.status_b_label.pack(pady=10)
        
        ttk.Label(self.deck_b_frame, text="Pitch").pack()
        self.pitch_b_meter = ttk.Meter(
            self.deck_b_frame,
            metersize=150,
            padding=5,
            amountused=0,
            metertype="semi",
            subtext="x Speed",
            interactive=False,
            bootstyle="warning"
        )
        self.pitch_b_meter.pack(pady=10)

    def create_mixer_panel(self):
        mixer_frame = ttk.Labelframe(self, text="Mixer", padding=10, bootstyle="secondary")
        mixer_frame.grid(row=2, column=0, columnspan=2, sticky="ew", padx=10, pady=10)
        
        # Master Volume
        vol_frame = ttk.Frame(mixer_frame)
        vol_frame.pack(side="left", fill="x", expand=True, padx=20)
        ttk.Label(vol_frame, text="Master Volume").pack()
        self.vol_progress = ttk.Progressbar(vol_frame, bootstyle="success-striped", length=200)
        self.vol_progress.pack(pady=5, fill="x")
        self.vol_label = ttk.Label(vol_frame, text="0%")
        self.vol_label.pack()

        # Mixer Mode Indicator
        self.mixer_mode_label = ttk.Label(mixer_frame, text="MODE: NORMAL", bootstyle="light-inverse", font=("Helvetica", 12, "bold"))
        self.mixer_mode_label.pack(side="right", padx=20)

    def create_effects_panel(self):
        fx_frame = ttk.Labelframe(self, text="Effects & EQ", padding=10, bootstyle="primary")
        fx_frame.grid(row=3, column=0, columnspan=2, sticky="ew", padx=10, pady=10)
        
        # Echo
        self.echo_check = ttk.Checkbutton(fx_frame, text="Echo", bootstyle="round-toggle", state="disabled")
        self.echo_check.pack(side="left", padx=20)
        
        # EQ
        eq_container = ttk.Frame(fx_frame)
        eq_container.pack(side="right", fill="x", expand=True)
        
        self.eq_bars = []
        for i, band in enumerate(["Low", "Mid", "High"]):
            frame = ttk.Frame(eq_container)
            frame.pack(side="left", fill="y", expand=True, padx=5)
            ttk.Label(frame, text=band).pack()
            bar = ttk.Progressbar(frame, orient="vertical", length=60, bootstyle="info")
            bar.pack(pady=5)
            self.eq_bars.append(bar)

    def update_gui(self):
        self.hardware.update()
        state = self.hardware.get_state()
        
        # Update Decks
        self.song_a_label.config(text=f"Song: {state['song_a']}")
        self.song_b_label.config(text=f"Song: {state['song_b']}")
        
        if state['deck_a_playing']:
            self.status_a_label.config(text="PLAYING", bootstyle="success")
        else:
            self.status_a_label.config(text="STOPPED", bootstyle="danger")
            
        if state['deck_b_playing']:
            self.status_b_label.config(text="PLAYING", bootstyle="success")
        else:
            self.status_b_label.config(text="STOPPED", bootstyle="danger")

        # Update Pitch Meters
        self.pitch_a_meter.configure(amountused=int(state['pitch_a'] * 100 / 2.5)) # Scale roughly to meter
        self.pitch_a_meter.amountusedvar.set(int(state['pitch_a'] * 100 / 2.5)) # Hack for meter update sometimes
        # Actually meter text update:
        # ttkbootstrap meter doesn't easily let us change the center text to float without subclassing or variable trickery
        # We'll just rely on the visual arc for now or try to set the variable if it was bound.
        # Let's just update the label if we can, but Meter handles its own text.
        
        self.pitch_b_meter.configure(amountused=int(state['pitch_b'] * 100 / 2.5))

        # Update Mixer
        self.vol_progress['value'] = state['volume'] * 100
        self.vol_label.config(text=f"{int(state['volume']*100)}%")
        
        if state['mixer_mode']:
            self.mixer_mode_label.config(text="MODE: MIXER (LOCKED)", bootstyle="danger-inverse")
        else:
            self.mixer_mode_label.config(text="MODE: NORMAL", bootstyle="light-inverse")

        # Update Effects
        # Checkbutton state variable would be better, but we can just select/deselect
        if state['echo_enabled']:
            if "selected" not in self.echo_check.state():
                self.echo_check.invoke() 
        else:
            if "selected" in self.echo_check.state():
                self.echo_check.invoke()

        # Update EQ
        for i, bar in enumerate(self.eq_bars):
            bar['value'] = state['eq_bands'][i] * 100

        # Schedule next update
        self.after(200, self.update_gui)

if __name__ == "__main__":
    app = GestureAudioApp()
    app.mainloop()
