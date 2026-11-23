LUMENWAKE 0.3.0
An audio-reactive visualiser plugin for Mac. VST3 and Audio Unit.

Put it on a track and the picture moves with whatever plays through it.
Audio passes through untouched.


WHAT YOU NEED
-------------
- macOS 11 or later, Apple Silicon or Intel
- A host that loads VST3 or Audio Unit plugins: Ableton Live, Logic Pro,
  GarageBand, Reaper, Bitwig and so on


WHAT IS IN THIS FOLDER
----------------------
  Lumenwake.vst3        The VST3 plugin
  Lumenwake.component   The Audio Unit (Logic Pro and GarageBand need this one)
  Lumenwake.app         The same thing as an app that listens to an audio input
  Licences              Licences of the open-source code inside


INSTALL
-------
1. In Finder choose Go > Go to Folder and open

       ~/Library/Audio/Plug-Ins

2. Copy "Lumenwake.vst3" into the "VST3" folder there, and
   "Lumenwake.component" into the "Components" folder. Make either folder if
   it is missing. You only need the one your host uses; both do no harm.

3. Tell macOS the plugin is allowed to run. This build is not signed with an
   Apple developer certificate, so macOS blocks it until you do this once.
   Open the Terminal app, paste these two lines and press Return:

       xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Lumenwake.vst3
       xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Lumenwake.component

   Nothing is printed when it works.

4. Ableton Live: Settings > Plug-Ins, turn on "Use VST3 Plug-In System
   Folders" (and "Use Audio Units" if you want that one), then press Rescan.
   Lumenwake appears under Plug-Ins. Drop it on an audio or instrument track
   and play something.

   Logic Pro and GarageBand: restart the app. Lumenwake is under
   Audio FX > Audio Units > Lumenwake.

For the app, drag "Lumenwake.app" to Applications, then run this once:

       xattr -dr com.apple.quarantine /Applications/Lumenwake.app

It asks for the microphone the first time, because it listens to an audio
input.

To remove the plugin, delete those files again.


USING IT
--------
The panel has two pages, chosen with the Main and FX buttons at the top.

MAIN PAGE. SCENE picks what you see:

  Image          Your own picture, moved by the music. 12 modes.
                 PNG, JPG, GIF (first frame) or WebP.
  Model 3D       Your own 3D model, or the built-in knot. 6 modes.
                 GLB, glTF, FBX, OBJ, STL or PLY.
  Neon Tunnel, Ink Flow, Kaleido Bloom, Liquid Metal, Fractal Dive, Spectrum
                 Generative scenes that need no file.

Drop a picture or a model on the window to load it, or use the Load button.
Drag a model to turn it. Scroll to zoom.

  Sensitivity, Smoothing   How hard and how quickly the picture responds.
  Intensity                How far the music is allowed to push it.
  Speed, Zoom, Trails, Glow, Hue, Saturation, Brightness
                           The overall look.
  Turn, Tilt, Spin         Model angle and turntable rotation.
  Hide controls            Gives the whole window to the picture.
                           Double-click the picture to bring them back.

FX PAGE. Effects laid over whatever the scene draws, in time with the music.
They are made for a logo or a 3D model on black.

  Strobe        Cuts the picture in and out.
  Pulse         Full brightness at the start of each cycle, then fading.
  Color Shift   Moves the picture through the colours. White and grey are
                tinted, so it works on an all-white logo; colours have
                their hue turned. Color mode: Cycle glides, Step jumps to
                a new colour each cycle.
  Zoom Punch    Kicks the picture larger at the start of each cycle.
  Shake         Jolts the picture at the start of each cycle.
  Swing         Rocks the picture left and right.
  RGB Split     Pulls red and blue apart.
  Glitch        Slices of the picture jump sideways.
  Glow Pump     Pumps the glow around bright parts.
  Pixelate      Breaks the picture into blocks that resolve.

Each effect has an amount (all the way left is off) and a rate, from 8 bars
down to 1/32.

The square in front of each effect's name switches that effect on or off,
and the On button beside EFFECTS switches all of them at once. A switch
leaves the amount alone, so the effect comes back as it was set. In the host
the switches are called "FX On", "FX Strobe On" and so on, and they automate
as on/off steps.

SYNC chooses the clock. "Host tempo" follows the host's tempo and bar
position, so a 1/4 strobe lands on the beat. "Free" runs at the BPM set
beside it, whatever the host is doing.

For a logo that holds still under the effects, use the Image scene with
Intensity all the way down.

Every control on both pages can be automated or mapped in the host. Loaded
files and the window size are saved with the project.

Flashing pictures can trigger seizures in people with photosensitive
epilepsy. Keep fast strobes off anything shown to an audience without a
warning.


RECORDING A TRANSPARENT VIDEO
-----------------------------
1. Under RECORD, pick a size and press Record. The button turns red. Dark
   checks show the parts of the picture that will be transparent.
2. Start playback in the host. Recording starts with playback and saves
   itself when playback stops. Pressing the button again also stops it.
3. The video is saved in your Movies folder, in a folder called Lumenwake.
   The Folder button opens it.

The file is a QuickTime .mov with an alpha channel (Animation codec,
lossless), with the audio included. Black is transparent, so the clip can be
laid over other footage in a video editor. Effects are part of the video.

Keep the Lumenwake window open while recording. The files are large: from
about 1.6 GB a minute to about 10 GB a minute at 1080p 30, depending on the
scene. 720p is under half of that.


KNOWN LIMITS
------------
- The Mac version is new. It was built and checked on a build machine (it
  passes Apple's Audio Unit validation, loads in a test host and draws every
  scene), but it has not yet been used in a real session on a Mac. If
  something misbehaves in your host, say so.
- Not signed or notarised by Apple, hence step 3 above.
- VST3 and Audio Unit only: no VST2 or AAX.
- No video files as a source, and no preset browser.
- Colours and textures stored in a 3D model are ignored; each mode applies
  its own surface.
- Compressed glTF (Draco, meshopt) is not supported.


LICENCES
--------
Lumenwake is built with the JUCE framework and other open-source code.
See the "Licences" folder.
VST is a registered trademark of Steinberg Media Technologies GmbH.
