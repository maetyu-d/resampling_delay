# Resampling Delay

A JUCE/C++ audio plugin with two lofi modes:

- **Delay**: a fractional delay line whose delay time moves by resampling the buffer, creating tape-style pitch bends.
- **Reverb**: a small modulated delay network whose reverb time changes by resampling the taps, creating smeared, unstable tails.

Controls:

- **Mode**: Delay or Reverb
- **Echo Time**: delay mode time
- **Repeats**: delay regeneration
- **Reverb Decay**: reverb mode size/decay
- **Dry/Wet**: dry/wet blend
- **LoFi Wear**: wow, sample hold, and bit crushing
- **Tone**: dark-to-bright one-pole filtering

## Build

This project expects a JUCE checkout. By default `CMakeLists.txt` points at:

```sh
/Users/user/Documents/Granny/JUCE
```

Configure and build:

```sh
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
```

The CMake target is configured for `AU`, `VST3`, and `Standalone`.
