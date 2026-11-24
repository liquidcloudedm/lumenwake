# Changes

Each version's section becomes the notes of its GitHub release, so write it for the people who use the plugin.

## 0.3.1

- Fixed recordings that showed a solid block of colour around the picture in place of its glow. Media players, and editors that read the clip's transparency as premultiplied, showed the block; the logo or scene itself was hidden under it.
- The video now stores the picture as it looks on black, with the transparency beside it (premultiplied alpha). It plays correctly in an ordinary player and overlays correctly in an editor.
- If a glow looks too dark over other footage, set the clip's alpha type to "premultiplied" in the editor.

## 0.3.0

- Every effect on the FX page has an on/off switch, and an On button beside the EFFECTS heading switches all of them at once.
- A switch leaves the effect's amount alone, so the effect comes back exactly as it was set.
- The switches can be automated in the host, where they are called "FX On", "FX Strobe On", "FX Pulse On" and so on.
- Projects saved with 0.2.0 open with every switch on and look the same as before.

## 0.2.0

- New FX page with ten effects laid over any scene: Strobe, Pulse, Color Shift, Zoom Punch, Shake, Swing, RGB Split, Glitch, Glow Pump and Pixelate.
- Effects follow the host's tempo and bar position, or run at a free BPM.
- Color Shift tints white and grey, so it works on an all-white logo.
- Every effect amount and rate can be automated.
- First Mac build: VST3, Audio Unit and standalone app.

## 0.1.0

- First version: Image and Model 3D scenes for your own pictures and 3D models, and six generative scenes.
- Record button that saves a transparent video when playback starts.
