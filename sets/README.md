DUB-1 ready-made sets

Sets for DUB-1 1.5 or later on the M-VAVE FM-1. A set is a .dub1 file of pieces to play together, each piece whole
(its sound as it was made, and its name), with the set's own Mode, Transition, the sirens' knobs and Tune. The FM-1 comes with the first two below: DUB-1
1.5.0 stores them at its first start, into the first empty set numbers (usually 1 and 2). The third, The Pieces, brings
back DUB-1's own eight pieces.

How to use one:
- Hear it first in the emulator, in the browser: https://sbegueria.github.io/fm1-dub/emu/ ("Hear a set": pick one
  of these, or choose the file).
- Send it to the FM-1 from the Sets page, https://sbegueria.github.io/fm1-dub/sets/ (Chrome or Edge, the FM-1
  connected by USB and stopped): choose the file, pick a set number, Send, and answer GLO on the FM-1. Then Load it
  into your pieces, from the page or on the FM-1 itself (GLO twice, the Set page's KNOB 4, Load). Or from a computer: python3 tools/dub1_set.py send 1 sets/winter-set.dub1
- Loading a set replaces your pieces 1 onwards (a set of five: 1 to 5); back up first (the Sets page's "Back up
  everything").

The sets:

winter-set.dub1   "Winter Set", 5 pieces: Hoarfrost, Black Ice, Snowfield, Aurora, Thaw
    The five songs of the hour-long Winter Set video, after DeepChord presents Echospace (The Coldest Season, and
    Liumin to close). Set mode with Transition Mix: press PLAY and the set plays itself, about an hour, each song's
    outro mixed into the next. The video's performer moves (the DJ filter, the throws, the sirens) are yours to play,
    on the Arrangement page.

four-to-five.dub1   "4 to 5 a.m.", 4 pieces: Night Drive, Pressure, Fog, Haze
    The four pieces of the hour-long night video, "4 a.m. to 5 a.m.", in its order. They are DUB-1's own recipes as
    1.5 plays them (the video was made with 1.1.0, whose recipes sounded different). Endless: each piece plays until
    you change it, as the video does about every fifteen minutes.

the-pieces.dub1   "The Pieces", 8 pieces: Basic, Chamber, Steppers, Fog, Concrete, Night Drive, Pressure, Haze
    DUB-1's own eight pieces, as a new FM-1 has them, in their order. Load it to get the originals back after other
    sets have replaced them (it replaces all eight of your pieces: back up first). Endless, as a new FM-1.

How they were made: by DUB-1's host simulator (tools/make_sets.sh): the first two from the scripts of the videos
(tests/film/21-set.amb and tests/film/20-night.amb), so each piece is set as the video's script sets it; The Pieces
from a fresh simulator, its pieces as a new FM-1 boots with them. Each file was checked to load back the same, and
the first 30 seconds of each piece to sound the same from the file as from where it was made. GPL-3.0, as DUB-1.
