LUMENWAKE 0.3.1
An audio-reactive visualiser plugin. VST3, Windows 64-bit.

Put it on a track and the picture moves with whatever plays through it.
Audio passes through untouched.


WHAT YOU NEED
-------------
- Windows 10 or 11, 64-bit
- A host that loads VST3 plugins. It was made for Ableton Live 12.
- A graphics card with OpenGL 3.2 or newer (anything from the last ten years)


INSTALL
-------
1. Copy the whole "Lumenwake.vst3" folder from this zip into

       C:\Program Files\Common Files\VST3\

   Windows asks for administrator permission. Copy the folder itself, not
   just the file inside it. If an older Lumenwake is there, replace it.

2. In Ableton Live: Settings > Plug-Ins, turn on "Use VST3 Plug-In System
   Folders", then press Rescan.

3. Lumenwake appears under Plug-Ins > VST3 > Lumenwake. Drop it on an audio
   or instrument track and play something.

If the host does not list it: right-click the zip you downloaded, choose
Properties, tick "Unblock", press OK, then extract and copy again.

To remove it, delete that "Lumenwake.vst3" folder.


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
as on/off steps. Projects saved with 0.2.0 open with every switch on.

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
3. The video is saved in your Videos folder, in a folder called Lumenwake.
   The Folder button opens it.

The file is a QuickTime .mov with an alpha channel (Animation codec,
lossless), with the audio included. Black is transparent, so the clip can be
laid over other footage in a video editor. Effects are part of the video.

The colour is stored as it looks on black (premultiplied alpha), so the
clip also plays correctly in an ordinary media player. If a glow looks too
dark once the clip is laid over other footage, set the clip's alpha type to
"premultiplied" in your editor (Premiere Pro: Interpret Footage. DaVinci
Resolve: Clip Attributes).

Keep the Lumenwake window open while recording. The files are large: from
about 1.6 GB a minute to about 10 GB a minute at 1080p 30, depending on the
scene. 720p is under half of that.


KNOWN LIMITS
------------
- This package is Windows only: VST3, no VST2 or AAX.
- No video files as a source, and no preset browser.
- Colours and textures stored in a 3D model are ignored; each mode applies
  its own surface.
- Compressed glTF (Draco, meshopt) is not supported.
- This is an early version. If a scene misbehaves in your host, say so.


LICENCES
--------
Lumenwake is built with the JUCE framework and other open-source code.
See the "Licences" folder.
VST is a registered trademark of Steinberg Media Technologies GmbH.
