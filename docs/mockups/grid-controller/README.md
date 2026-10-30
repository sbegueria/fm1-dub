# Grid controller mockup

The design behind DUB-1's Intech Grid pages (ui.c, "the Intech Grid"; the manual's "An Intech Grid: PBF4 and
EN16"): a canvas made with Claude's Design artifact, saved here as its source.

- `Grid.dc.html`: the prototype. DUB-1's screen beside a PBF4 and an EN16 drawn to scale (from Grid Editor's
  device layouts and Intech's renders), the cable in the PBF4; the controls work, GLO steps the pages,
  ALGORITHM switches the EN16 between Free and Tracks.
- `GridScreen.dc.html`: the screen component (the PBF4 page, the EN16 page in both layouts); `EN16Page.dc.html`,
  `EN16Tracks.dc.html`: still states of it.
- `Main.dc.html`, `Screen.dc.html`, `Learn.dc.html`, `PushTarget.dc.html`: the earlier EF44 study.
- `canvas.json`: the canvas's layout and pages.

The `.dc.html` files run in the Design canvas (they load its `support.js`); the published canvas is the place to
open them. The firmware follows the mockup except where the hardware and the tests asked otherwise: the Grid is
off until Song mode's KNOB 3 turns it on (the EN16's notes are a keyboard's too), and the CC knob's Learn sits at
0 of an ordinary knob, not left of a centred one.
