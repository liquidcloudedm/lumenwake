# Lumenwake

An audio-reactive visualiser plugin. Put it on a track, and the picture moves with whatever plays through it. Audio passes through untouched.

`dist/` holds the Windows build (VST3 and a standalone app). The same source is set up to build as VST3 and Audio Unit on a Mac; see "Mac, Audio Unit and VST2" below for what that needs and what has and has not been tried.

All shaders, scenes and the interface are original to this project.

## Install (Ableton Live 12)

1. Copy the whole `dist/Lumenwake.vst3` folder into `C:\Program Files\Common Files\VST3\` (needs administrator rights).
2. In Live: Settings > Plug-Ins, make sure "Use VST3 Plug-In System Folders" is on, then Rescan.
3. Lumenwake appears under Plug-Ins > VST3 > Lumenwake. Drop it on an audio or instrument track.

`dist/Lumenwake.exe` is the same thing as a standalone app that listens to an audio input.

## Scenes

| Scene | What it does |
| --- | --- |
| Image | Your own picture, moved by the music. 12 modes: Liquid, Ripple, Swirl, Kaleido, Tunnel, Flyover, Relief, Glitch, Mosaic, Echo, Wave, Prism. The picture keeps its own colours. |
| Model 3D | Your own 3D model, or the built-in knot. 6 modes: Chrome, Shatter, Dust, Hologram, Wire, Iridescent. The model holds the angle you set. |
| Neon Tunnel, Ink Flow, Kaleido Bloom, Liquid Metal, Fractal Dive, Spectrum | Generative scenes that need no source file. |

**Images:** PNG, JPG, GIF (first frame), WebP. Drop a file on the window or use "Load image...".

**Models:** GLB, glTF, FBX, OBJ, STL, PLY (including point clouds). Drop a file on the window or use "Load model...". Models are centred and scaled automatically; missing normals are generated. Drag the model to turn it, scroll to zoom.

## Controls

- **Sensitivity / Smoothing** - how hard and how quickly the visuals respond. Levels are auto-ranged, so quiet and loud tracks both work.
- **Intensity** - how far the music is allowed to push the picture.
- **Speed, Zoom, Trails, Glow, Hue, Saturation, Brightness** - the overall look.
- **Turn / Tilt / Spin** - model angle and optional turntable rotation.
- **Hide controls** - gives the whole window to the picture. Double-click the picture to bring them back.

Every control is a host parameter, so it can be automated or mapped in Live. Loaded files and window size are saved with the Live set.

## Effects

The **FX** page (button at the top of the panel) holds effects that are laid over whatever the scene draws. They are made for a logo or a 3D model sitting on black, and they run in time with the music.

| Effect | What it does |
| --- | --- |
| Strobe | Cuts the picture in and out: on for the first half of each cycle, off for the second. |
| Pulse | Full brightness at the start of each cycle, fading until the next. |
| Color Shift | Moves the picture through the colours. White and grey areas are tinted with the colour, so it works on an all-white logo; coloured areas have their hue turned. Color mode: Cycle glides through the colours once per cycle, Step jumps to a new colour at the start of each cycle. |
| Zoom Punch | Kicks the picture larger at the start of each cycle. |
| Shake | Jolts the picture at the start of each cycle. |
| Swing | Rocks the picture left and right. |
| RGB Split | Pulls red and blue apart. |
| Glitch | Slices of the picture jump sideways. |
| Glow Pump | Pumps the glow around bright parts. |
| Pixelate | Breaks the picture into blocks that resolve. |

Each effect has an amount (0 is off) and a rate from 8 bars down to 1/32, with a bar counted as four quarter notes.

**Switches.** The square in front of each effect's name turns that effect on or off, and the **On** button beside the EFFECTS heading turns all of them on or off at once. A switch does not touch the amount, so an effect comes back exactly as it was set. An effect that is off is shown dimmed and can still be adjusted. `docs/effect-switches.png` shows the page with Pulse and Color Shift switched off.

The switches are host parameters like everything else: "FX On" for all the effects, and "FX Strobe On", "FX Pulse On" and so on for each one. In Live they automate as on/off steps, so a strobe can be cut in for one bar without drawing its amount up and down. All switches are on by default, so a project saved with an earlier version looks the same.

**Sync** chooses the clock. With *Host tempo*, effects follow the host's tempo and bar position, so a quarter-note strobe lands on the beat; while the host is stopped they keep running at its tempo. With *Free*, they run at the BPM set beside it, whatever the host is doing. The standalone app has no host tempo and always uses the Free BPM.

Every effect switch, amount and rate, the sync choice, the free BPM and the color mode are host parameters, so all of it can be automated. Effects are applied after trails, so a strobe does not smear, and they are part of a recorded video: Strobe and Pulse make the picture transparent, not black.

For a logo that holds still under the effects, use the Image scene with Intensity at 0.

Flashing pictures can trigger seizures in people with photosensitive epilepsy. Keep fast strobes off anything shown to an audience without a warning.

## Recording a transparent video

1. Under RECORD, pick a size and frame rate and press **Record**. The button turns red and reads Cancel: the take is armed. The picture changes to the shape of the video, and dark checks show the parts that will be transparent.
2. Start playback in Live. Recording starts on the first audio block of playback and ends by itself when playback stops. If Live is already playing, it starts straight away. Pressing the button again (it reads Stop) also ends the take.
3. The file is saved as `Videos\Lumenwake\Lumenwake <date> <time>.mov`. **Folder** shows it.

The standalone app has no transport, so there Record starts at once and Stop ends the take.

What is in the file:

- QuickTime `.mov`, Animation codec, lossless, 32 bits per pixel with a straight alpha channel. Premiere Pro, After Effects, DaVinci Resolve, Final Cut and ffmpeg read it with the transparency intact.
- The audio that passed through the plugin, 16-bit stereo, lined up with the picture. Sample rates above 48 kHz are halved or quartered for the sound track.
- Black is transparent and light is as opaque as it is bright, so glows and trails blend over whatever is underneath. In the Model 3D scene the Chrome, Shatter and Iridescent surfaces stay solid, and the dark backdrop behind the model is left out.

Sizes: 1080p 30/60, 720p 30/60, vertical 1080 x 1920 and square 1080 x 1080.

Things to know:

- Keep the Lumenwake window open and on screen during a take. Frames are taken from what is drawn; closing the window ends the take and saves it.
- Lossless video is large. At 1080p 30, measured: about 1.6 GB a minute for a model, 2.5 GB for Spectrum, and about 10 GB a minute for scenes that fill the frame (Image, Neon Tunnel). 720p is under half of that; 60 fps is double. Record to an SSD.
- The take stops and saves itself when less than 300 MB is free on the drive.
- If the drive cannot keep up, the last picture is held for the frames that could not be written, and the status line counts them as "held".

To convert a take to ProRes 4444, which every editor accepts and which is about half the size for full-frame scenes:

```
ffmpeg -i "Lumenwake take.mov" -c:v prores_ks -profile:v 4444 -pix_fmt yuva444p10le -c:a copy out.mov
```

## Build (Windows)

Needs CMake 3.22+, Visual Studio with the C++ workload, and JUCE in `libs/JUCE`.

```
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target Lumenwake_VST3 Lumenwake_Standalone
```

Build from a short path (for example `C:\dev\Lumenwake`): deep folders run into the Windows 260-character path limit. Do not build through a junction or symlink to the source folder: Visual Studio then misses header changes and links stale object files.

## Mac, Audio Unit and VST2

**Mac (VST3 and Audio Unit).** A Mac plugin has to be compiled on a Mac; it cannot be made on Windows. The project is set up for it: on a Mac with Xcode and CMake, run

```
bash build-mac.sh
```

It fetches JUCE, builds `Lumenwake.vst3`, `Lumenwake.component` (the Audio Unit) and `Lumenwake.app` as universal binaries for Apple Silicon and Intel (macOS 11 or later), and puts them in `dist-mac/`. `bash build-mac.sh install` also copies the plugins into `~/Library/Audio/Plug-Ins` and runs Apple's `auval` check on the Audio Unit.

Without a Mac, `.github/workflows/build.yml` does the same on GitHub's Mac machines: push the project to a GitHub repository, open Actions, run "Build plugins", and download `Lumenwake-mac` from the finished run. The "What to build" choice on that button also builds the Windows packages there.

What has been checked on GitHub's Mac machines, for versions 0.2.0 and 0.3.0: all three build as universal binaries, the Audio Unit passes Apple's `auval`, the VST3 loads in the test host (`lw_hosttest`: pass-through, parameters, the effect switches from 0.3.0 on, state, editor), and the standalone app draws all 24 scenes and modes with no shader errors. The finished run holds a zip, `Lumenwake <version> (Mac).zip`, with the plugins, install notes (`packaging/README-mac.txt`) and licences. What has not been checked: the Mac build has not been used in Ableton Live or any other real host, by a person, on a real Mac, and recording has not been tried there.

Giving a Mac build to other people: macOS blocks plugins that are not signed with an Apple Developer ID and notarised (Apple Developer Program, paid yearly). Without that, each person has to clear the block once after copying the plugin, in Terminal:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Lumenwake.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Lumenwake.component
```

**VST2.** The older VST format (a `.dll` on Windows, a `.vst` on a Mac) can only be built with Steinberg's VST 2.4 SDK. JUCE does not include it, and Steinberg stopped licensing it in 2018, so it cannot be built or distributed here. If you already hold the SDK and a signed agreement from before then, add `-DLUMENWAKE_VST2_SDK=<path to the SDK>` to the cmake line and a `Lumenwake_VST` target appears. Ableton Live and every other current host load VST3.

## Releases

Every new minor or major version (0.2.0, 0.3.0, 1.0.0) gets a GitHub release with all the formats attached. To make one:

1. Set the version in `CMakeLists.txt` and add a section for it to `CHANGELOG.md`.
2. Push a tag named after the version: `git tag v0.3.0`, then `git push origin v0.3.0`.

"Build plugins" then builds both platforms and publishes the release, with that section of `CHANGELOG.md` as its notes and three files: `Lumenwake <version> (Mac).zip` (VST3, Audio Unit, standalone app), `Lumenwake <version> (Windows VST3).zip` and `Lumenwake <version> (Windows standalone).zip`. A tag that does not match the version in `CMakeLists.txt` stops the run before anything is built.

## Tests

Tests (`-DLUMENWAKE_BUILD_TESTS=ON`):

- `lw_modeltest file...` runs the model loaders and prints triangle counts.
- `lw_hosttest path\to\Lumenwake.vst3` loads the plugin the way a DAW does and checks pass-through, parameters, the effect switches, state and the editor. Add `--record 5` with the environment variables `LUMENWAKE_AUTOREC=1` and `LUMENWAKE_REC_DIR=<folder>` set, and it also plays the host transport (stopped, playing for 5 seconds, stopped) and checks that exactly one video appears. Parameters can be set for the take by name, with values from 0 to 1: `--record 5 "FX Strobe=1" "FX Strobe On=0"` records a take that must not strobe.
- `lw_rectest out.mov frames.raw audio.raw` writes a test clip with the video writer plus the raw frames and sound that went in. Decoding the clip with ffmpeg (`-f rawvideo -pix_fmt argb`) must give back `frames.raw` byte for byte.
- Setting the environment variable `LUMENWAKE_DUMP` to a folder and starting the standalone renders every scene and mode to PNG with a synthetic drum loop. `docs/sheet*.jpg` are from that run. With `LUMENWAKE_DUMP_ONLY=fx` it renders each effect instead, on the Image scene and on the model; `docs/effects.jpg` is from that run, on an all-white logo.

## Not included

- Video sources (MP4, MOV, WebM).
- A preset browser. Use Live's own device presets.
- A separate full-screen output window.
- Compressed video with alpha (ProRes 4444, WebM) straight from the plugin. Takes are lossless; convert them afterwards if you need a smaller file.
- Offline rendering. Takes are recorded live, so a scene too heavy for the graphics card to draw at the chosen frame rate records with held frames.
- Model textures and materials: each mode applies its own surface.
- Draco or meshopt compressed glTF.

## Third-party code

- [JUCE](https://juce.com) - plugin framework. Free for personal use under its own licence terms; check them before distributing.
- [ufbx](https://github.com/ufbx/ufbx) - FBX loading.
- [cgltf](https://github.com/jkuhlmann/cgltf) - glTF loading.
