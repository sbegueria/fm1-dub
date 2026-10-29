/* SPDX-License-Identifier: GPL-3.0-only */
/* DUB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           plays / rests                   EDIT  tapped: the CHORDS page (eight slots: KNOB 1 the slot, the
 *                  keys its root, KNOB 2 its kind, KNOB 3 its type, KNOB 4 Play: Endless or Loop); held: the next chord
 *   REC            tapped: DUB HOLD (the echo repeats what it holds, the chord holds); held: the EFFECTS grid, played
 *                  on the white keys while they're held
 *   OCT- / OCT+    on a layer's page (Chord, Pings, Bass, Drums, Noise, and their sound pages): its level -5 / +5
 *                  (held: on); both together, quickly: the layer muted or back (held 5 s: the update mode)
 *   SEL            the ARRANGEMENT page (PERFORM's that was): the keys mute (white 1-6; held, a part's channel
 *                  strip), solo (black 1-5: the chord, pings, bass, hats, kick; held, that part thrown into the
 *                  echo) and, held, make the dub moves (white 9-12) and four effects (white 13-16; white 7-8 the gap);
 *                  black 8-11 the dub sirens (a tap a short one, held a slow one, its knobs on the knobs); the knobs Width, Wet, Duck; the
 *                  screen the parts' meters
 *   SELECT         the DJ filter, on every page (1.4.0; with HOME held: the tempo)   ALGORITHM the voice, the walk
 *                  (Chords), the noise's character, else the screen   PRESETS the piece
 *   FX ENV LFO GLO / HOME SAVE ARP SEQ   the pages: DUB, CHORD, BASS, SETUP / FORM, NOISE, PINGS, DRUMS (1.4.0's
 *                  names: Space was Dub, Dub Form)
 *   KNOB 1-4       the page's four values
 *   GLO held       saves (it also saves by itself a few seconds after a change, while resting);
 *                  held on 1.5 s: the piece back to its recipe */
#include "../dsp/fastmath.h"

/* the pages, one a layer (a button each, NPAGES; the Noise page, V_NSND, SAVE's); SELECT steps through them in the
 * panel's order, PAGE_ORDER below (the view numbers stay as they were: the tests use them) */
/* (past NPAGES, on no button of their own: V_MIX (1.4.0: gone, its number kept: the tests use them); the layers' sound
 * pages, the layer's button pressed again) */
enum { V_DUB, V_CHORD, V_TEXTURE, V_BASS, V_DRUMS, V_SPACE, V_SETUP, NPAGES, V_MIX = NPAGES, V_CSND, V_PSND, V_BSND,
       V_DSND, V_CFM, V_PFM, V_BFM, V_CHORDS, V_PAT, V_PERF, V_KEY, V_NSND, V_ESND, V_NSND2, V_CLFO, V_NLFO, V_HATS, V_NVOX,
       V_STRIP, V_SONG, V_SMODE, V_WALK, V_SIREN, NVIEWS };
/* (V_PERF: PERFORM's knobs, the Arrangement page; V_STRIP a part's white key held (V_MIX, white 6 held, gone).
 * V_SONG (1.4.0, the user's: song structure): HOME again, the piece's song (its Intro, Main, Outro, Kind), HOME's
 * second; V_SMODE: GLO again, Song mode (Endless, Song, Set), the Setup page's second (the player's, as Tune).
 * V_WALK (1.4.0, the user's): EDIT again, the Chords page's second: the circle of fifths, the walk's character and
 * Change. V_SIREN (format 34, the user's): a siren's key held in the Arrangement page, that siren's knobs (sir_page),
 * till it's let go. V_KEY: HOME again, the key and the tempo, the Form page's second. V_CLFO, V_NLFO (1.4.0): the chord's LFO, ENV a fourth time (after its FM page), and
 * the noise's, SAVE a third time; for now, till the big redesign) */
/* (1.4.0, the user's: the names; the identifiers kept: V_DUB the Form page, HOME's; V_SPACE the Dub page, FX's, the echo
 * and the plate; V_PERF the Arrangement page, SEL's, PERFORM's that was. The layers' pages: each its part, then its
 * voice, then FM: V_DSND the Kick page, V_HATS the hats' (SEQ a third time), V_NVOX the Noise voice (SAVE again)) */
static const char *const VIEW_NAME[NVIEWS] = {"Form", "Chord", "Pings", "Bass", "Drums", "Dub", "Setup", "Mix",
                                              "Chord voice", "Pings voice", "Bass voice", "Kick",
                                              "Chord FM", "Pings FM", "Bass FM", "Chords", "Pattern", "Arrangement", "Key", "Noise",
                                              "Echo", "Noise space", "Chord LFO", "Noise LFO", "Hats", "Noise voice",
                                              "Strip", "Song", "Song mode", "Walk", "Siren"};
#define B_NONE 0xFFu
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_ARP, B_LFO, B_SEQ, B_FX, B_GLO, B_NONE, B_NONE, B_NONE, B_NONE,
                                         B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_SAVE, B_NONE, B_NONE,
                                         B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE, B_NONE};
/* HOME's screen views (the stored numbers; 3 and 4 gone, 1.4.0): ui_screen_ok */
enum { SCR_RIPPLES = 0, SCR_SCOPE = 1, SCR_SPEC = 2, SCR_TAPE = 5 };
#define SCR_N 4
static const uint8_t SCR_ORDER[SCR_N] = {SCR_RIPPLES, SCR_SCOPE, SCR_SPEC, SCR_TAPE};
static int sound_of(int v) { return v == V_CHORD ? V_CSND : v == V_TEXTURE ? V_PSND : v == V_BASS ? V_BSND : v == V_DRUMS ? V_DSND : -1; }
static int fm_of(int v) { return v == V_CHORD ? V_CFM : v == V_TEXTURE ? V_PFM : v == V_BASS ? V_BFM : -1; }
static int parent_of(int v)
{
    return v == V_CSND || v == V_CFM || v == V_CLFO ? V_CHORD : v == V_PSND || v == V_PFM ? V_TEXTURE : v == V_BSND || v == V_BFM ? V_BASS
         : v == V_DSND || v == V_HATS || v == V_PAT ? V_DRUMS : -1;
}
static int noise_page(int v) { return v == V_NSND || v == V_NVOX || v == V_NSND2 || v == V_NLFO; }   /* (SAVE's) */
/* (1.4.0, the user's) the Arrangement page's channel strips: a part's white key held, V_STRIP, the knobs that part's
 * Level, Echo send, Plate send and Filter (a Low cut before format 28); the part (0..5: the chord, the pings, the bass, the kick, the hats (with
 * the rim and the clap), the noise: white 1-6, the meters' rows; the user's: the kick and the hats apart, the noise its
 * own key) */
static uint8_t strip_part;
static uint8_t sir_page;                         /* (format 34) the siren its page shows (EN_SIR_*) */
#define NSTRIP 6
static const char *const STRIP_NAME[NSTRIP] = {"Chord", "Pings", "Bass", "Kick", "Hats", "Noise"};
static int layer_of(int v) { v = parent_of(v) >= 0 ? parent_of(v) : v; return v == V_CHORD ? 0 : v == V_TEXTURE ? 1 : v == V_BASS ? 2 : 3; }
static int fm_off(int v) { return v >= V_CFM && v <= V_BFM && !en_voice_fm(v - V_CFM); }   /* an FM page, not FM */
static int fm_knob_off(int v, int i)             /* a knob there doing nothing: the voice not FM, or operators 3 and 4's
                                                  * Ratio (the pings' Upper) on a two-operator algorithm (1.4.0: the LFO
                                                  * its own page) */
{
    if (v == V_SMODE)                            /* (the user's: Transition, Set mode alone) */
        return i == 1 && proj.par[P_SMODE] != 2;
    if (v < V_CFM || v > V_BFM || ((v == V_CFM || v == V_BFM) && i == 3))
        return 0;                                /* (1.4.0, the user's: the chord's Colour, the bass's Bite: every voice's) */
    if (!en_voice_fm(v - V_CFM))
        return 1;
    return i == (v == V_PFM ? 3 : 1) && en_fm_alg(v - V_CFM) == 8;
}
static int lfo_page(int v) { return v == V_CLFO || v == V_NLFO; }   /* (1.4.0) the LFO pages */
static int lfo_knob(void)                        /* (1.4.0, the user's) the knob the chord's LFO moves: its Destination's */
{
    return proj.par[P_CLDEST] == EN_CLD_RESO ? P_CRES : proj.par[P_CLDEST] == EN_CLD_COLOUR ? P_CCOL : P_CCUT;
}
static int nlfo_knob(void)                       /* (1.4.0, the user's) the noise's: Tone's or Air's; the Filter has no
                                                  * knob (the Noise LFO page's trace shows it) */
{
    return proj.par[P_NLDEST] == EN_NLD_TONE ? P_NCOL : proj.par[P_NLDEST] == EN_NLD_AIR ? P_NROOM : -1;
}
static int lfo_on_knob(int k, float *now, float *reach)   /* an LFO moving knob k: where it is and how far it goes,
                                                  * as shares of the knob's sweep (the chord's in octaves of its
                                                  * Cutoff, 5.5 the sweep; the noise's a quarter a unit) */
{
    if (k == lfo_knob() && en_lfo_reach > 0.001f) {
        *now = en_lfo_now * (1.0f / 5.5f), *reach = en_lfo_reach * (1.0f / 5.5f);
        return 1;
    }
    if (k == nlfo_knob() && en_nlfo_reach > 0.001f) {
        *now = 0.25f * en_nlfo_now, *reach = 0.25f * en_nlfo_reach;
        return 1;
    }
    return 0;
}
/* (1.4.0, the user's: the dub mixer) a part's knob as the mixer has it now: its level or a send moved by the ride
 * (en_mx_db, en_mx_snd: dB), in the knob's own units, as its law has it (the chord's, the pings' and the bass's level
 * squared; the kick's and the hats' 0.24 dB a step; the noise's level and the sends straight) */
static float ride_knob(int part, int what, float v)
{
    const float db = what ? en_mx_snd[part][what - 1] : en_mx_db[part];
    if (!what && (part == 3 || part == 4))
        return v + db * (1.0f / 0.2408f);
    return v * fm_exp2f(db * ((!what && part <= 2) ? 1.0f / 12.041f : 1.0f / 6.0206f));
}
static int page_of(int v)                        /* a sound page: its layer's; the Echo page: Space's */
{
    return v == V_KEY || v == V_SONG ? V_DUB : v == V_SMODE ? V_SETUP : v == V_ESND ? V_SPACE : noise_page(v) ? V_NSND
         : parent_of(v) >= 0 ? parent_of(v) : v;
}
/* SELECT's round and the dots, as the buttons sit on the panel: FX ENV LFO GLO, then HOME SAVE ARP SEQ */
#define NROUND 8
static const uint8_t PAGE_ORDER[NROUND] = {V_SPACE, V_CHORD, V_BASS, V_SETUP, V_DUB, V_NSND, V_TEXTURE, V_DRUMS};
static int round_pos(int v)                      /* a page's place in the round (-1: none) */
{
    int i;
    for (i = 0; i < NROUND; i++)
        if (PAGE_ORDER[i] == v)
            return i;
    return -1;
}
/* the layer a page is (OCT- / OCT+: its level and its mute): the Chord, Pings, Bass, Drums and Noise pages and
 * their sound, FM and Pattern pages; -1 elsewhere (the Chords page too) */
#define NLAYER 5
#define NOCT 7                                   /* (OCT's: the five layers, then the Kick and the Hats strips') */
static int oct_layer(int v)
{
    static const int8_t SL[NSTRIP] = {0, 1, 2, 5, 6, 4};
    v = parent_of(v) >= 0 ? parent_of(v) : v;
    if (v == V_STRIP)                            /* (a strip: its part's, OCT- / OCT+ its level too: the kick's and the
                                                  * hats' their own, 5 and 6) */
        return SL[strip_part];
    return v == V_CHORD ? 0 : v == V_TEXTURE ? 1 : v == V_BASS ? 2 : v == V_DRUMS ? 3 : noise_page(v) ? 4 : -1;
}
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 200, K_CPU, K_NONE, K_SLOT, K_CQ, K_CX,    /* (the Chords page: the slot, and its kind and type) */
       K_LANE, K_LCH,                                        /* (the Pattern page: the lane, and its chance) */
       K_FILTER, K_DUB, K_ENERGY, K_SPACE,                   /* the master actions (EN_MAC_*), in that order */
       K_MODE,                                               /* (the Key page: the mode, as ALGORITHM sets it) */
       K_KSHAPE,                                             /* (1.4.0, the Drums page: the kick's shape: Four, Ghost, Half,
                                                              * One, P_KDENS's) */
       K_PLAY,                                               /* (the Chords page: Endless or Loop, P_PMODE's bit 2) */
       K_WALK,                                               /* (1.4.0, the Walk page: the walk's character, P_PMODE's
                                                              * bits 0-1: Home, Wander, Free) */
       K_SAVE,                                               /* (Setup: a turn saves; it shows whether it's saved) */
       K_VALUES,                                             /* (1.4.0, the user's: the knobs' numbers, Peek or Always:
                                                              * proj.values, global) */
       K_SPITCH, K_SRATE, K_SDEPTH, K_SECHO,                /* (format 34, the user's) the siren page's: the siren held's
                                                              * (sir_page) Pitch, Rate, Depth, Echo (EN_SK_*'s order;
                                                              * proj.sir, global) */
       K_TRANS };                                            /* (1.4.0, the user's: a Set's Transition, LP, Segue, Mix:
                                                              * proj.trans, global; Set mode alone) */
static const char *const LANE_NAME[EN_NDRUM] = {"Kick", "Shaker", "Open", "Rim", "Clap"};
_Static_assert(P_NPARAMS <= K_LEDS, "the knobs' own codes must come after the parameters");
static const uint16_t VIEW_KNOB[NVIEWS][4] = {
    {K_ENERGY, P_VARY, P_MOTION, P_MOVE},      /* (1.4.0, the user's: Energy here alone (the Arrangement page's
                                                  * until its Width); the Filter on SELECT only; the old Drift the Chord
                                                  * page's Change; format 30, the fourth Drift, how much the sound moves) */
    {P_STABS, P_CTHROW, P_DRIFT, P_CVARY},       /* (1.4.0, the user's: the chord's part: Change, how long a chord lasts
                                                  * (Drift, from the Form page), Vary; Wet to the channel strip, to come) */
    {P_DENSITY, P_PVARY, K_NONE, K_NONE},        /* (1.4.0, the user's: the layers' parts, the empty places kept: the
                                                  * pings' Wet to the channel strip, their Octave to their voice; Cloud
                                                  * to the Echo page, the plate's) */
    {P_GROOVE, P_BVARY, P_BNOTES, P_FIFTH},      /* (the user's: Groove, its Vary where the LFO was, Density, Fifth) */
    {K_KSHAPE, P_HITS, P_DVARY, K_NONE},         /* (Duck: the Arrangement page's; the kick a switch, 1.4.0; the hats'
                                                  * level and tone on the Hats page) */
    {P_ECHO, P_ETIME, P_FEEDBACK, P_REVERB},
    {P_TUNE, P_MIDI, K_LEDS, K_SAVE},
    {P_CHORD, P_PINGS, P_BASS, P_DRUMS},
    {P_CCUT, P_CRES, P_CATT, P_DECAY},           /* (1.4.0, the user's: the chord's voice, the filter first, its Cutoff
                                                  * around the piece's own; Colour on the FM page. Env, P_CENV, off the
                                                  * page: the voices and the recipes) */
    {P_POCT, P_PTON, P_PLEN, P_PBEND},           /* (1.4.0, the user's: the voices, each its register first) */
    {P_BASSOCT, P_BSHAPE, P_BLEN, P_BSLIDE},     /* (Bite to the FM page, every voice's) */
    {P_KTUNE, P_KLEN, P_KPUNCH, P_RUMBLE},       /* (the Kick page: the rumble back, the user's second try) */
    {P_CALG, P_CFRAT, P_CFB, P_CCOL},            /* (1.4.0: the chord's LFO its own page; the pings' and the bass's gone;
                                                  * the user's: Colour here, on every voice, the only one on a voice not FM) */
    {P_PALG, P_PRAT, P_PFB, P_PFRAT},            /* (the pings: Ratio the modulator's, Upper operators 3 and 4's) */
    {P_BALG, P_BFRAT, P_BFB, P_BBITE},           /* (Bite on every voice, the only one on a voice not FM) */
    {K_SLOT, K_CQ, K_CX, K_PLAY},
    {K_LANE, P_DACCENT, K_LCH, P_DMODE},
    {P_EWIDTH, K_NONE, K_DUB, P_DUCK},           /* (Wet: an offset for playing; Duck the piece's own. 1.4.0, the
                                                  * user's: the Filter on SELECT only; KNOB 2 the dub mixer, how much
                                                  * the desk rode itself, gone (format 30: its rides Form's Drift's;
                                                  * format 31, its moves Motion's): free;
                                                  * KNOB 1 the desk's Width, the whole mix's stereo field (from the
                                                  * Echo page), Energy on the Form page alone) */
    {P_TEMPO, P_ROOT, K_MODE, P_SWING},
    {P_NGRAIN, P_NVARY, K_NONE, K_NONE},         /* (the noise's part, its groove; the level on OCT- / OCT+, as the
                                                  * layers'; its Drift the recipes') */
    {P_EBLUR, P_ETONE, P_SIZE, P_CLOUD},         /* (1.4.0, the user's: the space together; Width to the Form page) */
    {P_NWIDTH, P_NPLATE, P_NECHO, K_NONE},       /* (the Noise page's third: SAVE again; its Change the recipes') */
    {P_CLTRIM, P_CLWAVE, P_CLRATE, P_CLDEST},   /* (1.4.0) the LFO pages: (format 30, the variety plan's step 2) its
                                                  * Drift on Form's Drift; (layout B, the user's) its Wave, the wave's
                                                  * share against the wander; its rate; where it goes (ALGORITHM's too) */
    {P_NLTRIM, P_NLWAVE, P_NLRATE, P_NLDEST},
    {P_HATSLVL, P_HTONE, P_HLEN, K_NONE},        /* (1.4.0) the Hats page: SEQ a third time, the Pattern after it */
    {P_NROOM, P_NCOL, P_NTUNE, P_NWIDTH},        /* (1.4.0) the Noise voice: SAVE again; its Width from the Noise space
                                                  * page (gone: its sends the noise's channel strip) */
    {P_CHORD, P_CECHO, P_CPLATE, P_CLOW},        /* (1.4.0) a strip: the chord's here; vknob() the part's (STRIP_PAR) */
    {P_SINTRO, P_SMAIN, P_SOUTRO, P_SKIND},      /* (1.4.0, the user's) the Song page: the song's lengths in its order,
                                                  * in bars (a phrase a click), and which parts come first */
    {P_SMODE, K_TRANS, K_NONE, K_VALUES},        /* (the Song mode page: GLO again; the player's, global; 1.4.0, the
                                                  * knobs' numbers, Values; the user's: KNOB 2 a Set's Transition,
                                                  * Set mode alone, Values on KNOB 4, KNOB 3 free) */
    {K_WALK, P_DRIFT, K_NONE, K_NONE},           /* (1.4.0, the user's) the Walk page: EDIT again; the walk's character
                                                  * (also on ALGORITHM, as ever) and Change, the Chord page's too */
    {K_SPITCH, K_SRATE, K_SDEPTH, K_SECHO},      /* (format 34, the user's) a siren's page: its key held */
};
static const uint16_t STRIP_PAR[NSTRIP][4] = {
    {P_CHORD, P_CECHO, P_CPLATE, P_CLOW}, {P_PINGS, P_PECHO, P_PPLATE, P_PLOW}, {P_BASS, P_BECHO, P_BPLATE, P_BLOW},
    {P_KLVL, P_KECHO, P_KPLATE, P_KLOW}, {P_HATSLVL, P_DECHO, P_DPLATE, P_DLOW}, {P_CRACKLE, P_NECHO, P_NPLATE, P_NLOW}};
/* (the Kick strip's level its own, x1 at 50; the Hats strip's the Hats page's Level, the shaker's and the open hat's
 * (the rim and the clap in its sends and its Filter, their level the kit's: P_DRUMS, the Drums pages' OCT, over both)) */
static int vknob(int v, int i)                   /* a page's knob i: VIEW_KNOB's, a strip's its part's */
{
    return v == V_STRIP ? STRIP_PAR[strip_part][i] : VIEW_KNOB[v][i];
}
static void page_pos(int v, int *n, int *m)     /* (1.4.0, the user's) a page's place among its button's: n of m (m 1: alone) */
{
    static const uint8_t SEQ[9][4] = {{V_DUB, V_SONG, V_KEY}, {V_CHORD, V_CSND, V_CFM, V_CLFO}, {V_TEXTURE, V_PSND, V_PFM},
                                      {V_BASS, V_BSND, V_BFM}, {V_DRUMS, V_DSND, V_HATS, V_PAT}, {V_SPACE, V_ESND},
                                      {V_NSND, V_NVOX, V_NLFO}, {V_SETUP, V_SMODE}, {V_CHORDS, V_WALK}};
    static const uint8_t LEN[9] = {3, 4, 3, 3, 4, 2, 3, 2, 2};   /* (1.4.0: HOME's Song page, GLO's Song mode, EDIT's Walk) */
    int s, i;
    for (s = 0; s < 9; s++)
        for (i = 0; i < LEN[s]; i++)
            if (SEQ[s][i] == v) {
                *n = i + 1, *m = LEN[s];
                return;
            }
    *n = *m = 1;
}
static const char *view_name(int v)              /* a page's name (a strip's: its part's, "Chord strip"; a siren's, "Wail
                                                  * siren") */
{
    static const char *const SN[NSTRIP] = {"Chord strip", "Pings strip", "Bass strip", "Kick strip", "Hats strip",
                                           "Noise strip"};
    static const char *const RN[EN_NSIR] = {"Wail siren", "Whoop siren", "Hi-lo siren", "Laser siren"};
    return v == V_STRIP ? SN[strip_part] : v == V_SIREN ? RN[sir_page % EN_NSIR] : VIEW_NAME[v];
}
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Energy", "Vary", "Motion", "Drift"},
    {"Density", "Throws", "Change", "Vary"},
    {"Density", "Vary", "", ""},
    {"Groove", "Vary", "Density", "Fifth"},
    {"Kick", "Hits", "Vary", ""},
    {"Echo", "Echo time", "Feedback", "Reverb"},
    {"Tune", "MIDI out", "Lights", "Save"},
    {"Chord", "Pings", "Bass", "Drums"},
    {"Cutoff", "Reso", "Attack", "Decay"},
    {"Octave", "Tone", "Length", "Bend"},
    {"Octave", "Shape", "Gate", "Glide"},        /* (1.4.0, the user's: the common words) */
    {"Tune", "Length", "Punch", "Rumble"},
    {"Algo", "Ratio", "Feedback", "Colour"},
    {"Algo", "Ratio", "Feedback", "Upper"},
    {"Algo", "Ratio", "Feedback", "Bite"},
    {"Slot", "Chord", "Type", "Play"},
    {"Lane", "Accent", "Chance", "Play"},
    {"Width", "", "Wet", "Duck"},
    {"Tempo", "Root", "Mode", "Swing"},
    {"Density", "Vary", "", ""},
    {"Blur", "Tone", "Size", "Cloud"},
    {"Width", "Plate", "Echo", ""},
    {"Drift", "Wave", "Rate", "Target"},
    {"Drift", "Wave", "Rate", "Target"},
    {"Level", "Tone", "Length", ""},
    {"Air", "Tone", "Decay", "Width"},
    {"Level", "Echo", "Plate", "Filter"},   /* (1.4.0, format 28, KellyOffski's: the Low cut a centred filter) */
    {"Intro", "Main", "Outro", "Kind"},
    {"Mode", "Transition", "", "Values"},
    {"Walk", "Change", "", ""},
    {"Pitch", "Rate", "Depth", "Echo"},
};
/* what each knob does, a line: under the big ring in the middle while it's being turned (each fits 228 px: the
 * host's expect helpfit) */
static const char *const KNOB_HELP[NVIEWS][4] = {
    {"Sparse and dry, to busy and bright", "How much the loop changes: held to restless",
     "How much the arrangement changes", "How much the sound moves: still to restless"},
    {"Stabs a bar, one to six", "How often a stab is thrown to the echo", "How long a chord lasts",
     "The stabs' turns, more or less; left: Hold"},
    {"How many notes each loop holds", "The loops' changes, more or less; left: Hold", "", ""},
    {"The pattern, from sparse to busy", "Its figure's changes, more or less; left: Hold", "How many of the groove's notes play",
     "How often the fifth, not the root"},
    {"The kick's shape, Vary its wander", "How busy the hats are", "The kick's and hats' changes; left: Hold", ""},
    {"How much goes into the echo", "The tape loop's length, in beats", "How long the repeats go on",
     "How much goes into the plate"},
    {"Fine tuning, in cents", "Notes and clock out over MIDI", "The keys' and buttons' lights",
     "A turn saves all eight pieces"},
    {"The chord's level", "The pings' level", "The bass's level", "The drums' level"},
    {"Darker, or brighter, than the piece's own", "The filter's resonance", "The strike: a snap, to a swell",
     "How long each stab rings"},
    {"The pings two octaves down to two up", "The ping, round to metallic", "How long each ping rings",
     "The pitch falling at the strike"},
    {"The bass an octave down or up", "A pure sine, to a square", "How long each note holds (its gate)",
     "How long it slides to the next"},
    {"The kick's pitch, in semitones", "How long the kick booms", "The kick's click and start pitch",
     "A dark roar under the kick"},
    {"Which operators modulate which", "Operators 3 and 4's pitch", "Op 4 on itself: pure to gritty",
     "The voice's own colour"},
    {"Which operators modulate which", "The modulator: harmonic to clangy", "Op 4 on itself: pure to gritty",
     "Operators 3 and 4's pitch"},
    {"Which operators modulate which", "Operators 3 and 4's pitch", "Op 4 on itself: pure to gritty",
     "Drive: clean to growling"},
    {"The slot you're writing", "Its kind (Empty: a gap)", "Triad, 6, 7, Maj7, 9, 11, Add9",
     "Endless, or the slots in a loop"},
    {"The lane you're writing", "How much quieter the other steps", "How likely the lane's steps play",
     "Auto patterns, or your own"},
    {"The whole mix mono, to wide", "", "The whole music dry, or drowned",
     "How far each beat pushes the rest down"},
    {"Beats a minute", "The key's root note", "The scale: Minor, Dorian, ...", "Off sixteenths late: the whole groove"},
    {"How many hits ring, in two bars", "Its groove's changes, more or less; left: Hold", "", ""},
    {"Clean repeats, to a dissolving wash", "The repeats darker, or brighter", "How long the plate rings",
     "The reverb through the grain cloud"},
    {"The noise mono, to wide", "How much of it rings in the plate", "How much of it goes into the echo", ""},
    {"More or less than Drift; left: still", "Wander to wave: 0 wander, 100 wave", "How long the wave: 1 to 32 bars",
     "What it moves: Cutoff, Reso, Colour"},
    {"More or less than Drift; left: still", "Wander to wave: 0 wander, 100 wave", "How long the wave: 1 to 32 bars",
     "What it moves: Filter, Tone, Air"},
    {"The hats' level against the kick", "The hats, dark to bright", "How long the hats ring", ""},
    {"The hiss under it, softer or more", "The rings darker, or brighter", "How long each hit rings on the chord",
     "The noise mono, to wide"},
    {"The part's level", "How much it sends into the echo", "How much it sends into the plate",
     "Left darker, right thinner, before the sends"},
    {"The parts coming in, a phrase a click", "The piece itself, before the outro", "The parts leaving, then silence",
     "Ambient first, or the kick and the bass"},
    {"Endless, a song, or a set of the pieces", "A set's hand-over: LP, Segue or Mix", "",
     "The knobs' numbers: shown a moment, or always"},
    {"Home, Wander or Free: how far it roams", "How long a chord lasts", "", ""},
    {"Up or down an octave", "How fast it goes round", "How far its pitch swings", "Into the echo; past half, repeats stack"},
};
/* (format 34) the siren page's help, each siren its own (the big ring's, as KNOB_HELP's) */
static const char *const SIR_HELP[EN_NSIR][4] = {
    {"Up or down an octave, on the key's notes", "One wail held, a note value (4 bars)",
     "Its top, on the key's notes (an octave)", "Into the echo; past half, repeats stack"},
    {"Up or down an octave, free: tune it by ear", "Whoops a second (a box's: 2.5)",
     "How far each whoop rises (an octave)", "Into the echo; past half, repeats stack"},
    {"Up or down an octave, on the key's notes", "One swap held, a note value (a quarter)",
     "Its other note, on the key's notes (the fifth)", "Into the echo; past half, repeats stack"},
    {"Where it lands, on the key's notes", "The warble, a second (6)", "How far it falls (two octaves)",
     "Into the echo; past half, repeats stack"},
};
/* (the user's) Transition's help, a line a choice (each fits 228 px in FONT_XS: the host's expect helpfit); and while
 * Song mode isn't Set */
static const char *const TRANS_HELP[EN_NTRANS] = {"Each song ends; a pause, then the next",
                                                  "The next song from the bar after the outro",
                                                  "Tempos glide; the next pulse under the echo"};
static const char *const TRANS_OFF = "Only in Set mode";

/* the screen's colours: a dark room, as a dub techno sleeve (grey on black, a cold blue). The chord and
 * the scale in teal, the pings in a paler blue, the bass in brass, the drums in amber; dub hold in red.
 * Each layer has a dark tint for the shapes at rest. */
#define K_BG RGB(16, 20, 26)
#define K_PANEL RGB(36, 44, 56)
#define K_LINE RGB(64, 76, 94)
#define K_DIM RGB(146, 160, 176)
#define K_TEXT RGB(222, 228, 232)
#define K_ACC RGB(92, 196, 204)          /* the accent: the page, the knob being turned, a message's word */
#define K_BELL RGB(128, 170, 224)
#define K_BELL_T RGB(54, 76, 112)
#define K_PAD RGB(84, 200, 186)
#define K_PAD_T RGB(40, 96, 98)
#define K_PULSE RGB(236, 150, 84)
#define K_PULSE_T RGB(106, 72, 50)
#define K_BASS RGB(204, 180, 112)
#define K_BASS_T RGB(98, 86, 58)
#define K_FROST RGB(232, 92, 84)         /* dub hold */
#define K_FROST_T RGB(128, 56, 52)
#define K_RIM RGB(214, 124, 176)         /* the rim */
#define K_RIM_T RGB(108, 58, 86)
#define K_CLAP RGB(232, 206, 98)         /* the clap */
#define K_CLAP_T RGB(108, 98, 48)
#define K_NOISE RGB(186, 174, 156)       /* the noise (its level ring) */

static struct {
    uint8_t view;
    uint8_t perf, perf_from;           /* PERFORM (SEL): the keys play the mutes and the moves, the knobs
                                        * the mix; the page it was started from */
    uint8_t strip_key, strip_used;     /* (1.4.0) a part's white key down (its index + 1; 0: none), a strip knob
                                        * turned while it is (its release then no mute) */
    uint32_t strip_t;                  /* when it went down */
    uint32_t mutes, moves;             /* the layers muted (EN_M_*), the moves held (EN_PF_*) */
    uint8_t bkey, bkey_thr;            /* (1.4.0) a black key down in PERFORM (its index + 1), its throw begun */
    uint32_t bkey_t;                   /* when it went down */
    uint8_t skey, skey_hold;           /* (1.4.0) a siren's key down in PERFORM (its index + 1), its hold begun */
    uint32_t skey_t;                   /* when it went down */
    uint32_t pfx;                      /* PERFORM's effects held (EN_FX_* bits), with the grid's */
    uint32_t solo;                     /* the layers soloed (EN_M_*): the others muted while any is */
    int8_t mac[EN_NMAC];               /* the master actions (HOME's Filter, PERFORM's knobs): -50..50, never saved */
    uint32_t fx;                       /* the effects held (EN_FX_*), REC's grid */
    uint8_t slot, chords_from;         /* the Chords page: the slot edited; the page it was opened from */
    uint8_t noise_from;                /* the Noise page (SAVE): the page it was opened from */
    uint8_t noise_round;               /* (1: SELECT brought it, and goes on from it) */
    uint8_t oct_both, oct_done;        /* OCT- and OCT+ down together this press; its mute done (or too late) */
    uint32_t oct_t, oct_rep;           /* when both went down; an OCT held: its next step */
    uint8_t oct_ring;                  /* the level ring in the middle: its layer + 1 (0: none) */
    uint32_t card_until, card_t0;      /* a nudge's card in the middle: up until, then fading; when it came */
    uint8_t card_shake;                /* the card a big move's (red: a shake, new chords, a reset) or not */
    uint16_t card_col;                 /* its colour */
    uint8_t lock;                      /* Lock (SEL held): each phrase keeps what it has */
    char card_head[20], card_text[40];   /* ("BACK TO THE MAIN": 16 and its end) */
    uint32_t oct_until, oct_pulse;     /* the ring: up until, then fading; its last step (a pulse) */
    uint32_t big_until;                /* (1.4.0) the big ring (a knob turning, SELECT's filter): up until, then fading */
    int16_t big_k;                     /* its knob (VIEW_KNOB's codes) */
    float big_v;                       /* its arc, gliding to the value */
    char big_name[16];                 /* its name, under it */
    const char *big_help;              /* what it does, under the name (KNOB_HELP's; 0: none) */
    char big_txt[16];                  /* (a value of no knob's: the screen's view) its text, and its place 0..1 */
    float big_tf;
    uint16_t big_col;                  /* its colour: the layer's, or the accent */
    float oct_v;                       /* the ring's arc, gliding to the level */
    uint32_t prog_gen;                 /* the engine's slots, as last taken (en_prog_gen) */
    int8_t cue_from;                   /* (1.4.0) a Set's Transition cued: the piece still ending (its outro, to the
                                        * downbeat), the header's name till then; -1 none */
    uint32_t q_drops;                  /* (1.4.1) the engine's dropped commands, as last seen (en_queue_drops) */
    uint32_t prog_from;                /* (1.4.0) and taken only from this command on (en_queue_w after a piece's regrow):
                                        * PRESET spun fast handed a piece the slots of the one passed through */
    uint8_t lane;                      /* the Pattern page: the lane edited */
    uint8_t grid_said;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t peek_until;                         /* (1.4.0) the knobs' numbers shown till then (a page opened) */
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
} ui;

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static char sp_buf[ANN_MAX + 1];                 /* (the screen reader's text, built below) */
static uint32_t sp_n;
static void sp_cat(const char *s);
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void caps(char *d, const char *s, int n);
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* (1.4.0, the user's: the value is the big ring's) the header shows the name alone; the reader hears it with its value */
static void say_name(const char *a, const char *b)
{
    say_as(a, 0, 0);
    if (ann_on()) {
        char t[48];
        msg_text(a, b, t);
        ann_say(t, 0, ANN_NOW);
        sp_said = 1;
    }
}
/* a long press's card in the middle (a nudge, a shake, the next chord, new chords, saved, a reset): what it did, in
 * big, outlined in its colour; it fades out CARD_SHOW after (draw_card) */
#define CARD_SHOW 800u                   /* (1.4.0, the user's: they distract; a second, fade and all, till the aids
                                          * can be turned off) */
#define CARD_FADE 200u
static int str_begins(const char *s, const char *p)   /* (the firmware's libc has no strcmp, strncmp) */
{
    while (*p && *s == *p)
        s++, p++;
    return !*p;
}
static int card_on(uint32_t now) { return ui.card_until && now < ui.card_until + CARD_FADE; }
static void card_show(const char *head, const char *text, uint16_t col, int big)
{
    int i;
    for (i = 0; head && head[i] && i < (int)sizeof ui.card_head - 1; i++)
        ui.card_head[i] = head[i];
    ui.card_head[i] = 0;
    for (i = 0; text && text[i] && i < (int)sizeof ui.card_text - 1; i++)
        ui.card_text[i] = text[i];
    ui.card_text[i] = 0;
    ui.card_col = col;
    ui.card_shake = (uint8_t)big;
    ui.card_t0 = plat_ms();
    ui.card_until = ui.card_t0 + CARD_SHOW;
}
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)proj.par[P_ROOT];
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
/* the modes as they're stepped through (ALGORITHM, the Key page's Mode): the most common first. (The values kept
 * are the engine's, MODE_*) */
static const uint8_t MODE_ORDER[EN_NMODES] = {MODE_AEOLIAN, MODE_IONIAN, MODE_DORIAN, MODE_MIXOLYDIAN, MODE_PENT_MIN,
                                              MODE_PENT_MAJ, MODE_PHRYGIAN, MODE_LYDIAN, MODE_HIRAJOSHI, MODE_IN,
                                              MODE_IWATO, MODE_YO, MODE_INSEN};   /* (1.4.0, KellyOffski's: the Japanese last) */
static int mode_pos(int m)
{
    int i;
    for (i = 0; i < EN_NMODES && MODE_ORDER[i] != m; i++)
        ;
    return i < EN_NMODES ? i : 0;
}
/* the Chords page's slots (amb.h): written (a kind), the walk's (no kind, a degree + 1 as the root) or a gap */
static int slot_is_walk(int i) { return !proj.par[P_CQ0 + i] && proj.par[P_CR0 + i]; }
static int slot_is_gap(int i) { return !proj.par[P_CQ0 + i] && !proj.par[P_CR0 + i]; }
static int scale_steps(void)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(proj.pool >> i & 1u);
    return n ? n : 1;
}
static void walk_written(int i, int *r, int *q, int *x)   /* a walk slot's chord, as it would be written */
{
    en_walk_chord(proj.pool, proj.par[P_ROOT], (proj.par[P_CR0 + i] - 1) % scale_steps(), r, q, x);
}
static void knob_set(int k, int v);
static void slot_write(int i)                    /* a walk slot about to be edited: written as it sounds */
{
    int r, q, x;
    if (!slot_is_walk(i))
        return;
    walk_written(i, &r, &q, &x);
    knob_set(P_CR0 + i, r);
    knob_set(P_CX0 + i, x);
    knob_set(P_CQ0 + i, q);
}
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    if (k == K_SLOT)
        return ui.slot;
    if (k == K_CQ || k == K_CX) {
        if (slot_is_walk(ui.slot)) {             /* the walk's: its chord as it would be written */
            int r, q, x;
            walk_written(ui.slot, &r, &q, &x);
            return k == K_CQ ? q : x;
        }
        return proj.par[(k == K_CQ ? P_CQ0 : P_CX0) + ui.slot];
    }
    if (k == K_LANE)
        return ui.lane;
    if (k == K_LCH)
        return proj.par[P_DCH0 + ui.lane];
    if (k >= K_FILTER && k <= K_SPACE)
        return ui.mac[k - K_FILTER];
    if (k == K_MODE)
        return mode_pos(proj.par[P_MODE]);
    if (k == K_KSHAPE) {                         /* (the user's order, four on the floor first: Four, Ghost, Half, One:
                                                  * P_KDENS 100, 75, 50, 25, the kick thinning) */
        static const int8_t POS[5] = {3, 3, 2, 1, 0};   /* (by P_KDENS to the nearest 25: 0, 25, 50, 75, 100) */
        int q = (proj.par[P_KDENS] + 12) / 25;
        return POS[q < 0 ? 0 : q > 4 ? 4 : q];
    }
    if (k == K_PLAY)
        return proj.par[P_PMODE] & EN_PLOOP ? 1 : 0;
    if (k == K_WALK)
        return (proj.par[P_PMODE] & 3) % EN_NWALK;
    if (k == K_SAVE)
        return ui.dirty ? 1 : 0;
    if (k == K_VALUES)
        return proj.values ? 1 : 0;
    if (k >= K_SPITCH && k <= K_SECHO)          /* (format 34: as it plays: the tuned ones on the key's notes) */
        return en_sir_eff(sir_page, k - K_SPITCH, proj.sir[sir_page % EN_NSIR][k - K_SPITCH]);
    if (k == K_TRANS)
        return proj.trans < EN_NTRANS ? proj.trans : 0;
    return 0;
}
static void knob_range(int k, int *lo, int *hi)
{
    if (k >= K_SPITCH && k <= K_SECHO) {
        en_sir_range(sir_page, k - K_SPITCH, lo, hi);
    } else if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = en_param_info(k)->hi;
    } else if (k >= K_FILTER && k <= K_SPACE) {
        *lo = -50;
        *hi = 50;
    } else {
        *lo = 0;
        *hi = k == K_PLAY || k == K_VALUES ? 1 : k == K_TRANS ? EN_NTRANS - 1 : k == K_WALK ? EN_NWALK - 1 : k == K_KSHAPE ? 3 : k == K_MODE ? EN_NMODES - 1 : k == K_LEDS ? 2 : k == K_CPU ? 100 : k == K_SLOT ? EN_NPROG - 1 : k == K_CQ ? EN_NCQ - 1 : k == K_CX ? EN_NCX - 1
            : k == K_LANE ? EN_NDRUM - 1 : k == K_LCH ? 100 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : k == K_SLOT ? "Slot" : k == K_CQ ? "Chord" : k == K_CX ? "Type"
         : k == K_LANE ? "Lane" : k == K_LCH ? "Chance" : k == K_FILTER ? "Filter" : k == K_DUB ? "Wet"
         : k == K_ENERGY ? "Energy" : k == K_SPACE ? "Duck" : k == K_MODE ? "Mode" : k == K_KSHAPE ? "Kick" : k == K_PLAY ? "Play" : k == K_WALK ? "Walk" : k == K_SAVE ? "Save" : k == K_VALUES ? "Values"
         : k == K_SPITCH ? "Pitch" : k == K_SRATE ? "Rate" : k == K_SDEPTH ? "Depth" : k == K_SECHO ? "Echo" : k == K_TRANS ? "Transition" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_SLOT) {
        itoa_u((uint32_t)v + 1u, b);
    } else if (k == K_CQ || k == K_CX) {
        en_param_text((k == K_CQ ? P_CQ0 : P_CX0) + ui.slot, v, b);
    } else if (k == K_LANE) {
        const char *n = LANE_NAME[(unsigned)v < EN_NDRUM ? v : 0];
        while ((*b++ = *n++))
            ;
    } else if (k == K_LCH) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else if (k == K_FILTER) {                  /* Off in the middle; Shut (the low-pass) down, Open up, 0..100 */
        const char *n = v < 0 ? "Shut " : "Open ";
        if (!v) {
            n = "Off";
            while ((*b++ = *n++))
                ;
            return;
        }
        while (*n)
            *b++ = *n++;
        itoa_u((uint32_t)(v < 0 ? -v : v) * 2u, b);
    } else if (k >= K_DUB && k <= K_SPACE) {     /* 0: the piece as it is; more or less of it, -100..+100 */
        if (v)
            *b++ = v < 0 ? '-' : '+';
        itoa_u((uint32_t)(v < 0 ? -v : v) * 2u, b);
    } else if (k == K_MODE) {
        const char *n = mode_name();
        while ((*b++ = *n++))
            ;
    } else if (k == K_KSHAPE) {
        static const char *const KN[4] = {"Four", "Ghost", "Half", "One"};
        const char *n = KN[(unsigned)v < 4u ? v : 3];
        while ((*b++ = *n++))
            ;
    } else if (k == K_WALK) {
        const char *n = EN_WALK_NAME[(unsigned)v < EN_NWALK ? v : 0];
        while ((*b++ = *n++))
            ;
    } else if (k == K_PLAY) {
        const char *n = v ? "Loop" : "Endless";
        while ((*b++ = *n++))
            ;
    } else if (k == K_SAVE) {                    /* changed since the last save: "Save" (turn it); else "Saved" */
        const char *n = v ? "Save" : "Saved";
        while ((*b++ = *n++))
            ;
    } else if (k == K_VALUES) {
        const char *n = v ? "Always" : "Peek";
        while ((*b++ = *n++))
            ;
    } else if (k >= K_SPITCH && k <= K_SECHO) {   /* (format 34: "+3", "1/8", "2.5 Hz", "12 st", "70") */
        en_sir_text(sir_page, k - K_SPITCH, v, b);
    } else if (k == K_TRANS) {
        const char *n = EN_TRANS_NAME[(unsigned)v < EN_NTRANS ? v : 0];
        while ((*b++ = *n++))
            ;
    } else {
        b[0] = 0;
    }
}

static void set_mode(int m);
static void do_save(void)                       /* GLO held, or Setup's KNOB 4 turned */
{
    if (project_save() == 0) {
        ui.dirty = 0;
        say("SAVED", 0);
    } else {
        warn("NOT SAVED", "FLASH ERROR");
    }
}
static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        int was = proj.par[k];
        proj.par[k] = (int16_t)v;
        en_set(k, v);
        if (k == P_PMODE && (was & EN_PLOOP) && !(v & EN_PLOOP))
            en_prog_rotate();                    /* (from Loop, by hand: the slot playing up to slot 1) */
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    } else if (k == K_VALUES) {
        proj.values = (uint8_t)v;
    } else if (k >= K_SPITCH && k <= K_SECHO) {   /* (format 34) the siren held's: kept for its next tap or hold, saved;
                                                  * the tuned ones' Pitch and Depth a note of the key a click (not past
                                                  * 0: the notch) */
        const int sk = k - K_SPITCH, was = knob_get(k);
        int d = v - was, n = was;
        while (d) {
            const int m = en_sir_step(sir_page, sk, n, d > 0 ? 1 : -1);
            if ((was > 0 && m < 0) || (was < 0 && m > 0) || m == n) {
                n = (was > 0 && m < 0) || (was < 0 && m > 0) ? 0 : n;
                break;
            }
            n = m, d += d > 0 ? -1 : 1;
        }
        proj.sir[sir_page % EN_NSIR][sk] = (int8_t)n;
        en_sir_set(sir_page, sk, n);
    } else if (k == K_TRANS) {
        proj.trans = (uint8_t)v;                 /* (the engine told by set_tick) */
    } else if (k == K_SLOT) {
        ui.slot = (uint8_t)v;
        return;                                  /* (only where the page looks: nothing saved) */
    } else if (k == K_CQ || k == K_CX) {
        slot_write(ui.slot);
        knob_set((k == K_CQ ? P_CQ0 : P_CX0) + ui.slot, v);
        if (k == K_CQ && !v) {                   /* Empty: a gap (the chord before it held) */
            knob_set(P_CR0 + ui.slot, 0);
            knob_set(P_CX0 + ui.slot, 0);
        }
        return;
    } else if (k == K_LANE) {
        ui.lane = (uint8_t)v;
        return;
    } else if (k == K_LCH) {
        knob_set(P_DCH0 + ui.lane, v);
        return;
    } else if (k >= K_FILTER && k <= K_SPACE) {
        ui.mac[k - K_FILTER] = (int8_t)v;
        en_macro(k - K_FILTER, v);
        return;                                  /* (for playing: nothing saved) */
    } else if (k == K_MODE) {                    /* (the knob: the place in MODE_ORDER) */
        set_mode(MODE_ORDER[v]);
        return;
    } else if (k == K_KSHAPE) {                  /* (the kick's shape: P_KDENS 25 .. 100) */
        static const int16_t KV[4] = {100, 75, 50, 25};
        knob_set(P_KDENS, KV[(unsigned)v < 4u ? v : 0]);
        return;
    } else if (k == K_PLAY) {                    /* (the walk's character kept) */
        knob_set(P_PMODE, (proj.par[P_PMODE] & 3) | (v ? EN_PLOOP : 0));
        return;
    } else if (k == K_WALK) {                    /* (Loop kept; its chords walked again at once, as ALGORITHM does) */
        knob_set(P_PMODE, (proj.par[P_PMODE] & EN_PLOOP) | v);
        en_prog_rewalk();
        return;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    if (k == K_SAVE) {                           /* a turn, either way: saved */
        do_save();
        return;
    }
    knob_range(k, &lo, &hi);
    if (hi - lo < 10 && !accel_off) {            /* (1.4.0, the user's) a switch: one position a click, the encoder's
                                                  * double counts (a click read as two) within 35 ms as one */
        static uint32_t sw_t[16];
        const uint32_t now = plat_ms();
        if (now - sw_t[role & 15] < 35u)
            return;
        sw_t[role & 15] = now;
        knob_set(k, knob_get(k) + (e > 0 ? 1 : -1));
        return;
    }
    if (lo < 0 && hi > 0 && hi - lo >= 10) {     /* (1.3.0) a centred knob: a notch at 0, three clicks to leave it */
#define NOTCH 2                                  /* (the clicks out of 0 held back) */
        static int8_t held[16];
        static uint32_t held_t[16];
        int v = knob_get(k), n, r = role & 15;
        if (v == 0) {                            /* (clicks out of 0 held back, NOTCH of them, one way, within 3 s of
                                                  * each other: a slow ride, a click a second, leaves it; a stray one
                                                  * doesn't) */
            int32_t a = e > 0 ? e : -e, dir = e > 0 ? 1 : -1;
            if (plat_ms() - held_t[r] > 3000u || (held[r] > 0) != (dir > 0))
                held[r] = 0;
            held_t[r] = plat_ms();
            while (a > 0 && (held[r] > 0 ? held[r] : -held[r]) < NOTCH)
                held[r] = (int8_t)(held[r] + dir), a--;
            if (!a)
                return;
            e = dir * a;
            held[r] = 0;
        }
        n = v + accel(role, e, hi - lo);
        knob_set(k, (v > 0 && n < 0) || (v < 0 && n > 0) ? 0 : n);   /* (and it stops at 0 on the way through) */
        return;
    }
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - proj.par[P_ROOT] + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(HOME, KEY)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

/* the voices: ALGORITHM on a layer's page steps through that layer's (round: after the last, the first) */
static int view_voice(int v)
{
    v = page_of(v);
    return v == V_CHORD ? P_CVOICE : v == V_TEXTURE ? P_PVOICE : v == V_BASS ? P_BVOICE : v == V_DRUMS ? P_KIT : -1;
}
static void set_voice(int p, int v)
{
    const en_param_t *pi = en_param_info(p);
    char t[12];
    int n = pi->hi - pi->lo + 1;
    v = ((v - pi->lo) % n + n) % n + pi->lo;
    proj.par[p] = (int16_t)v;
    en_set(p, v);
    en_param_text(p, v, t);
    say_name(p == P_KIT ? "KIT" : "VOICE", t);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    en_seed(plat_ms());
    en_regrow();
    ui.prog_from = en_queue_w();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

/* ----------------------------------------------------------- PERFORM --- */
/* SEL's mode, on the keys from F3 up (1.4.0, the user's: six parts, the noise its own key; an empty white key between
 * the parts and the moves, to tell them apart; the Mix, white 6 held, gone: each part's Level its strip's. The moves
 * rethought: a throw a part, on its black key held; the whites after the gap the global moves, then five of the
 * grid's effects):
 *   white 1-6    the parts, a tap each: muted and back, their levels kept (chord, pings, bass, kick, hats, noise: the
 *                mutes OCT- + OCT+ make on a layer's page, the same); held, the part's channel strip on the knobs
 *   black 1-5    the chord (F#3), the pings, the bass (A#3), the hats (C#4): each the part of the white key on its
 *                left; and (the user's, zzq: the kick rather than the noise) the kick (D#4), which has no black key
 *                of its own (B3, its white, has none after it); the noise none (white 6 still mutes it). A tap:
 *                soloed and back (more than one: they play together; the rest muted); held: that part thrown into
 *                the echo while held (the chord every stab thrown; the others their echo send pushed, a little more
 *                feedback), the tails left to ring
 *   white 7-8    none: dark, the gap (1.4.0, the user's: two keys, the Siren gone, the moves and the effects one
 *                key up)
 *   white 9-12   the global moves, held as long as the key is: the filter pushed open, pushed shut, the floor dropped
 *                (the kick and the bass out), the echo pushed (all in, feeding back)
 *   white 13-16  four of the grid's effects (REC held), held as long as the key is: Time x2, Freeze, Tape stop,
 *                Stutter
 *   black 8-11   (the user's) the dub sirens, one voice (a new one takes over), tuned to the key, into the echo:
 *                Wail (A#4), Whoop (C#5), Hi-lo (D#5), Laser (F#5). A tap: a short one, about a bar, played to its
 *                end; held past a third of a second: the slow one, from where it is, looping till let go (then it
 *                fades, the echo ringing on). Wail and Hi-lo follow the tempo, Whoop and the Laser don't; Whoop
 *                out of tune (a box's). (Format 34) held, its own page (V_SIREN): the knobs its Pitch, Rate, Depth
 *                and Echo while held, kept for its next tap or hold (proj.sir)
 * Each key that does something is lit: a part while it sounds, a solo (blinking while on), a move, an effect, a
 * throw, a siren (dark while held, or sounding). The rest do nothing (F#4, G#4: unlit); MIDI notes in still change
 * the scale */
#define PERF_NP 6                                /* the parts: 0..5; the moves 6..9; the effects 10..13; the blacks 14..18 */
#define PERF_NM 4
#define PERF_NF 4
#define PERF_NB 5
#define PERF_N (PERF_NP + PERF_NM + PERF_NF + PERF_NB)
#define PERF_F0 (PERF_NP + PERF_NM)
#define PERF_B0 (PERF_F0 + PERF_NF)
static const uint8_t PERF_KEY[PERF_N] = {0, 2, 4, 6, 7, 9,           /* white 1-6: the parts */
                                         14, 16, 18, 19,             /* white 9-12: the moves */
                                         21, 23, 24, 26,             /* white 13-16: the effects */
                                         1, 3, 5, 8, 10};            /* black 1-5: the parts' solos and throws */
static const char *const PERF_NAME[PERF_N] = {"CHORD", "PINGS", "BASS", "KICK", "HATS", "NOISE", "OPEN", "SHUT", "DROP",
                                              "ECHO", "TIME X2", "FREEZE", "TAPE STOP", "STUTTER", "CHORD",
                                              "PINGS", "BASS", "HATS", "KICK"};
static const uint16_t PERF_MBIT[PERF_NM] = {EN_PF_OPEN, EN_PF_CLOSE, EN_PF_KICKDROP, EN_PF_ECHO};
static const uint8_t PERF_FX[PERF_NF] = {1, 10, 2, 12};   /* (EN_FX_NAME's: Time x2, Freeze, Tape stop, Stutter) */
static const uint8_t PERF_BPART[PERF_NB] = {0, 1, 2, 4, 3};   /* each black key's part (EN_M_* bit; the last the kick's,
                                                  * the noise's before: the user's, zzq) */
static const uint16_t PERF_BTHR[PERF_NB] = {EN_PF_THROW, EN_PF_PTHROW, EN_PF_BTHROW, EN_PF_HATTHROW, EN_PF_KTHROW};
static const char *const PERF_THRNAME[PERF_NB] = {"CHORD THROW", "PINGS THROW", "BASS THROW", "HATS THROW", "KICK THROW"};
static const char *const PERF_THRWHAT[PERF_NB] = {"every stab into the echo", "the pings into the echo",
                                                  "the bass into the echo", "the hats into the echo",
                                                  "the kick into the echo"};
#define PERF_NS 4                                /* (the user's) the dub sirens: black 8-11 (EN_SIR_*'s order) */
static const uint8_t PERF_SKEY[PERF_NS] = {17, 20, 22, 25};   /* (A#4, C#5, D#5, F#5) */
static const char *const PERF_SNAME[PERF_NS] = {"WAIL", "WHOOP", "HI-LO", "LASER"};
static const char *const PERF_STAP[PERF_NS] = {"one bar", "a burst of whoops", "a bar of swaps", "one drop"};
static const char *const PERF_SHOLD[PERF_NS] = {"held: slow wails", "held: whoops, on and on",
                                                "held: slow swaps, rising", "held: a slow fall, again"};   /* (format
                                                  * 34: their rates the knobs') */
static void perf_mutes(void)                     /* to the engine: the mutes, and with a solo, all but it */
{
    en_mute(ui.mutes | (ui.solo ? 0x3Fu & ~ui.solo : 0u));
}
static int perf_sounds(int i)                    /* part i (0..5) heard: not muted, not soloed out */
{
    if (ui.solo && !(ui.solo >> i & 1u))
        return 0;
    return !(ui.mutes >> i & 1u);
}
static void set_view(int v);
static void lock_toggle(void)                    /* SEL held: Lock, on or off: the phrases keep their kick, groove and plan,
                                                  * the borrowed groove rests (not saved; off at boot) */
{
    ui.lock = (uint8_t)!ui.lock;
    en_lock(ui.lock);
    say("LOCK", ui.lock ? "ON" : "OFF");
    card_show("LOCK", ui.lock ? "the groove kept" : "the groove free", K_ACC, 0);
}
/* (1.4.0, the user's) PERFORM's keys said in the middle too, on a card: a mute, a solo, a move */
static const uint16_t PERF_COL[PERF_NP] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_TEXT, K_NOISE};
static const char *const PERF_WHAT[PERF_NM] = {"the filter pushed open", "the filter pushed shut",
                                               "the kick and the bass out", "the echo pushed, feeding back"};
static const char *const PERF_LOW[PERF_NP] = {"chord", "pings", "bass", "kick", "hats", "noise"};
static void perf_fx_send(void) { en_fx(ui.fx | ui.pfx); }   /* (the grid's and PERFORM's effects together) */
static void perf_solo(int b)                     /* a black key tapped: its part soloed, or not */
{
    const int p = PERF_BPART[b];
    const uint32_t m = 1u << p;
    ui.solo ^= m;
    perf_mutes();
    if (ui.solo & m) {
        say("SOLO", PERF_NAME[PERF_B0 + b]);
        card_show("SOLO", PERF_LOW[p], K_CLAP, 0);
    } else if (ui.solo) {                        /* (one let go, others still soloed) */
        say(PERF_NAME[PERF_B0 + b], "SOLO OFF");
        card_show(PERF_NAME[PERF_B0 + b], "solo off", K_CLAP, 0);
    } else {
        say("SOLO", "OFF");
        card_show("SOLO", "off: all heard", K_CLAP, 0);
    }
}
static void perf_throw(int b, int on)            /* a black key held: its part thrown into the echo; let go: no more */
{
    if (on)
        ui.moves |= PERF_BTHR[b];
    else
        ui.moves &= ~(uint32_t)PERF_BTHR[b];
    en_perf(ui.moves);
    if (on) {
        say(PERF_THRNAME[b], 0);
        card_show(PERF_THRNAME[b], PERF_THRWHAT[b], K_FROST, 0);
    }
}
static void perf_siren(int s, int down)          /* a siren's key: the short one at once; held, the slow one (strip_tick);
                                                  * let go: a held one fades, a tap's plays to its end */
{
    if (down) {
        if (ui.view == V_SIREN)                  /* (format 34: another takes over; its page, if held, its own) */
            set_view(V_PERF);
        ui.skey = (uint8_t)(s + 1);
        ui.skey_t = plat_ms();
        ui.skey_hold = 0;
        en_siren(s, EN_SIR_TAP);
        say(PERF_SNAME[s], 0);
        card_show(PERF_SNAME[s], PERF_STAP[s], K_FROST, 0);
        return;
    }
    if (ui.skey != s + 1)
        return;                                  /* (another took over: this one's already gone) */
    if (ui.skey_hold)
        en_siren(s, EN_SIR_OFF);
    ui.skey = 0, ui.skey_hold = 0;
    if (ui.view == V_SIREN)                      /* (format 34: its page gone, back to the Arrangement's) */
        set_view(V_PERF);
}
static void perf_key(uint32_t key, int down)
{
    int i;
    for (i = 0; i < PERF_NS && PERF_SKEY[i] != key; i++)
        ;
    if (i < PERF_NS) {
        perf_siren(i, down);
        return;
    }
    for (i = 0; i < PERF_N && PERF_KEY[i] != key; i++)
        ;
    if (i >= PERF_N)
        return;
    if (i >= PERF_B0) {                          /* a black key: a tap a solo, a hold a throw (decided by how long it is
                                                  * held: strip_tick starts the throw; the release a solo if none began) */
        const int bk = i - PERF_B0;
        if (down) {
            ui.bkey = (uint8_t)(bk + 1);
            ui.bkey_t = plat_ms();
            ui.bkey_thr = 0;
            return;
        }
        if (ui.bkey != bk + 1)
            return;
        ui.bkey = 0;
        if (ui.bkey_thr)
            perf_throw(bk, 0);
        else
            perf_solo(bk);
        ui.bkey_thr = 0;
        return;
    }
    if (i < PERF_NP) {                           /* the chord, pings, bass, kick, hats, noise: a tap, a mute (the levels
                                                  * kept); (1.4.0, the user's) held, the part's channel strip
                                                  * (strip_tick): the mute then on the key's release, if it was a tap */
        if (down) {
            ui.strip_key = (uint8_t)(i + 1);
            ui.strip_t = plat_ms();
            ui.strip_used = 0;
            return;
        }
        if (ui.strip_key != i + 1)
            return;                              /* (its press went elsewhere: SEL, or another key's strip) */
        ui.strip_key = 0;
        if (ui.view == V_STRIP) {                /* a hold: back to the page, no mute */
            set_view(V_PERF);
            return;
        }
        ui.mutes ^= 1u << i;                     /* (bit i: EN_M_CHORD .. EN_M_NOISE, in this order) */
        perf_mutes();
        say(PERF_NAME[i], ui.mutes >> i & 1u ? "OFF" : "ON");
        card_show(PERF_NAME[i], ui.mutes >> i & 1u ? "muted" : "on", ui.mutes >> i & 1u ? K_DIM : PERF_COL[i], 0);
        return;
    }
    if (i >= PERF_F0) {                          /* an effect, the grid's, held as long as the key is */
        const uint32_t m = 1u << PERF_FX[i - PERF_F0];
        if (down)
            ui.pfx |= m;
        else
            ui.pfx &= ~m;
        perf_fx_send();
        if (down) {
            say(PERF_NAME[i], 0);
            card_show(PERF_NAME[i], "held", K_FROST, 0);
        }
        return;
    }
    i -= PERF_NP;                                /* a move: its bit, EN_PF_OPEN .. EN_PF_ECHO */
    if (down)
        ui.moves |= PERF_MBIT[i];
    else
        ui.moves &= ~(uint32_t)PERF_MBIT[i];
    en_perf(ui.moves);
    if (down) {
        say(PERF_NAME[PERF_NP + i], 0);
        card_show(PERF_NAME[PERF_NP + i], PERF_WHAT[i], K_FROST, 0);
    }
}

static void set_view(int v);
#define KNB_PEEK 1500u                          /* (1.4.0) ms a page's knobs show their numbers as it opens */
#define STRIP_HOLD 300u                          /* ms a part's white key is held before its strip shows */
static void strip_open(void)                     /* the held key's part's strip on the knobs (white 1-6: the chord, the
                                                  * pings, the bass, the kick, the hats, the noise: the meters' rows) */
{
    strip_part = (uint8_t)(ui.strip_key - 1);
    set_view(V_STRIP);                           /* (the reader: the page, "Chord strip page: Level 52, ...") */
}
static void strip_tick(void)                     /* a part's key held long enough: its strip; a black key: its throw; a
                                                  * siren's: its slow one */
{
    if (ui.skey && !ui.skey_hold && ui.perf && plat_ms() - ui.skey_t >= STRIP_HOLD) {
        const int s = ui.skey - 1;
        ui.skey_hold = 1;
        en_siren(s, EN_SIR_HOLD);
        say(PERF_SNAME[s], "HELD");
        if (ui.view == V_PERF || ui.view == V_SIREN) {   /* (format 34, the user's) its page: the knobs ride it till
                                                  * it's let go (a strip open stays; the page itself says it: no card) */
            sir_page = (uint8_t)s;
            set_view(V_SIREN);
            ui.card_until = 0;
        } else {
            card_show(PERF_SNAME[s], PERF_SHOLD[s], K_FROST, 0);
        }
    }
    if (ui.strip_key && ui.perf && ui.view == V_PERF && plat_ms() - ui.strip_t >= STRIP_HOLD)
        strip_open();
    if (ui.bkey && !ui.bkey_thr && ui.perf && plat_ms() - ui.bkey_t >= STRIP_HOLD) {
        ui.bkey_thr = 1;
        perf_throw(ui.bkey - 1, 1);
    }
}
static void perf_set(int on)                     /* on: the mixer on the knobs; off: back to the page */
{
    ui.perf = (uint8_t)on;
    if (on) {
        if (ui.view != V_PERF && ui.view != V_STRIP && ui.view != V_SIREN)
            ui.perf_from = ui.view;
        set_view(V_PERF);
    } else {
        if (ui.moves) {                          /* the moves and throws let go; the mutes stay */
            ui.moves = 0;
            en_perf(0);
        }
        if (ui.pfx) {                            /* (its effects too; the grid's held on) */
            ui.pfx = 0;
            perf_fx_send();
        }
        ui.strip_key = 0;
        ui.bkey = 0, ui.bkey_thr = 0;
        if (ui.skey) {                           /* (a siren held: let go, it fades) */
            if (ui.skey_hold)
                en_siren(ui.skey - 1, EN_SIR_OFF);
            ui.skey = 0, ui.skey_hold = 0;
        }
        if (ui.view == V_PERF || ui.view == V_STRIP || ui.view == V_SIREN)
            set_view(ui.perf_from);
    }
    if (ann_on()) {                              /* (1.4.0, the user's: not on the screen, the page itself shows it; the
                                                  * reader still hears it) */
        char t[48];
        msg_text("ARRANGEMENT", on ? "ON" : "OFF", t);
        ann_say(t, 0, ANN_NOW);
        sp_said = 1;
    }
}

/* ------------------------------------------------------- the effects grid --- */
/* REC held: the white keys from F3 up are the sixteen effects (four groups of four: the echo, the filter,
 * the space, rhythm and noise), each as long as its key is held, even once REC is let go. A tap on REC alone
 * is still dub hold (REC's release does nothing once a key played) */
static const uint8_t FX_KEY[EN_NFX] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
#define GRID_HOLD 300u                   /* ms of REC held before the grid shows */
static int grid_up(void) { return ui.fx || ((ui.btn >> B_REC & 1u) && plat_ms() - ui.down_t[B_REC] >= GRID_HOLD); }
static int fx_key(uint32_t key, int down)        /* 1: the key was an effect's */
{
    int e;
    for (e = 0; e < EN_NFX && FX_KEY[e] != key; e++)
        ;
    if (e >= EN_NFX)
        return 0;
    if (down) {
        ui.fx |= 1u << e;
        ui.btn_used |= 1u << B_REC;              /* (REC's release: no dub hold now) */
        say(EN_FX_NAME[e], 0);
    } else {
        ui.fx &= ~(1u << e);
    }
    en_fx(ui.fx | ui.pfx);
    return 1;
}

/* ----------------------------------------------------------- the chords --- */
/* EDIT tapped: the Chords page, the eight coming: the walk's (teal: degrees, they follow the key), yours (white)
 * and gaps (the chord before held). KNOB 1 the slot, the keys its root (a walk's chord becomes yours, its kind and
 * type kept; a gap becomes a minor chord), KNOB 2 its kind (Empty: a gap), KNOB 3 its type, KNOB 4 Play:
 * Endless (slot 1 playing; on, all move up and the walk writes slot 8) or Loop (the slots in turn, to the last
 * that isn't a gap). The engine holds them (it writes the walk's): prog_take mirrors them into the project.
 * EDIT again, or any page's button: back. EDIT held: the next chord */
#define NEXT_HOLD 500u
static int prog_count(void)                      /* how far the slots run (Loop): to the last that isn't a gap */
{
    int i, n = 0;
    for (i = 0; i < EN_NPROG; i++)
        if (!slot_is_gap(i))
            n = i + 1;
    return n;
}
static const char *slot_label(int i, char *nm)   /* written: its name; the walk's: the chord the scale gives it */
{
    if (slot_is_walk(i)) {
        int pcs[EN_NPAD + 1], iv[4], t[4], d = (proj.par[P_CR0 + i] - 1) % scale_steps(), k;
        en_chord_tones(proj.pool, proj.par[P_ROOT], d, t);
        en_voicing(proj.pool, d, proj.par[P_CX0 + i] & 1, iv);
        pcs[0] = t[0];
        for (k = 0; k < EN_NPAD; k++)
            pcs[k + 1] = (t[0] + iv[k]) % 12;
        return en_chord_name(pcs, EN_NPAD + 1, nm);
    }
    return en_chord_label(proj.par[P_CR0 + i], proj.par[P_CQ0 + i], proj.par[P_CX0 + i], nm);
}
/* (1.4.0, the user's) a slot's chord as the stabs voice it: its four notes (MIDI, ascending; the root first, in C3..B3,
 * as the engine puts it), from its kind and type (yours) or the scale's degree (the walk's); a gap: the chord before
 * it, *held set (the one it holds); none before: 0 notes */
static int slot_notes(int i, int *nt, int *held)
{
    int iv[4], r, k, j = i;
    *held = 0;
    while (j >= 0 && slot_is_gap(j))
        j--, *held = 1;
    if (j < 0)
        return 0;
    if (slot_is_walk(j)) {
        int t[4], d = (proj.par[P_CR0 + j] - 1) % scale_steps();
        en_chord_tones(proj.pool, proj.par[P_ROOT], d, t);
        en_voicing(proj.pool, d, proj.par[P_CX0 + j] & 1, iv);
        r = t[0];
    } else {
        en_chord_iv(proj.par[P_CQ0 + j], proj.par[P_CX0 + j], iv);
        r = proj.par[P_CR0 + j];
    }
    r = ((r % 12) + 12) % 12;
    for (k = 0; k < 4; k++)
        nt[k] = 48 + r + iv[k];
    return 4;
}
static void prog_take(void)                      /* the engine's slots (the walk writes them, Endless moves them) */
{
    int i;
    if (en_prog_gen == ui.prog_gen || (int32_t)(en_prog_q - ui.prog_from) < 0)
        return;                                  /* (none new, or the piece's own still on its way) */
    ui.prog_gen = en_prog_gen;
    for (i = 0; i < EN_NPROG; i++) {
        proj.par[P_CR0 + i] = en_prog_slot[i][0];
        proj.par[P_CQ0 + i] = en_prog_slot[i][1];
        proj.par[P_CX0 + i] = en_prog_slot[i][2];
    }
}
static void set_view(int v);
static void slot_key(uint32_t key)               /* the Chords page: a key, the slot's root */
{
    char nm[16];
    slot_write(ui.slot);                         /* (the walk's: its kind and type kept, the root yours) */
    if (!proj.par[P_CQ0 + ui.slot])              /* (a gap: a Minor, its kind first: a root alone reads as the walk's
                                                  * degree, and K_CQ would write that degree's chord over it) */
        knob_set(P_CQ0 + ui.slot, EN_CQ_MINOR);
    knob_set(P_CR0 + ui.slot, (int)((5u + key) % 12u));
    say(slot_label(ui.slot, nm), 0);
}
static uint8_t edit_new;                         /* EDIT held on: the new progression made this hold */
static void edit_released(uint32_t held)
{
    if (edit_new) {                              /* (held on: done already) */
        edit_new = 0;
    } else if (held >= NEXT_HOLD) {              /* held: the next chord */
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        card_show("NEXT CHORD", en_playing ? "on the bar" : "", K_PULSE, 0);
    } else if (ui.view == V_CHORDS) {            /* tapped again: the Walk page (1.4.0, the user's), then back */
        set_view(V_WALK);
    } else if (ui.view == V_WALK) {
        set_view(V_CHORDS);
    } else {
        if (ui.perf) {                           /* (a page's button leaves PERFORM: the keys the scale again) */
            perf_set(0);
            set_view(V_DUB);
        }
        ui.chords_from = (uint8_t)ui.view;
        set_view(V_CHORDS);
    }
}

/* ---------------------------------------------------------- the pattern --- */
/* the Drums button a third time: the Pattern page. KNOB 1 the lane (kick, shaker, open hat, rim), the white keys
 * its sixteen steps (a key, a step: on or off), a black key held with a white one: that step accented; KNOB 2
 * the accents' strength, KNOB 3 the lane's chance, KNOB 4 Play: Auto (generated, as ever) or Pattern. The first
 * edit takes the pattern over from what Auto plays. The arrangement still takes lanes out (a Break, a fill) */
static const uint8_t STEP_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* the white keys */
static uint32_t lane_steps(int d) { return proj.par[P_DMODE] ? (uint32_t)(uint16_t)proj.par[P_DPAT0 + d] : en_drum_pat[d]; }
static void pattern_key(uint32_t key, uint32_t keys)
{
    int st, d = ui.lane, acc;
    uint32_t m, black = 0;
    char t[24];
    for (st = 0; st < 16 && STEP_KEY[st] != key; st++)
        ;
    if (st >= 16)
        return;                                  /* (a black key: an accent's, with a white one) */
    for (m = 0; m < 16; m++)
        keys &= ~(1u << STEP_KEY[m]);
    black = keys;
    if (!proj.par[P_DMODE]) {                    /* the first edit: Auto's pattern, taken over */
        int k;
        for (k = 0; k < EN_NDRUM; k++)
            knob_set(P_DPAT0 + k, (int)(int16_t)(uint16_t)en_drum_pat[k]);
        knob_set(P_DMODE, 1);
    }
    m = (uint32_t)(uint16_t)proj.par[P_DPAT0 + d];
    acc = (int)((uint32_t)(uint16_t)proj.par[P_DACC0 + d] >> st & 1u);
    if (black) {                                 /* accent it (on, too), or not */
        knob_set(P_DACC0 + d, (int)(int16_t)(uint16_t)((uint32_t)(uint16_t)proj.par[P_DACC0 + d] ^ (1u << st)));
        knob_set(P_DPAT0 + d, (int)(int16_t)(uint16_t)(m | 1u << st));
        acc = !acc;
    } else {
        knob_set(P_DPAT0 + d, (int)(int16_t)(uint16_t)(m ^ 1u << st));
    }
    t[0] = 0;
    itoa_u((uint32_t)st + 1u, t);
    say_as(LANE_NAME[d], 0, 0);
    if (ann_on()) {
        sp_n = 0;
        sp_buf[0] = 0;
        sp_cat("Step ");
        sp_cat(t);
        sp_cat(black ? (acc ? " accented" : " not accented") : (proj.par[P_DPAT0 + d] >> st & 1 ? " on" : " off"));
        ann_say(sp_buf, 0, ANN_NOW);
        sp_said = 1;
    }
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    if (v != ui.view) {
        ui.big_until = 0;                        /* (a new page: the last page's big ring gone) */
        ui.peek_until = plat_ms() + KNB_PEEK;    /* (and its knobs' numbers a moment: the encoders can't be read
                                                  * without turning them) */
    }
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    if (ui.perf && (b == B_HOME || b == B_ENV || b == B_ARP || b == B_LFO || b == B_SEQ || b == B_FX || b == B_GLO ||
                    b == B_SAVE || b == B_EDIT)) {   /* (1.3.0) a page's button leaves PERFORM: the keys the scale
                                                      * again, the page its first (SEL brings PERFORM back) */
        perf_set(0);
        set_view(V_DUB);
        if (b == B_HOME)
            return;
    }
    if (sound_of(ui.view) >= 0 && b == VIEW_BTN[ui.view]) {   /* the layer's button again: its sound page */
        set_view(sound_of(ui.view));
        return;
    }
    if (parent_of(ui.view) >= 0 && b == VIEW_BTN[parent_of(ui.view)]) {   /* again: an FM voice's FM page; then back */
        int pv = parent_of(ui.view);
        if (ui.view == sound_of(pv) && fm_of(pv) >= 0)   /* (on a voice that isn't FM, the page says so) */
            set_view(fm_of(pv));
        else if (ui.view == V_CFM)
            set_view(V_CLFO);                    /* (1.4.0) the chord: its LFO, then back */
        else if (ui.view == sound_of(pv) && pv == V_DRUMS)
            set_view(V_HATS);                    /* the drums: the hats (1.4.0), then their pattern */
        else if (ui.view == V_HATS)
            set_view(V_PAT);
        else
            set_view(pv);
        return;
    }
    if (b == B_FX && (ui.view == V_SPACE || ui.view == V_ESND)) {   /* FX again: the Echo page; again: back */
        set_view(ui.view == V_SPACE ? V_ESND : V_SPACE);
        return;
    }
    if (b == B_HOME && (ui.view == V_DUB || ui.view == V_SONG)) {   /* HOME again: the Song page (1.4.0), then the
                                                  * Key page (the key and the tempo); again: back */
        set_view(ui.view == V_DUB ? V_SONG : V_KEY);
        return;
    }
    if (b == B_GLO && ui.view == V_SETUP) {      /* (1.4.0) GLO again: Song mode; again: back */
        set_view(V_SMODE);
        return;
    }
    for (v = 0; v < NPAGES; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_SEL:                                  /* PERFORM: one page, so again stays (1.3.0: a page's button or
                                                  * SELECT leaves it) */
        if (!ui.perf)
            perf_set(1);
        else if (ui.view == V_STRIP || ui.view == V_SIREN)
            set_view(V_PERF);
        break;
    case B_EDIT:                                 /* (its release: edit_released, in input) */
        break;
    case B_PLAY:
        if (ui.btn >> B_HOME & 1u) {             /* (1.4.0, the user's) HOME held + PLAY: the outro from the next
                                                  * phrase, in any Song mode; again, in it or while it waits: back to
                                                  * the main */
            const int back = (en_song_ph == EN_SG_OUTRO || en_song_ask) && !en_tr_cue;   /* (as the engine has it: in
                                                  * it, or asked by hand; a planned one near is only kept: OUTRO) */
            ui.btn_used |= 1u << B_HOME;
            if (!en_playing || en_tr_cue)        /* (a Transition's cue: the next piece set, nothing to call) */
                break;
            en_outro();
            say("OUTRO", back ? "OFF" : "NEXT PHRASE");
            card_show(back ? "BACK TO THE MAIN" : "OUTRO", back ? "the outro left" : "from the next phrase", K_ACC, 0);
            break;
        }
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            en_play(1);
            say("PLAYING", 0);
        }
        break;
    case B_REC:
        if (plat_ms() - ui.down_t[B_REC] >= GRID_HOLD)
            break;                               /* held long enough for the grid: only a tap is dub hold */
        say("DUB HOLD", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_SAVE:                                 /* the Noise page (one page: again stays; saving: GLO held, Setup's
                                                  * KNOB 4) */
        if (noise_page(ui.view)) {               /* (1.3.0: two pages now, 1.4.0 four, the user's: its part, its
                                                  * voice, its space, its LFO; SAVE again steps through) */
            set_view(ui.view == V_NSND ? V_NVOX : ui.view == V_NVOX ? V_NLFO : V_NSND);   /* (1.4.0: the Noise space page gone, its sends the noise's channel strip, its Width the Noise voice's) */
        } else {
            ui.noise_from = (uint8_t)ui.view;
            ui.noise_round = 0;
            set_view(V_NSND);
        }
        break;
    default: break;
    }
}

/* ---------------------------------------------------------- OCT- / OCT+ --- */
/* on a layer's page (oct_layer): a tap its level -5 / +5 (on its release), held on from 400 ms a step every 100 ms;
 * both together, let go within a second: the layer muted, or back (on the first release: held longer, nothing, so
 * the update mode, both held 5 s, main_fm1.c, never mutes). Elsewhere nothing, for now */
#define OCT_STEP 5
#define OCT_REPEAT0 400u
#define OCT_REPEAT 100u
#define OCT_QUICK 1000u
#define OCT_SHOW 1200u                           /* the ring up after the last press, then OCT_FADE fading */
#define OCT_FADE 250u
static const uint8_t LAYER_LEVEL[NOCT] = {P_CHORD, P_PINGS, P_BASS, P_DRUMS, P_CRACKLE, P_KLVL, P_HATSLVL};
static const char *const LAYER_NAME[NOCT] = {"CHORD", "PINGS", "BASS", "DRUMS", "NOISE", "KICK", "HATS"};
static const uint32_t LAYER_MUTE[NOCT] = {EN_M_CHORD, EN_M_PINGS, EN_M_BASS, EN_M_KICK | EN_M_HATS, EN_M_NOISE, EN_M_KICK,
                                          EN_M_HATS};
static void oct_show(int l)                      /* the ring up (from where it was, if it was: it glides) */
{
    uint32_t now = plat_ms();
    if (ui.oct_ring != l + 1 || now >= ui.oct_until + OCT_FADE)
        ui.oct_v = (float)proj.par[LAYER_LEVEL[l]] * 0.01f;
    ui.oct_ring = (uint8_t)(l + 1);
    ui.oct_until = now + OCT_SHOW;
    ui.big_until = 0;                            /* (one ring at a time) */
    ui.oct_pulse = now;
}
/* (1.4.0, the user's: "the large circle ... take it further"; the screen too small to see what a knob changes) the
 * big ring: a knob turning shows its value over the middle, large, its name under it; one ring at a time, the last
 * one turned; it fades out OCT_SHOW after the last click */
static float big_frac(int k)                     /* the knob's place, 0..1 of its range */
{
    int lo, hi;
    knob_range(k, &lo, &hi);
    return hi > lo ? (float)(knob_get(k) - lo) / (float)(hi - lo) : 0.0f;
}
static void big_show_help(int k, const char *name, uint16_t col, const char *help);
static void big_show(int k, const char *name, uint16_t col) { big_show_help(k, name, col, 0); }
static void big_show_help(int k, const char *name, uint16_t col, const char *help)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.big_k != k || now >= ui.big_until + OCT_FADE)
        ui.big_v = big_frac(k);
    ui.big_k = (int16_t)k;
    for (i = 0; name && name[i] && i < (int)sizeof ui.big_name - 1; i++)
        ui.big_name[i] = (char)(name[i] >= 'a' && name[i] <= 'z' ? name[i] - 32 : name[i]);
    ui.big_name[i] = 0;
    ui.big_col = col;
    ui.big_help = help;
    ui.big_txt[0] = 0;
    ui.big_until = now + OCT_SHOW;
    ui.oct_ring = 0;                             /* (one ring at a time: the last one turned) */
}
static int big_on(uint32_t now) { return ui.big_until && now < ui.big_until + OCT_FADE; }
static void big_show_text(const char *name, const char *txt, float tf, uint16_t col)   /* (no knob's: the screen) */
{
    int i;
    big_show_help(K_NONE, name, col, 0);
    for (i = 0; txt[i] && i < (int)sizeof ui.big_txt - 1; i++)
        ui.big_txt[i] = txt[i];
    ui.big_txt[i] = 0;
    ui.big_tf = tf;
    ui.big_v = tf;
}
static int oct_ring_on(uint32_t now)             /* the ring's layer + 1, while it shows on this page (else 0) */
{
    if (!ui.oct_ring || now >= ui.oct_until + OCT_FADE || oct_layer(ui.view) != ui.oct_ring - 1)
        return 0;
    return ui.oct_ring;
}
static void oct_step(int b)
{
    int l = oct_layer(ui.view);
    char t[8];
    if (l < 0)
        return;
    knob_set(LAYER_LEVEL[l], proj.par[LAYER_LEVEL[l]] + (b == B_OCTUP ? OCT_STEP : -OCT_STEP));
    itoa_u((uint32_t)proj.par[LAYER_LEVEL[l]], t);
    say(LAYER_NAME[l], t);
    oct_show(l);
}
static void oct_mute(void)
{
    int l = oct_layer(ui.view);
    uint32_t m;
    if (l < 0)
        return;
    m = LAYER_MUTE[l];
    if ((ui.mutes & m) == m)                     /* (the drums: both out, back; else both out) */
        ui.mutes &= ~m;
    else
        ui.mutes |= m;
    perf_mutes();
    say(LAYER_NAME[l], (ui.mutes & m) == m ? "MUTED" : "ON");
    oct_show(l);
}
static void oct_press(int b)
{
    const int other = b == B_OCTUP ? B_OCTDN : B_OCTUP;
    if (ui.btn >> other & 1u) {                  /* both: a mute, if let go soon (the taps' steps not taken) */
        ui.oct_both = 1;
        ui.oct_done = 0;
        ui.oct_t = plat_ms();
        ui.btn_used |= 1u << B_OCTDN | 1u << B_OCTUP;
    } else {
        ui.oct_both = 0;
        ui.oct_rep = plat_ms() + OCT_REPEAT0;
    }
}
static void oct_release(int b)
{
    if (ui.oct_both) {
        if (!ui.oct_done && plat_ms() - ui.oct_t < OCT_QUICK)
            oct_mute();
        ui.oct_done = 1;
    } else if (!(ui.btn_used >> b & 1u)) {
        oct_step(b);                             /* a tap */
    }
}
static void oct_hold(void)                       /* one OCT held on: its steps */
{
    uint32_t dn = ui.btn >> B_OCTDN & 1u, up = ui.btn >> B_OCTUP & 1u, now = plat_ms();
    int b = up ? B_OCTUP : B_OCTDN;
    if (ui.oct_both || dn == up || (int32_t)(now - ui.oct_rep) < 0)
        return;
    ui.btn_used |= 1u << b;                      /* (its release then takes no step) */
    ui.oct_rep = now + OCT_REPEAT;
    oct_step(b);
}

/* MIDI CC in, channel 1 (a controller's knobs and faders): a fixed map onto the panel's knobs, 0..127 over each
 * one's whole range (64 a centred knob's 0); the big ring and the reader show it, as a turn would */
static const struct { uint8_t cc; uint16_t k; } CC_MAP[] = {
    {1, K_FILTER},                                         /* (the mod wheel: the DJ filter, SELECT's) */
    {20, P_CHORD}, {21, P_PINGS}, {22, P_BASS}, {23, P_DRUMS},   /* the layers' levels */
    {24, P_KLVL}, {25, P_HATSLVL}, {26, P_CRACKLE},        /* the Kick, Hats and Noise strips' levels */
    {27, K_DUB}, {28, K_ENERGY}, {29, K_SPACE},            /* the master actions: Wet, Energy, Duck (for playing) */
    {71, P_CRES}, {72, P_DECAY}, {73, P_CATT}, {74, P_CCUT},   /* the chord's voice (the GM sound controllers') */
    {91, P_REVERB}, {92, P_ECHO}, {93, P_FEEDBACK}, {94, P_ETIME}, {95, P_CLOUD},   /* the space */
    {102, P_DENSITY}, {103, P_VARY}, {104, P_HITS}, {105, P_SWING}, {106, P_SIZE},
};
static void cc_in(uint32_t cc, uint32_t v)
{
    unsigned i;
    int k, lo, hi;
    char n[16], t[12];
    const char *s;
    for (i = 0; i < sizeof CC_MAP / sizeof CC_MAP[0] && CC_MAP[i].cc != cc; i++)
        ;
    if (i == sizeof CC_MAP / sizeof CC_MAP[0])
        return;
    k = CC_MAP[i].k;
    knob_range(k, &lo, &hi);
    knob_set(k, lo + (int)((v * (uint32_t)(hi - lo) + 63u) / 127u));
    knob_text(k, t);
    s = knob_name(k);
    for (i = 0; s[i] && i < sizeof n - 1; i++)
        n[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    n[i] = 0;
    say_name(n, t);
    big_show(k, s, K_ACC);
    ui.turn_t = plat_ms();
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale, its CCs turn knobs
 * (CC_MAP); clock and start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v)
            toggle_note((int)(n % 12u));
        else if (st == 0xB0u)
            cc_in(n, v & 0x7Fu);
    }
}


#define RESET_HOLD 1500u                 /* SAVE held: the piece back to its recipe */

/* NUDGE: a layer's button held (ENV the chord, ARP the pings, LFO the bass, SEQ the drums): its sound a small step
 * further, a few of its values each a few units, a walk pulled back to the piece's recipe (the further a value has
 * gone, the likelier it steps back), so nudges explore round the piece's sound and never wander off. The biggest
 * change said, and the layer's sound page shown, that knob lit. (1.4.0, format 29, the variety plan: Evolve's own
 * nudges, by themselves, gone: the sound moves by hand, by Drift's LFOs and the dub mixer; Vary is the patterns) */
#define NUDGE_HOLD 500u
#define SHAKE_HOLD 1500u                  /* held on: a shake (a new voice, every value a big step) */
#define NNUDGE 6
static const uint8_t NUDGE_BTN[NNUDGE] = {B_ENV, B_ARP, B_LFO, B_SEQ, B_FX, B_SAVE};
static const char *const NUDGE_NAME[NNUDGE] = {"CHORD", "PINGS", "BASS", "DRUMS", "SPACE", "NOISE"};
static const uint8_t NUDGE_PAR[NNUDGE][8] = {   /* (the FM ones, the last two of the first three, on an FM voice only) */
    {P_CATT, P_CCOL, P_CRES, P_DECAY, P_CATT, P_CCUT, P_CFRAT, P_CFB},   /* (Env, on no page now, left be) */
    {P_PTON, P_PRAT, P_PLEN, P_PBEND, P_PTON, P_PLEN, P_PFRAT, P_PFB},   /* (1.4.0: Wet, off the page, left be) */
    {P_BBITE, P_BSHAPE, P_BLEN, P_BSLIDE, P_FIFTH, P_BLEN, P_BFRAT, P_BFB},
    {P_KTUNE, P_KLEN, P_KPUNCH, P_HTONE, P_KLEN, P_HATSLVL, P_HTONE, P_KTUNE},   /* (Hats: their level, against the kick) */
    {P_ECHO, P_FEEDBACK, P_REVERB, P_SIZE, P_CLOUD, P_ECHO, P_ECHO, P_ECHO},   /* (the space: FX; its shake, a new echo time) */
    {P_NCOL, P_NGRAIN, P_NTUNE, P_NROOM, P_NCOL, P_NGRAIN, P_NTUNE, P_NCOL},   /* (the noise: SAVE; its shake, a new character) */
};
static uint32_t nudge_seed = 0x9E3779B9u;
static int nrand(int n)
{
    nudge_seed = nudge_seed * 1664525u + 1013904223u;
    return n > 0 ? (int)((nudge_seed >> 8) % (uint32_t)n) : 0;
}
/* the bass's groove, a step to a neighbour (Drone, Offbeat, Dotted, Sync, Roller, the knob's row): back towards the
 * piece's own as often as not (pull: 0..100, how likely back when it isn't the piece's) */
static void groove_step(int recipe, int pull)
{
    int g = proj.par[P_GROOVE], hi = en_param_info(P_GROOVE)->hi, d;
    if (g != recipe && nrand(100) < pull)
        d = g > recipe ? -1 : 1;
    else
        d = g == 0 ? 1 : g == hi ? -1 : nrand(2) ? 1 : -1;
    knob_set(P_GROOVE, g + d);
}
enum { NUDGE_HAND, NUDGE_SHAKE };
/* how a nudge goes: HAND (the button held: 3-4 values, steps
 * up to an eighth of the range), SHAKE (held on: a new voice or kit, and every value a big step). Each value walks
 * from where it is, pulled back to the recipe: back as likely as it is far from it, always back from a quarter of
 * the range away */
static uint8_t nudge_quiet;                      /* (HOME held on: the shakes all at once, said once, the page kept) */
static void nudge(int layer, int how)
{
    /* (1.4.0: the layer's pages its knobs are on: the first the one shown, unless another's moved most) */
    static const uint8_t SND[NNUDGE][3] = {{V_CSND, V_CFM, V_CHORD}, {V_PSND, V_PFM, V_TEXTURE}, {V_BSND, V_BFM, V_BASS},
                                           {V_DSND, V_HATS, V_DRUMS}, {V_SPACE, V_ESND, V_SPACE}, {V_NVOX, V_NSND, V_NSND}};
    static const uint8_t VOICE[NNUDGE] = {P_CVOICE, P_PVOICE, P_BVOICE, P_KIT, P_ETIME, P_NCHAR};
    int16_t rec[P_NPARAMS];
    uint16_t pool;
    int j, best = -1, bd = 0, fm, n, np = 0;
    uint8_t pick[8];
    char vn[24] = "";
    if (how == NUDGE_SHAKE && layer == 4) {      /* the space's: another echo time */
        int nt = en_param_info(P_ETIME)->hi + 1, cur = proj.par[P_ETIME], v = (cur + 1 + nrand(nt - 1)) % nt;
        knob_set(P_ETIME, v);
        en_param_text(P_ETIME, v, vn);
    } else if (how == NUDGE_SHAKE) {             /* a new voice (another than this), first: the values then fit it */
        const en_param_t *pi = en_param_info(VOICE[layer]);
        int nv = pi->hi - pi->lo + 1 - (layer == 0), cur = proj.par[VOICE[layer]], v;
        v = (cur + 1 + nrand(nv > 1 ? nv - 1 : 1)) % nv;   /* (the chord's: not Noise, the last, an effect more
                                                             * than a colour; ALGORITHM still has it) */
        if (v == cur)
            v = (v + 1) % nv;
        set_voice(VOICE[layer], v);
        en_param_text(VOICE[layer], proj.par[VOICE[layer]], vn);
    }
    fm = layer < 3 && en_voice_fm(layer);
    en_piece(proj.piece, rec, &pool);
    if (how == NUDGE_SHAKE && layer == 2) {      /* the bass's shake: a new groove too, a neighbour */
        int k = 0;
        groove_step(rec[P_GROOVE], 50);
        while (vn[k] && k < 10)
            k++;
        vn[k++] = ',';
        vn[k++] = ' ';
        vn[k] = 0;
        {
            char gn[12];
            int i;
            en_param_text(P_GROOVE, proj.par[P_GROOVE], gn);
            for (i = 0; gn[i] && k < 23; i++)
                vn[k++] = gn[i];
            vn[k] = 0;
        }
    }
    for (j = 0; j < (fm ? 8 : 6); j++) {         /* (the values to choose from, each once) */
        int q, dup = 0;
        for (q = 0; q < np; q++)
            dup |= pick[q] == NUDGE_PAR[layer][j];
        if (!dup)
            pick[np++] = NUDGE_PAR[layer][j];
    }
    n = how == NUDGE_SHAKE ? np : 3 + nrand(2);
    for (j = 0; j < n && np; j++) {
        int w = nrand(np), p = pick[w], lo = en_param_info(p)->lo, hi = en_param_info(p)->hi;
        int st = (hi - lo) / (p == P_DECAY || p == P_CCUT || p == P_FEEDBACK ? 16 : 8), v = proj.par[p], d;
        pick[w] = pick[--np];                    /* (not twice in one nudge) */
        if (p == P_KTUNE)
            st = 2;                              /* (the kick's tune: semitones) */
        if (how == NUDGE_SHAKE)
            st = st * 3 / 2;
        if (st < 1)
            st = 1;
        d = 1 + nrand(st);                       /* a step of 1..st; back towards the recipe as likely as it is far
                                                  * from it (a quarter of the range away: always back), else either way */
        if (v != rec[p] && nrand(hi - lo) < 4 * (v > rec[p] ? v - rec[p] : rec[p] - v))
            d = v > rec[p] ? -d : d;
        else if (nrand(2))
            d = -d;
        v += d;
        v = v < lo ? lo : v > hi ? hi : v;
        d = v - proj.par[p];
        if (!d)
            continue;
        knob_set(p, v);
        if (d * d > bd * bd)
            best = p, bd = d;
    }
    if (layer == 1 && (how == NUDGE_SHAKE || nrand(2)))   /* the pings: now and then a note of one loop, another */
        en_ping_nudge();
    {   /* said: "CHORD NUDGE  Colour +9", "CHORD SHAKE  Keys" */
        char t[24], head[16], v[8];
        const char *nm = how == NUDGE_SHAKE ? vn : best >= 0 ? en_param_info(best)->name : "";
        int k = 0, i;
        while (*nm && k < (how == NUDGE_SHAKE ? 23 : 14))
            t[k++] = *nm++;
        if (how == NUDGE_HAND && best >= 0) {
            t[k++] = ' ';
            t[k++] = bd > 0 ? '+' : '-';
            itoa_u((uint32_t)(bd > 0 ? bd : -bd), v);
            for (i = 0; v[i] && k < 23; i++)
                t[k++] = v[i];
        }
        t[k] = 0;
        for (i = 0; NUDGE_NAME[layer][i]; i++)
            head[i] = NUDGE_NAME[layer][i];
        for (k = 0; (how == NUDGE_SHAKE ? " SHAKE" : " NUDGE")[k] && i < 15; k++)
            head[i++] = (how == NUDGE_SHAKE ? " SHAKE" : " NUDGE")[k];
        head[i] = 0;
        if (!nudge_quiet)
            say(head, t);
        if (!nudge_quiet)                        /* (a card in the middle too) */
            card_show(head, t, how == NUDGE_SHAKE ? K_FROST : K_PULSE, how == NUDGE_SHAKE);
    }
    if (!ui.perf && !nudge_quiet) {
        int sv = SND[layer][0], q;
        for (q = 0; q < 3 && how == NUDGE_HAND; q++) {   /* (1.4.0: the page whose knob moved most: the voice page, the FM
                                                  * one, the Hats page...) */
            for (j = 0; j < 4 && !(VIEW_KNOB[SND[layer][q]][j] == best && !fm_knob_off(SND[layer][q], j)); j++)
                ;
            if (j < 4) {
                sv = SND[layer][q];
                break;
            }
        }
        if (noise_page(sv) && !noise_page(ui.view)) {
            ui.noise_from = (uint8_t)ui.view;   /* (SAVE again goes back there) */
            ui.noise_round = 0;
        }
        set_view(sv);
        for (j = 0; j < 4; j++)
            if (vknob(ui.view, j) == best && how == NUDGE_HAND) {
                ui.touched = (int8_t)j;
                ui.touch_until = plat_ms() + 1500u;
            }
    }
}
static void groove_tick(void)                    /* (1.4.0, format 29) the bass's borrowed groove: on a phrase's first
                                                  * bar, now and then, by the bass's amount (Vary and its trim) */
{
    static uint8_t last_pb = 0xFF;
    uint8_t pb = en_phrase_bar;
    const int t = proj.par[P_BVARY];
    const float a = t <= -50 ? 0.0f : 100.0f * en_vary_amount(proj.par[P_VARY] + t);   /* (format 35: the amount, on
                                                  * the knob's new scale) */
    if (pb == last_pb)
        return;
    last_pb = pb;
    if (pb != 0 || !en_playing || en_frozen || a <= 0.0f || ui.lock)   /* (Lock, or the bass held: it rests) */
        return;
    if ((float)nrand(4000000) < a * a * 7.0f) {   /* the bass's groove, rarely, to a neighbour and back: as the square
                                                  * of the amount, no threshold (100: 7 phrases in 400, every few
                                                  * minutes, as Evolve 100 had it; 50: a quarter of that; 0 never) */
        int16_t rec[P_NPARAMS];
        uint16_t pool;
        en_piece(proj.piece, rec, &pool);
        groove_step(rec[P_GROOVE], 70);
    }
}
static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_GLO)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
            if (i == B_OCTDN || i == B_OCTUP)
                oct_press((int)i);
        } else if (i == B_OCTDN || i == B_OCTUP) {
            oct_release((int)i);
        } else if (i == B_EDIT) {               /* EDIT: the Chords page (a tap), the next chord (held) */
            edit_released(plat_ms() - ui.down_t[B_EDIT]);
        } else if (i == B_GLO && !(ui.btn_used & m) && plat_ms() - ui.down_t[B_GLO] >= NUDGE_HOLD) {
            do_save();                           /* GLO held and let go: saved (held on: the reset, below, unsaved) */
            if (!ui.dirty)
                card_show("SAVED", "all eight pieces", K_ACC, 0);
        } else if (i == B_SEL && !(ui.btn_used & m) && plat_ms() - ui.down_t[B_SEL] >= NUDGE_HOLD) {
            lock_toggle();                       /* (SEL held: Lock; a tap, PERFORM) */
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    oct_hold();
    for (i = 0; i < NNUDGE; i++) {               /* a layer's button held: a nudge; held on: a shake (its release
                                                  * then does nothing) */
        static uint8_t shaken;
        uint32_t m = 1u << NUDGE_BTN[i], held = plat_ms() - ui.down_t[NUDGE_BTN[i]];
        if (!(btn & m)) {
            shaken &= (uint8_t)~(1u << i);
        } else if (!(ui.btn_used & m) && held >= NUDGE_HOLD) {
            ui.btn_used |= m;
            nudge((int)i, NUDGE_HAND);
        } else if ((ui.btn_used & m) && held >= SHAKE_HOLD && !(shaken >> i & 1u) &&
                   (int32_t)(ui.turn_t - ui.down_t[NUDGE_BTN[i]]) <= 0) {
            shaken |= (uint8_t)(1u << i);
            nudge((int)i, NUDGE_SHAKE);
        }
    }
    {   /* (1.3.0) HOME held on (1.5 s, nothing turned meanwhile: not the tempo): a shake of everything that shakes,
         * the chord, the pings, the bass, the drums and the noise, at once; again and again, till something sounds */
        static uint8_t home_shaken;
        const uint32_t m = 1u << B_HOME;
        if (!(btn & m)) {
            home_shaken = 0;
        } else if (!home_shaken && !(ui.btn_used & m) && plat_ms() - ui.down_t[B_HOME] >= SHAKE_HOLD &&
                   (int32_t)(ui.turn_t - ui.down_t[B_HOME]) <= 0) {
            static const uint8_t ALL[5] = {0, 1, 2, 3, 5};
            int j;
            home_shaken = 1;
            ui.btn_used |= m;
            nudge_quiet = 1;
            for (j = 0; j < 5; j++)
                nudge(ALL[j], NUDGE_SHAKE);
            nudge_quiet = 0;
            say("SHAKE", "EVERYTHING");
            card_show("SHAKE ALL", "chord pings bass drums noise", K_FROST, 1);
        }
    }
    if ((btn >> B_EDIT & 1u) && !edit_new && plat_ms() - ui.down_t[B_EDIT] >= SHAKE_HOLD &&
        (int32_t)(ui.turn_t - ui.down_t[B_EDIT]) <= 0) {   /* EDIT held on: a new progression, the walk's chords
                                                  * all walked again (yours, the gaps and the one playing kept) */
        edit_new = 1;
        en_prog_rewalk();
        say("NEW CHORDS", EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK]);
        card_show("NEW CHORDS", EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK], K_FROST, 1);
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            int down = (int)(keys >> i & 1u);
            if (((btn >> B_REC & 1u) && down) || (!down && ui.fx))
                if (fx_key(i, down))
                    continue;                    /* REC held: the effects (and their keys let go) */
            if (btn >> B_REC & 1u)
                continue;                        /* (REC held: the other keys do nothing) */
            if (ui.view == V_PAT && !ui.perf) {  /* the Pattern page: the white keys, the lane's steps */
                if (down)
                    pattern_key(i, keys);
                continue;
            }
            if (ui.view == V_CHORDS && !ui.perf) {   /* the Chords page: the keys, the slot's root */
                if (down)
                    slot_key(i);
                continue;
            }
            if (ui.perf)
                perf_key(i, (int)(keys >> i & 1u));
            else if (keys >> i & 1u)
                toggle_note((int)((5u + i) % 12u));   /* key 0 is F3 */
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_KEY) {              /* Tempo is the Key page's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {                                 /* (1.4.0, the user's: the DJ filter, on every page, as the Form
                                                  * page's KNOB 1; its notch at Off; a ring while it turns) */
            char t[12];
            turn(EN_SELECT, K_FILTER, e);
            knob_text(K_FILTER, t);
            say_name("FILTER", t);
            big_show_help(K_FILTER, "Filter", K_ACC, "Shut: under water. Open: thin");
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {         /* a layer's page: its voice; the Chords page: the walk; elsewhere: the screen */
        int vp = view_voice(ui.view);
        if (noise_page(ui.view) && ui.view != V_NLFO) {   /* the Noise pages: its character */
            int c = ((proj.par[P_NCHAR] + (int)(e % EN_NNCHAR)) % EN_NNCHAR + EN_NNCHAR) % EN_NNCHAR;
            knob_set(P_NCHAR, c);
            say_name("NOISE", EN_NCHAR_NAME[c]);
            big_show(P_NCHAR, "Noise", K_NOISE);  /* (1.4.0, the user's: ALGORITHM big in the centre too) */
        } else if (ui.view == V_CHORDS || ui.view == V_WALK) {   /* the walk's character: its chords walked again, at
                                                  * once (1.4.0: the Walk page's KNOB 1 too) */
            int w = (((proj.par[P_PMODE] & 3) + (int)(e % EN_NWALK)) % EN_NWALK + EN_NWALK) % EN_NWALK;
            knob_set(P_PMODE, (proj.par[P_PMODE] & EN_PLOOP) | w);
            en_prog_rewalk();
            {   /* the character, and what it does */
                static const char *const WHAT[EN_NWALK] = {"Home: stays near home", "Wander: roams wider",
                                                         "Free: goes anywhere"};
                say("WALK", WHAT[w]);
            }
        } else if (ui.view == V_NLFO) {          /* (1.4.0, the user's) the Noise LFO page: where its LFO and drift go
                                                  * (Filter, Tone, Air), round; its character on the other Noise pages */
            int d = ((proj.par[P_NLDEST] + (int)(e % EN_NNLDEST)) % EN_NNLDEST + EN_NNLDEST) % EN_NNLDEST;
            knob_set(P_NLDEST, d);
            say_name("DESTINATION", EN_NLDEST_NAME[d]);
            big_show_help(P_NLDEST, "Destination", K_NOISE, "What the LFO moves");
        } else if (ui.view == V_CLFO) {          /* (1.4.0, the user's) the Chord LFO page: where the LFO and the drift go
                                                  * (Cutoff, Reso, Colour), round */
            int d = ((proj.par[P_CLDEST] + (int)(e % EN_NCLDEST)) % EN_NCLDEST + EN_NCLDEST) % EN_NCLDEST;
            knob_set(P_CLDEST, d);
            say_name("DESTINATION", EN_CLDEST_NAME[d]);
            big_show_help(P_CLDEST, "Destination", K_PAD, "What the LFO moves");
        } else if (vp >= 0) {
            static const uint16_t LC[NOCT] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_NOISE, K_PULSE, K_TEXT};
            const int l = oct_layer(ui.view);
            set_voice(vp, proj.par[vp] + (int)e);   /* (every detent: a quick turn comes as several at once) */
            big_show(vp, vp == P_KIT ? "Kit" : "Voice", l >= 0 ? LC[l] : K_ACC);   /* (1.4.0: big in the centre) */
        } else {                                 /* the middle of the screen: the ripples, a scope, a spectrum (the mode,
                                                  * a piece's choice, is on the Key page) */
            static const char *const SCREEN_NAME[SCR_N] = {"RIPPLES", "SCOPE", "SPECTRUM", "TAPE"};
            static const char *const SN[SCR_N] = {"Ripples", "Scope", "Spectrum", "Tape"};
            int at = 0, j;
            for (j = 0; j < SCR_N; j++)          /* (its place among the four; 1.4.0: the stored numbers kept) */
                if (SCR_ORDER[j] == ui_screen_ok(proj.screen))
                    at = j;
            at = ((at + (int)(e % SCR_N)) % SCR_N + SCR_N) % SCR_N;
            proj.screen = SCR_ORDER[at];
            mark_dirty();
            say_name("SCREEN", SCREEN_NAME[at]);
            big_show_text("Screen", SN[at], (float)at / (float)(SCR_N - 1), K_ACC);   /* (1.4.0: big in the centre too) */
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (int)e);
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_GLO & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_GLO] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held on: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_GLO;
        project_piece_reset();
        project_apply();
        en_regrow();
        ui.prog_from = en_queue_w();
        say("RESET", piece_name(proj.piece));
        card_show("RESET", piece_name(proj.piece), K_FROST, 1);
        mark_dirty();
    }
    strip_tick();
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (grid_up())
                continue;                        /* (the grid on the screen: the knobs rest) */
            if (ui.strip_key && ui.perf && ui.view == V_PERF)
                strip_open();                    /* (a part's key down and a knob turned: its strip at once) */
            if (ui.view == V_STRIP)
                ui.strip_used = 1;
            if (fm_knob_off(ui.view, (int)i) && vknob(ui.view, i) == K_TRANS) {
                big_show_help(K_TRANS, KNOB_LABEL[ui.view][i], K_DIM, TRANS_OFF);   /* (the user's: Set mode alone;
                                                  * the turn says so, the value kept) */
                ui.touched = (int8_t)i;
                ui.touch_until = plat_ms() + 1200u;
                ui.act_t = ui.turn_t = plat_ms();
                continue;
            }
            if (fm_knob_off(ui.view, (int)i) || vknob(ui.view, i) == K_NONE)
                continue;                        /* (an FM page on a voice that isn't FM: the knobs rest, greyed; a
                                                  * knob of none, 1.4.0, rests too) */
            turn(EN_K1 + (int)i, vknob(ui.view, i), e);
            /* (1.4.0, the user's: no more the knob's name in the header: the big ring names it, over it; the header
             * the piece's) */
            {   /* (1.4.0) the big ring: the knob's value large over the middle (not where the page itself is the
                 * picture: the Chords and the Pattern pages; nor Save and the CPU) */
                static const uint16_t LC[NOCT] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_NOISE, K_PULSE, K_TEXT};
                const int k = vknob(ui.view, i), l = oct_layer(ui.view);
                if (ui.view != V_CHORDS && ui.view != V_PAT && k != K_SAVE && k != K_CPU && k != K_NONE)
                {
                    char sn[24];                 /* (a strip's: its part's name first, "CHORD ECHO") */
                    const char *nm = KNOB_LABEL[ui.view][i];
                    if (ui.view == V_SIREN) {    /* (format 34: a siren's, its own help, in its red) */
                        big_show_help(k, nm, K_FROST, SIR_HELP[sir_page % EN_NSIR][i]);
                        goto shown;
                    }
                    if (ui.view == V_STRIP) {
                        int a = 0, b = 0;
                        while (STRIP_NAME[strip_part][a])
                            sn[a] = STRIP_NAME[strip_part][a], a++;
                        sn[a++] = ' ';
                        while (nm[b] && a < (int)sizeof sn - 1)
                            sn[a++] = nm[b++];
                        sn[a] = 0;
                        nm = sn;
                    }
                    big_show_help(k, nm, l >= 0 ? LC[l] : K_ACC, k == K_TRANS ? TRANS_HELP[knob_get(k)] : KNOB_HELP[ui.view][i]);
                }
            }
        shown:
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the main screen (140), the knobs (72). (1.4.0, the user's: the knobs' rule 2 px
 * more room above and below; the main screen 4 px shorter, its pictures drawn as they were, all 4 px higher: main_begin
 * draws at MAIN_SHIFT over the canvas's top, so its drawing coordinates still run 0 .. MAIN_DH, the top 4 rows off it) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 140
#define MAIN_SHIFT 4
#define MAIN_DH (MAIN_H + MAIN_SHIFT)
#define KNB_Y 168
#define KNB_H 72

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static void main_begin(uint16_t bg)               /* the main screen's canvas: drawn 0 .. MAIN_DH, shown from MAIN_SHIFT */
{
    cv_begin(240, MAIN_H, bg);
    cv_oy = -MAIN_SHIFT;
}

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* (1.4.0) a rounded rectangle's corner inset at row j of its r rows (0 the outermost) */
static int32_t round_in(int32_t r, int32_t j)
{
    const float dy = (float)(r - j) - 0.5f;
    return r - (int32_t)(fm_sqrtf(fm_maxf((float)(r * r) - dy * dy, 0.0f)) + 0.5f);
}
/* (1.4.0) a rounded rectangle's pixels blended towards c by a/16 (12: what was there a quarter): a see-through panel */
static void cv_shade(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t a, uint16_t c)
{
    int32_t j, i;
    const uint32_t cr = (uint32_t)(c >> 11) * a, cg = (uint32_t)((c >> 5) & 63u) * a, cb = (uint32_t)(c & 31u) * a,
                   na = 16u - a;
    if (!a)
        return;
    for (j = 0; j < h; j++) {
        const int32_t yy = y + j + cv_oy, in = j < r ? round_in(r, j) : j >= h - r ? round_in(r, h - 1 - j) : 0;
        int32_t x0, x1;
        if (yy < cv_y0 || yy >= cv_y1)
            continue;
        x0 = x + in < 0 ? 0 : x + in;
        x1 = x + w - in > (int32_t)cv_w ? (int32_t)cv_w : x + w - in;
        for (i = x0; i < x1; i++) {
            uint16_t *q = &cv_px[(uint32_t)yy * cv_w + (uint32_t)i];
            const uint32_t b = swap16(*q);
            *q = swap16(((((b >> 11) * na + cr) >> 4) << 11) | (((((b >> 5) & 63u) * na + cg) >> 4) << 5) |
                        (((b & 31u) * na + cb) >> 4));
        }
    }
}
/* (1.4.0) a rounded rectangle's edge, 1 px, in c */
static void cv_round_edge(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    int32_t j;
    cv_rect(x + r, y, w - 2 * r, 1, c);
    cv_rect(x + r, y + h - 1, w - 2 * r, 1, c);
    cv_rect(x, y + r, 1, h - 2 * r, c);
    cv_rect(x + w - 1, y + r, 1, h - 2 * r, c);
    for (j = 0; j < r; j++) {                    /* (the corners: from this row's inset to the next's) */
        const int32_t a = round_in(r, j), b = j + 1 < r ? round_in(r, j + 1) : 0, len = a - b > 0 ? a - b : 1;
        cv_rect(x + b, y + j, len, 1, c);
        cv_rect(x + w - b - len, y + j, len, 1, c);
        cv_rect(x + b, y + h - 1 - j, len, 1, c);
        cv_rect(x + w - b - len, y + h - 1 - j, len, 1, c);
    }
}
/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc_th(int32_t cx, int32_t cy, int32_t r, float th, float v, uint16_t track, uint16_t fill, uint16_t bg,
                   int bipolar)
{
    const float a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    const int nohandle = bipolar & 2;            /* (1.4.0: an LFO's dot in its place: the fill alone says where) */
    bipolar &= 1;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        if (!nohandle) {
            disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
            disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
        } else if (hi > lo) {
            disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f, fill);   /* (the fill's end
                                                  * round) */
        }
    }
}

static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    arc_th(cx, cy, r, 3.4f, v, track, fill, bg, bipolar);
}
/* (1.4.0, the user's) over a knob's ring: the span f0..f1 (0..1 of its sweep) in col, and a dot at fn: what an LFO
 * does to it, its reach and where it is now (the Chord page's Cutoff) */
static void arc_span_w(int32_t cx, int32_t cy, int32_t r, float f0, float f1, float fn, uint16_t col, uint16_t dot,
                      int bold)
{
    const float a0 = 0.375f, sweep = 0.75f, th = 3.4f, rm = (float)r - th * 0.5f,
                ro = (float)r + (bold ? 3.7f : 2.7f), ri = (float)r + 1.0f,   /* (the span just outside the ring, under 2
                                                  * px: the knob's own fill left to read; bold, over 3) */
                io = (float)r - th - 0.6f, ii = io - (bold ? 2.8f : 1.5f);   /* (and just inside it, the user's: the reach
                                                  * doubled) */
    int32_t x, y;
    f0 = fm_clampf(f0, 0.0f, 1.0f), f1 = fm_clampf(f1, 0.0f, 1.0f), fn = fm_clampf(fn, 0.0f, 1.0f);
    for (y = -r - 5; y <= r + 5; y++)
        for (x = -r - 5; x <= r + 5; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f) +
                fm_clampf(io + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ii + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t >= f0 && t <= f1)
                px_blend(cx + x, cy + y, col, (uint32_t)(fm_minf(a, 1.0f) * 16.0f + 0.5f));
        }
    {
        const float e = (a0 + sweep * fn) * FM_TWO_PI, dx = (float)cx + fm_cosf(e) * rm, dy = (float)cy + fm_sinf(e) * rm;
        if (bold)                                /* (a dark rim round a bigger dot: it reads on the band and the ring) */
            disc(dx, dy, th * 0.5f + 2.6f, K_BG);
        disc(dx, dy, th * 0.5f + (bold ? 1.6f : 0.9f), dot);
    }
}
static void arc_span(int32_t cx, int32_t cy, int32_t r, float f0, float f1, float fn, uint16_t col, uint16_t dot)
{
    arc_span_w(cx, cy, r, f0, f1, fn, col, dot, 0);
}
/* a mark across a knob's ring at v (0..1 of its sweep), a little past both its edges: where a knob's behaviour changes
 * (Motion's regions, 40 and 70) */
static void arc_mark_th(int32_t cx, int32_t cy, int32_t r, float th, float v, uint16_t col)
{
    const float a = (0.375f + 0.75f * v) * FM_TWO_PI, c = fm_cosf(a), sn = fm_sinf(a), sx = -sn * 0.7f, sy = c * 0.7f;
    float d;
    for (d = (float)r - th - 1.8f; d <= (float)r + 1.6f; d += 0.4f) {
        px_blend(cx + (int32_t)(c * d + 0.5f), cy + (int32_t)(sn * d + 0.5f), col, 16u);
        if (th > 4.0f)                          /* (on the big ring, two pixels wide) */
            px_blend(cx + (int32_t)(c * d + sx + 0.5f), cy + (int32_t)(sn * d + sy + 0.5f), col, 16u);
    }
}
static void arc_mark(int32_t cx, int32_t cy, int32_t r, float v, uint16_t col) { arc_mark_th(cx, cy, r, 3.4f, v, col); }

/* (format 31, the variety plan's step 3) a knob's zones: a continuous knob whose range moves something all the way
 * (no dead zone), each zone where a new kind of event joins (decision 6). Each zone: where it begins (the knob's
 * value), its name (under the big ring's value, round the ring, the reader's), its sentence (the help line while it's
 * turned). Motion's first; (format 32, step 4) Hits' and the bass's Density's. Every other knob has none: continuous
 * (0..100 or centred, every step moving), or a selector (its named or counted values: Stabs, Groove, Kick, Change, ...);
 * Energy, Vary, Drift and the Densities of the pings and the noise continuous, no event joining at a threshold */
typedef struct {
    int16_t at;
    const char *name, *help, *help0;             /* (help0: the sentence at its first value, if that's another: 0) */
} zone_t;
static const zone_t MOTION_ZONES[] = {
    {0, "Breathes", "Parts out and back, the kick out a bar or two", "Nothing leaves: the loop as it is"},
    {40, "Sections", "Thin and Groove phrases join, rarely to often", 0},
    {70, "Breaks", "Breaks join: the kick and the bass out a phrase", 0},
};
/* (format 32, the variety plan's step 4) the Drums page's Hits: the shaker's steps growing all the way (each its last
 * step faded in), the open hat joining at 15 (the offbeats of one and three), on every offbeat from 40, the rim on two
 * and four from 70 (hats_publish) */
static const zone_t HITS_ZONES[] = {
    {0, "Shaker", "The shaker alone, a step more as it turns", "No hats at all"},
    {15, "Open hat", "The open hat joins on one and three's offbeat", 0},
    {40, "Offbeats", "The open hat on every offbeat", 0},
    {70, "Rim", "The rim joins on two and four: the dub snare", 0},
};
/* the Bass page's Density: from the bar's first note alone to the whole groove (the last note faded in); from 30 a busy
 * fourth or eighth bar plays the whole groove and a pickup into the next (ph_bar) */
static const zone_t BNOTES_ZONES[] = {
    {0, "Sparse", "The groove's notes join one by one", "The bar's first note alone"},
    {30, "Pickups", "Busy bars add a pickup into the next", 0},
};
static const char *zone_help(const zone_t *z, int v) { return z->help0 && v == z->at ? z->help0 : z->help; }
static const struct {
    uint16_t k;
    uint8_t n;
    const zone_t *z;
} KNOB_ZONES[] = {
    {P_MOTION, 3, MOTION_ZONES},
    {P_HITS, 4, HITS_ZONES},
    {P_BNOTES, 2, BNOTES_ZONES},
};
#define NKNOB_ZONES ((int)(sizeof KNOB_ZONES / sizeof KNOB_ZONES[0]))
static int knob_zones(int k, const zone_t **z)   /* its zones, how many (0: none) */
{
    int i;
    for (i = 0; i < NKNOB_ZONES; i++)
        if (KNOB_ZONES[i].k == k) {
            *z = KNOB_ZONES[i].z;
            return KNOB_ZONES[i].n;
        }
    *z = 0;
    return 0;
}
static int zone_at(int k, int v)                 /* the zone v is in (-1: the knob has none) */
{
    const zone_t *z;
    int n = knob_zones(k, &z), i;
    for (i = n - 1; i > 0 && v < z[i].at; i--)
        ;
    return n ? i : -1;
}
static float zone_frac(int k, int v)             /* a value's place on the ring, 0..1 */
{
    const en_param_t *p = en_param_info(k);
    return p->hi > p->lo ? (float)(v - p->lo) / (float)(p->hi - p->lo) : 0.0f;
}
/* the ring in segments, one a zone: a gap in the background's colour where each begins, the zone the knob is in its
 * track lit a little (its fill as it is) */
static void arc_zones_th(int32_t cx, int32_t cy, int32_t r, float th, int k, int v, uint16_t lit, uint16_t bg)
{
    const zone_t *z;
    const int n = knob_zones(k, &z), at = zone_at(k, v);
    int i;
    if (!n)
        return;
    {   /* (the zone's track, past the value, lit) */
        const float a0 = 0.375f, sweep = 0.75f, f0 = fm_maxf(zone_frac(k, z[at].at), zone_frac(k, v)),
                    f1 = at + 1 < n ? zone_frac(k, z[at + 1].at) : 1.0f, ro = (float)r, ri = ro - th;
        int32_t x, y;
        for (y = -r - 1; y <= r + 1; y++)
            for (x = -r - 1; x <= r + 1; x++) {
                float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
                a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
                if (a <= 0.0f)
                    continue;
                t = turns(dy, dx) - a0;
                if (t < 0.0f)
                    t += 1.0f;
                t /= sweep;
                if (t > f0 + 0.02f && t < f1)
                    px_blend(cx + x, cy + y, lit, (uint32_t)(a * 16.0f + 0.5f));
            }
    }
    for (i = 1; i < n; i++) {                    /* (the gaps: a radial line, ~2 px wide (3 on the big ring)) */
        const float a = (0.375f + 0.75f * zone_frac(k, z[i].at)) * FM_TWO_PI, c = fm_cosf(a), sn = fm_sinf(a);
        const float w = th > 4.0f && r > 30 ? 1.4f : 0.9f;
        float d, o;
        for (d = (float)r - th - 0.8f; d <= (float)r + 0.8f; d += 0.4f)
            for (o = -w; o <= w; o += 0.45f)
                px_blend(cx + (int32_t)(c * d - sn * o + 0.5f), cy + (int32_t)(sn * d + c * o + 0.5f), bg, 16u);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (dxsloop's thresholds on the divider's raw reading): three bars, 591 up, 561, 531; on USB
 * (powering and charging it: there is no charger line) the bars fill one, two, three in turn */
static int batt_bars(void)
{
    int32_t b = plat_battery();
    return b >= 591 ? 3 : b >= 561 ? 2 : b >= 531 ? 1 : 0;
}
static int batt_pct(void)                        /* a rough charge, 521 .. 601 */
{
    int32_t p = (plat_battery() - 521) * 100 / 80;
    return p < 0 ? 0 : p > 100 ? 100 : (int)p;
}
static void draw_battery(int32_t bx)
{
    int lvl = plat_usb_power() ? 1 + (int)((plat_ms() / 600u) % 3u) : batt_bars(), k;
    uint16_t c = !plat_usb_power() && lvl <= 1 ? K_FROST : K_DIM, o = !plat_usb_power() && !lvl ? K_FROST : K_DIM;
    cv_rect(bx, 9, 15, 1, o);                    /* (empty: the outline red) */
    cv_rect(bx, 17, 15, 1, o);
    cv_rect(bx, 9, 1, 9, o);
    cv_rect(bx + 14, 9, 1, 9, o);
    cv_rect(bx + 15, 11, 2, 5, o);
    for (k = 0; k < lvl; k++)
        cv_rect(bx + 2 + k * 4, 11, 3, 5, c);
}

/* the header (1.4.0, the user's: "where we are" moved up here, read at a glance, the main area's corner free): the
 * piece in a tag; then the phrase: the section's word (or the song's part: INTRO, OUTRO) and its eight bars as cells,
 * the B bars (4 and 8) taller, the bar playing lit and flashing white on each beat, the 8th blinking on its bar when
 * the next phrase brings a change; resting, the cells dim and a play triangle after them. On the right, only what
 * matters now: the battery when low or charging (Setup: always, with its charge), a dot while unsaved, EXT while an
 * external clock leads, the red "!" while the master clips; Setup's audio load. In a song, a line along the
 * bottom: the whole song, its parts in their hues, filled as far as it has gone. (The tempo lives on the Key
 * page, and the big ring shows it as HOME held + SELECT turns it.) Messages take its place for a moment. */
static int clip_on(uint32_t now);
static int hdr_song(void) { return en_song_ph != EN_SG_OFF && en_song_be > 0u; }
static int hdr_batt(void)                        /* the battery shown: on Setup, or low (under 20%), or charging (USB) */
{
    if (plat_battery() < 0 && !plat_usb_power())
        return 0;
    return ui.view == V_SETUP || plat_usb_power() || batt_pct() < 20;
}
static void hdr_phrase(int32_t x0, int32_t x1, uint32_t now)   /* the word and the cells, between x0 and x1 */
{
    char t[12];
    const int song_part = en_song_ph == EN_SG_INTRO || en_song_ph == EN_SG_OUTRO;
    int32_t tw, cs = 6, gap = 3, cw, x, b;
    caps(t, en_song_ph == EN_SG_INTRO ? "Intro" : en_song_ph == EN_SG_OUTRO ? "Outro" : EN_SEC_NAME[en_section], sizeof t);
    tw = text_w(&FONT_XS, t);
    cw = 8 * cs + 7 * gap;
    if (x0 + tw + 6 + cw > x1)                    /* (a long name: the cells smaller; then the word left out) */
        cs = 5, gap = 2, cw = 8 * cs + 7 * gap;
    if (x0 + tw + 6 + cw > x1)
        tw = -6;
    if (x0 + tw + 6 + cw > x1)
        return;                                   /* (no room at all: Setup's load, a long name) */
    if (tw > 0)
        cv_text(x0, 6, &FONT_XS, t, en_section == SEC_FULL && !song_part ? K_DIM : K_ACC);
    x = x0 + tw + 6;
    for (b = 0; b < 8; b++) {
        const int bb = (b == 3 || b == 7) && !ui.lock, lit = en_playing && b <= en_phrase_bar,   /* (Lock: all alike) */
                  now_b = en_playing && b == en_phrase_bar;
        uint16_t c = !en_playing ? (bb ? mix(K_BG, K_LINE, 12) : mix(K_BG, K_LINE, 9))
                   : now_b ? ((en_bars & 3u) == 0u ? K_TEXT : K_ACC)   /* (a white flash on each beat) */
                   : lit ? (bb ? K_TEXT : K_DIM) : (bb ? K_DIM : K_LINE);
        if (b == 7 && now_b && en_change_next)
            c = (now / 200u) & 1u ? K_ACC : K_LINE;
        cv_round(x + b * (cs + gap), bb ? 8 : 10, cs, bb ? 11 : 9, 1, c);
    }
    if (!en_playing) {                            /* resting: a play triangle after them */
        const int32_t px = x + cw + 7;
        int32_t j;
        if (px + 6 <= x1)
            for (j = 0; j < 5; j++)
                cv_rect(px + j, 9 + j, 1, 11 - 2 * j, K_DIM);
    }
}
static const uint16_t SONG_HUE[3] = {K_ACC, K_TEXT, K_PULSE};   /* a song's parts: the intro, the main, the outro */
static void hdr_song_line(void)                  /* a song: a line along the bottom, its parts, how far it has gone */
{
    /* (1.4.0, the user's: the parts in their own hues: the intro teal, the main a light neutral, the outro coral; each
     * dim ahead and bright where already played, a dark pixel between parts, the place where it is white; 3 px high:
     * one read badly, two thin still: the user's, zzy) */
    const float k = 224.0f / (float)en_song_be, at = fm_minf((float)en_bars * (1.0f / 16.0f), (float)en_song_be);
    const int32_t xi = 8 + (int32_t)((float)en_song_b0 * k + 0.5f), xo = 8 + (int32_t)((float)en_song_bo * k + 0.5f),
                  xd = 8 + (int32_t)(at * k + 0.5f), y = HDR_H - 3;
    int32_t x;
    for (x = 8; x < 232; x++) {
        const int part = x < xi ? 0 : x < xo ? 1 : 2, done = x < xd;
        if ((x == xi && xi > 8) || (x == xo && xo < 232))
            continue;                            /* (a gap where one part ends and the next begins) */
        cv_rect(x, y, 1, 3, mix(K_BG, SONG_HUE[part], done ? (part == 1 ? 11 : 14) : (part == 1 ? 4 : 6)));
    }
    if (en_song_ph != EN_SG_END)
        cv_rect(xd - 1 < 8 ? 8 : xd - 1, y - 1, 3, 4, K_TEXT);   /* (where it is: a row over the line) */
}
static void draw_header(void)
{
    const uint32_t now = plat_ms();
    int msg = now < ui.msg_until && !big_on(now) && !oct_ring_on(now), clip = clip_on(now);
                                                 /* (1.4.0, the user's: a ring names itself; the header keeps the
                                                  * piece) */
    const int song = hdr_song(), batt = hdr_batt();
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)ui.view << 8 | (uint32_t)batt << 16),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)ui.lock << 7),
                      (uint32_t)(ui.view == V_SETUP ? batt_pct() : 0) << 22 | (uint32_t)batt_bars() << 29 | (uint32_t)plat_usb_power() << 31 |
                      (plat_usb_power() ? (now / 600u) % 3u << 7 : 0u) | (ui.view == V_SETUP ? plat_cpu_pct() << 8 | plat_cpu_peak() << 15 |
                                                                           (plat_xruns() & 0x1FFu) << 22 : 0u));
    h = hash(h, (uint32_t)en_section | (uint32_t)en_song_ph << 4 | (uint32_t)en_phrase_bar << 8 | (uint32_t)en_change_next << 12);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (uint32_t)((en_bars & 3u) == 0u) | (en_phrase_bar == 7 && en_change_next ? (now / 200u) & 1u : 0u) << 1);
    if (song)
        h = hash(h, (uint32_t)((float)en_bars * (1.0f / 16.0f) * 224.0f / (float)en_song_be) | en_song_be << 12 | en_song_bo << 22);
    h = hash(h, (uint32_t)clip | (uint32_t)(ui.solo != 0) << 1 | (uint32_t)en_frozen << 2 | (uint32_t)en_tr_cue << 3);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (batt) {                              /* the battery, at the right edge, when it matters */
            draw_battery(219);
            x = 213;
        }
        if (ui.view == V_SETUP && batt) {        /* Setup: the charge, by the icon */
            char c[8];
            if (plat_usb_power()) {
                c[0] = 'U', c[1] = 'S', c[2] = 'B', c[3] = 0;
            } else {
                uint32_t k;
                itoa_u((uint32_t)batt_pct(), c);
                for (k = 0; c[k]; k++)
                    ;
                c[k++] = '%', c[k] = 0;
            }
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_TEXT);
            x -= 8;
        }
        if (ui.dirty) {                            /* unsaved: a small disk (1.4.0, the user's: the dot read as nothing) */
            const int32_t dx = x - 9, dy = 9;
            cv_rect(dx, dy, 8, 10, K_DIM);         /* (its body, a corner cut at the top right) */
            cv_rect(dx + 8, dy + 2, 1, 8, K_DIM);
            cv_rect(dx + 2, dy, 4, 3, K_BG);       /* (the shutter's window, its slot lit) */
            cv_rect(dx + 4, dy, 1, 2, K_DIM);
            cv_rect(dx + 2, dy + 6, 5, 3, K_BG);   /* (the label) */
            x -= 15;
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load (now/peak), and any dropouts ("!n") */
            char c[24] = "CPU ";
            uint32_t k;
            itoa_u(plat_cpu_pct(), c + 4);
            for (k = 0; c[k]; k++)
                ;
            c[k++] = '/';
            itoa_u(plat_cpu_peak(), c + k);
            if (plat_xruns()) {
                for (; c[k]; k++)
                    ;
                c[k++] = ' ', c[k++] = '!';
                itoa_u(plat_xruns() > 999u ? 999u : plat_xruns(), c + k);
            }
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);   /* (the leftmost on the right: its changing width moves nothing) */
            x -= 4;
        } else if (en_ext) {                      /* an external clock leads: said, small */
            x -= text_w(&FONT_XS, "EXT");
            cv_text(x, 8, &FONT_XS, "EXT", K_DIM);
            x -= 6;
        }
        {   /* (1.4.0, the user's) dub hold, Lock and a solo: at the right end, in red (SOLO in the clap's colour) */
            static const char *const W[3] = {"SOLO", "LOCK", "HOLD"};
            const int on[3] = {ui.solo != 0, ui.lock != 0, en_frozen != 0};
            int k;
            for (k = 0; k < 3; k++)
                if (on[k]) {
                    x -= text_w(&FONT_XS, W[k]);
                    cv_text(x, 8, &FONT_XS, W[k], k ? K_FROST : K_CLAP);
                    x -= 6;
                }
        }
        if (clip) {                              /* (1.4.0, the user's) the master clipping: a red "!", a second; on
                                                  * a red tag of its own, not Setup's grey "!n" (the dropouts) */
            const int32_t cw = text_w(&FONT_B, "!") + 6;
            cv_round(x - 2 - cw, 5, cw, 18, 4, RGB(236, 64, 56));
            cv_text(x - 2 - cw + 3, 6, &FONT_B, "!", K_TEXT);
            x -= cw + 6;
        }
        {                                        /* the piece, in a coral tag (a Set's Transition cued: the one
                                                  * ending, its OUTRO, till the downbeat) */
            const char *pn = piece_name(en_tr_cue && ui.cue_from >= 0 ? ui.cue_from : proj.piece);
            w = text_w(&FONT_B, pn) + 12;
            cv_round(4, 4, w, 20, 6, K_TEXT);
            cv_text(10, 6, &FONT_B, pn, K_BG);
        }
        /* (1.4.0, the user's: the piece's name alone; its key on the Key page) */
        hdr_phrase(4 + w + 10, x - 6, now);       /* then where we are */
        if (song)
            hdr_song_line();
    }
    cv_blit(0, 0);
}

/* the main screen: echo ripples. Each stab drops a ring in the middle that spreads and fades; the echo's
 * repeats follow it, a ring each echo time, each as loud as the echo keeps of the one before (a throw: wider
 * and brighter; dub hold: red, and the repeats don't die). The chord's name in the middle. Around it: the
 * section and its eight bars (top left), the way to the next chord (top right), the four layers' levels
 * (left), the kick's ducking (right), and the bar as it plays, a lane a layer (below: stabs, kick, hats,
 * bass), the step playing lit */
#define CX 120
#define CY 48
#define RIP_N 10
#define RIP_R 124                                /* a ring is gone by this radius (1.4.0, the user's: the screen's sides
                                                  * free, the rings out to them; was 46) */
#define RIP_V 0.068f                             /* px a millisecond: ~1.8 s to the sides */
#define PIC_Y1 90                                /* (1.4.0) the pictures' last row + 1: the lanes below kept clear */
#define RIP_K 8                                  /* a ring and up to seven repeats */
#define RIP_W 64                                 /* a ring's outline: the sound's wave, round the circle */
static struct {
    uint32_t t0;
    float a[RIP_K], l1;                          /* each repeat's strength (a[0] the note), the first echo's level */
    uint8_t thr, on, nk, done;                   /* nk: the repeats measured so far; done: no more to come */
    uint8_t lay;                                 /* (1.4.0) its layer, its colour: 0 the chord, 1 the pings, 2 the bass, 3 the drums */
    int8_t w[RIP_W];
} rip[RIP_N];
/* the echo's output, as measured: its peak each frame, the last few frames (a repeat's level is the peak around
 * when it's due) */
#define EPK_N 16
static struct {
    uint32_t t;
    float pk;
} epk[EPK_N];
static uint8_t epk_w;
static uint32_t rip_stabs, rip_bass;            /* (rip_bass: the overview's bass notes, 1.4.0) */
static int8_t rip_src = -2;                      /* whose notes the ripples are: -1 the whole (the stabs), or a layer */
static int scr_layer(int v)                      /* a layer's own page (and the Noise page): the screen that layer's */
{
    return v == V_CHORD || v == V_TEXTURE || v == V_BASS || v == V_DRUMS || v == V_NSND || v == V_NVOX || v == V_NSND2 ||
           (v >= V_CSND && v <= V_DSND) || v == V_HATS ? oct_layer(v) : -1;                    /* (1.3.0: the sound pages too, live, not the envelope) */
}

static const char *chord_now(char *nm)          /* the chord's name, as the screen shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    if (en_chord_slot >= 0 && proj.par[P_CQ0 + en_chord_slot])   /* a slot's: its own name */
        return slot_label(en_chord_slot, nm);
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

/* a new stab: a new ripple (into the oldest slot if all are busy). On a layer's page, that layer's notes instead,
 * in its colour (the bass and the drums: a ring, no repeats; they hardly go into the echo) */
static int rip_slot(void)                       /* a free ring, or the oldest */
{
    int i, k = 0;
    for (i = 0; i < RIP_N; i++) {
        if (!rip[i].on)
            return i;
        if (rip[i].t0 < rip[k].t0)
            k = i;
    }
    return k;
}
static void rip_outline(int k)                  /* its outline: the wave as it sounds now (the last 128 of the scope's
                                                 * samples, every other one; drawn so its ends meet), at its own peak */
{
    int32_t v[RIP_W], pk = 1, j;
    uint32_t w0 = en_scope_w - 2u * RIP_W;
    for (j = 0; j < RIP_W; j++)
        v[j] = en_scope[(w0 + 2u * (uint32_t)j) & (EN_SCOPE_N - 1u)];
    for (j = 0; j < RIP_W; j++) {
        v[j] -= (v[RIP_W - 1] - v[0]) * j / (RIP_W - 1);
        pk = v[j] > pk ? v[j] : -v[j] > pk ? -v[j] : pk;
    }
    for (j = 0; j < RIP_W; j++)
        rip[k].w[j] = (int8_t)(v[j] * 127 / pk);
}
static void ripples_take(uint32_t now)
{
    const int src = scr_layer(ui.view);
    uint32_t n = src > 0 && src < 4 ? en_hits[src] : en_stabs, nb = en_hits[2];
    int i, k;
    if (src != rip_src) {                        /* (another page: the old rings gone) */
        rip_src = (int8_t)src;
        rip_stabs = n;
        rip_bass = nb;
        for (i = 0; i < RIP_N; i++)
            rip[i].on = 0;
        return;
    }
    if (src == 4)                                /* (the Noise pages: no rings, the noise's specks alone) */
        return;
    if (src < 0 && nb != rip_bass) {             /* (1.4.0, the user's: on the whole, the bass's notes too: a gold ring,
                                                  * no repeats) */
        rip_bass = nb;
        if ((ui.mutes & LAYER_MUTE[2]) != LAYER_MUTE[2]) {
            k = rip_slot();
            rip[k].t0 = now, rip[k].on = 1, rip[k].nk = 1, rip[k].l1 = 0.0f, rip[k].thr = 0, rip[k].done = 1;
            rip[k].lay = 2;
            rip[k].a[0] = fm_minf(en_hit_vel[2] * 0.8f, 1.0f);
            rip_outline(k);
        }
    }
    if (n == rip_stabs)
        return;
    rip_stabs = n;
    if (src >= 0 && (ui.mutes & LAYER_MUTE[src]) == LAYER_MUTE[src])   /* (a layer muted: no rings) */
        return;
    if (src == 4)                                /* (the Noise page shows its spectrum) */
        return;
    k = rip_slot();
    rip[k].t0 = now;
    rip[k].on = 1;
    rip[k].nk = 1;
    rip[k].l1 = 0.0f;
    rip[k].lay = (uint8_t)(src > 0 ? src : 0);
    if (src > 0) {                               /* (the pings, the bass, the drums) */
        rip[k].thr = 0;
        rip[k].a[0] = fm_minf(en_hit_vel[src] * 0.9f, 1.0f);
    } else {
        rip[k].thr = en_stab_thrown;
        rip[k].a[0] = fm_minf(en_stab_vel * (0.6f + 0.4f * en_echo_in) * (en_stab_thrown ? 1.5f : 1.0f), 1.0f);
    }
    /* repeats: from the echo as it comes out (none for the bass and the drums, hardly in it; none for a note into a
     * held echo, which plays what it holds) */
    rip[k].done = (uint8_t)(src == 2 || src == 3 || en_frozen || (src <= 0 && en_echo_in < 0.01f));
    rip_outline(k);
}

/* the echo's level this frame (its peak since the last); then each ring's next repeat, once it's due and past: the
 * echo's peak around then, against its first repeat's. So a filtered, ducked or dropped echo shows, and the feedback
 * as it really dies away */
static void echo_take(uint32_t now)
{
    const float ms = en_echo_ms > 20.0f ? en_echo_ms : 400.0f;
    int i, j;
    epk[epk_w].t = now;
    epk[epk_w].pk = en_echo_pk;
    en_echo_pk = 0.0f;
    epk_w = (uint8_t)((epk_w + 1u) % EPK_N);
    for (i = 0; i < RIP_N; i++) {
        uint32_t tk;
        float pk = 0.0f, a;
        if (!rip[i].on || rip[i].done)
            continue;
        tk = rip[i].t0 + (uint32_t)((float)rip[i].nk * ms);
        if ((int32_t)(now - tk) < 35)
            continue;
        for (j = 0; j < EPK_N; j++)
            if ((int32_t)(epk[j].t - tk) >= -30 && (int32_t)(epk[j].t - tk) <= 35 && epk[j].pk > pk)
                pk = epk[j].pk;
        if (rip[i].nk == 1)
            rip[i].l1 = pk;
        if (rip[i].l1 < 0.004f)                  /* (no echo to speak of) */
            a = 0.0f;
        else if (rip[i].nk == 1)
            a = rip[i].a[0] * 0.85f;
        else                                     /* (as the echo's level fell since the first; never louder) */
            a = fm_minf(rip[i].a[1] * pk / rip[i].l1, rip[i].a[rip[i].nk - 1]);
        rip[i].a[rip[i].nk] = a;
        if (++rip[i].nk >= RIP_K || a < 0.05f)
            rip[i].done = 1;
    }
}

static void circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)   /* an outline, one pixel (midpoint) */
{
    int32_t x = r, y = 0, e = 1 - r;
    while (x >= y) {
        cv_pset(cx + x, cy + y, c), cv_pset(cx - x, cy + y, c), cv_pset(cx + x, cy - y, c), cv_pset(cx - x, cy - y, c);
        cv_pset(cx + y, cy + x, c), cv_pset(cx - y, cy + x, c), cv_pset(cx + y, cy - x, c), cv_pset(cx - y, cy - x, c);
        y++;
        if (e < 0) {
            e += 2 * y + 1;
        } else {
            x--;
            e += 2 * (y - x) + 1;
        }
    }
}

static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c);

/* a ring: its outline the wave it was born with (smoothed a little more each repeat, as the tape darkens it), the
 * wave's depth with its strength, small near the middle */
static void wave_ring(float r, const float *w, float d, uint16_t c)
{
    static float cs[RIP_W], sn[RIP_W];
    int32_t x0 = 0, y0 = 0, x1, y1, xp = 0, yp = 0, j;
    if (cs[0] == 0.0f)
        for (j = 0; j < RIP_W; j++)
            cs[j] = fm_cosf(6.2831853f * (float)j / RIP_W), sn[j] = fm_sinf(6.2831853f * (float)j / RIP_W);
    d *= fm_minf(r * (1.0f / 14.0f), 1.0f) * (1.0f + r * (1.0f / 70.0f));   /* (deeper as they grow: 1.4.0) */
    for (j = 0; j <= RIP_W; j++) {
        float rr = r + d * w[j % RIP_W];
        x1 = CX + (int32_t)(rr * cs[j % RIP_W] + 0.5f);
        y1 = CY + (int32_t)(rr * sn[j % RIP_W] + 0.5f);
        if (j)
            line(xp, yp, x1, y1, c);
        else
            x0 = x1, y0 = y1;
        xp = x1, yp = y1;
    }
    (void)x0, (void)y0;
}

static int draw_ripples(uint32_t now)           /* 1: something still moves */
{
    static const uint16_t LC[4] = {K_PAD, K_BELL, K_BASS, K_PULSE};
    const float ms = en_echo_ms > 20.0f ? en_echo_ms : 400.0f;
    int i, live = 0;
    for (i = 0; i < RIP_N; i++) {
        float age = (float)(now - rip[i].t0), w[RIP_W], tmp[RIP_W];
        int k, j, p = 0, seen = 0;
        if (!rip[i].on)
            continue;
        for (j = 0; j < RIP_W; j++)
            w[j] = (float)rip[i].w[j] * (1.0f / 127.0f);
        for (k = 0; k < rip[i].nk; k++) {
            float t = age - (float)k * ms, r = 5.0f + t * RIP_V, a = rip[i].a[k] * (1.0f - r / (float)RIP_R);
            uint16_t c;
            for (; p < k; p++) {                 /* (a pass of the smoothing a repeat) */
                for (j = 0; j < RIP_W; j++)
                    tmp[j] = 0.25f * (w[(j + RIP_W - 1) % RIP_W] + 2.0f * w[j] + w[(j + 1) % RIP_W]);
                for (j = 0; j < RIP_W; j++)
                    w[j] = tmp[j];
            }
            if (t < 0.0f || r >= (float)RIP_R || a < 0.05f)
                continue;
            seen = 1;
            c = mix(K_BG, en_frozen && rip_src < 2 && rip[i].lay < 2 ? K_FROST : rip[i].thr ? K_ACC : LC[rip[i].lay & 3], q16(a * 2.0f));
            wave_ring(r, w, 4.0f * rip[i].a[k], c);   /* two pixels wide (a throw: three) */
            wave_ring(r + 1.0f, w, 4.0f * rip[i].a[k], c);
            if (rip[i].thr)
                wave_ring(r + 2.0f, w, 4.0f * rip[i].a[k], c);
        }
        if (!seen && rip[i].done && age > 200.0f)
            rip[i].on = 0;                       /* its last repeat has faded */
        else
            live = 1;
    }
    return live;
}

/* the noise in the ripples: a speck for each click that shows (en_nev_*), somewhere round the middle, on its side,
 * as big and bright as it sounds, fading fast; a pop brighter and bigger; a dust grain a soft, slower blob */
#define SPK_N 24
static struct {
    uint32_t t0;
    int16_t x, y;
    uint8_t k, s;
} spk[SPK_N];
static uint32_t spk_r, spk_rng = 12345u;
static void specks_take(uint32_t now)
{
    uint32_t w = en_nev_w;
    if (w - spk_r > EN_NEV_N)                    /* (too far behind: the newest only) */
        spk_r = w - EN_NEV_N;
    for (; spk_r != w; spk_r++) {
        int i, o = 0;
        float a, r, p;
        for (i = 0; i < SPK_N; i++)              /* (into the oldest) */
            if (spk[i].t0 < spk[o].t0)
                o = i;
        spk_rng = spk_rng * 1664525u + 1013904223u;
        a = (float)(spk_rng >> 8) * (6.2831853f / 16777216.0f);
        spk_rng = spk_rng * 1664525u + 1013904223u;
        r = 12.0f + 84.0f * (float)(spk_rng >> 8) * (1.0f / 16777216.0f);   /* (1.4.0: out to the sides) */
        p = (float)en_nev_p[spk_r % EN_NEV_N] * (1.0f / 255.0f) - 0.5f;
        spk[o].t0 = now | 1u;
        spk[o].x = (int16_t)(CX + (int32_t)(r * fm_cosf(a) + 60.0f * p));
        spk[o].y = (int16_t)(CY + (int32_t)(r * 0.44f * fm_sinf(a)));
        spk[o].k = en_nev_k[spk_r % EN_NEV_N];
        spk[o].s = en_nev_s[spk_r % EN_NEV_N];
    }
}
static int draw_specks(uint32_t now)              /* 1: one still shows */
{
    int i, live = 0;
    for (i = 0; i < SPK_N; i++) {
        uint32_t age = now - spk[i].t0, life = spk[i].k == 2 ? 700u : spk[i].k == 1 ? 500u : 350u;
        float s = (float)spk[i].s * (1.0f / 255.0f), f;
        if (!spk[i].t0 || age >= life)
            continue;
        live = 1;
        f = (0.45f + 0.55f * fm_minf(s * 2.0f, 1.0f)) * (1.0f - (float)age / (float)life);
        if (spk[i].k == 2)
            cv_disc(spk[i].x, spk[i].y, 3, mix(K_BG, K_NOISE, q16(f * 0.5f)));
        else
            cv_disc(spk[i].x, spk[i].y, spk[i].k == 1 ? 3 : 2, mix(K_BG, spk[i].k == 1 ? K_TEXT : K_NOISE, q16(f)));
    }
    return live;
}

static void draw_orbits(void)                    /* the ping loops, around the ripples: a dot each, going round */
{
    int i;
    for (i = 0; i < EN_NLOOP; i++) {             /* (a loop a step a turn's step; the loops of unequal length) */
        float a = 6.2831853f * (float)en_loop_pos[i] / (float)EN_LOOP_LEN[i] - 1.5707963f + (float)i * 1.9f,
              rr = 30.0f + 13.0f * (float)i, g = en_loop_glow[i];   /* (1.4.0: wider, flattened: the room's to the sides) */
        int32_t x = CX + (int32_t)(rr * fm_cosf(a) + 0.5f), y = CY + (int32_t)(0.6f * rr * fm_sinf(a) + 0.5f);
        if (!en_playing)
            g = 0.0f;
        cv_disc(x, y, g > 0.5f ? 3 : 2, mix(mix(K_BELL_T, K_BELL, 5), K_BELL, q16(g * 1.3f)));
    }
}

/* the middle as a scope: the output (mono), from a rising zero crossing so it stands, a thick line with a
 * glow over the two traces before it, fading; its height following the level shown (the gain rises for a
 * quiet moment) */
#define SC_X 8                                   /* (1.4.0, the user's: nearly the screen's width; was 128 wide at 56) */
#define SC_W 224
#define SC_Y 59
#define SC_H 28
#define SC_STEP 0.75f                            /* (samples a pixel: 168 of the 256 shown, room left to find a crossing) */
static void draw_scope(void)
{
    static int16_t s[EN_SCOPE_N];
    static int8_t was[2][SC_W];                  /* the last two traces, fading (a phosphor's) */
    static float gain = 4.0f;
    const uint16_t c = en_frozen ? K_FROST : K_PAD, gl = mix(K_BG, c, 5), ph[2] = {mix(K_BG, c, 6), mix(K_BG, c, 3)};
    uint32_t w = en_scope_w;
    int i, t0 = 0, m = 1;
    int32_t x;
    for (i = 0; i < EN_SCOPE_N; i++)             /* (the oldest first) */
        s[i] = en_scope[(w + (uint32_t)i) & (EN_SCOPE_N - 1u)];
    for (i = 2; i < EN_SCOPE_N - (int)(SC_W * SC_STEP) - 2; i++)
        if (s[i - 1] < 0 && s[i] >= 0 && s[i + 1] - s[i - 2] > 200) {
            t0 = i;
            break;
        }
    for (x = 0; x < (int32_t)(SC_W * SC_STEP); x++)
        m = s[t0 + x] > m ? s[t0 + x] : -s[t0 + x] > m ? -s[t0 + x] : m;
    gain += (fm_minf(0.85f * 32000.0f / (float)(m > 600 ? m : 600), 30.0f) - gain) * 0.25f;
    for (x = 0; x <= SC_W; x += 4)               /* the graticule: the middle dotted, ticks along the edges */
        cv_rect(SC_X + x, SC_Y, 1, 1, K_LINE);
    for (x = 0; x <= SC_W; x += SC_W / 8) {
        cv_rect(SC_X + x, SC_Y - SC_H - 2, 1, 3, K_LINE);
        cv_rect(SC_X + x, SC_Y + SC_H, 1, 3, K_LINE);
    }
    for (i = 1; i >= 0; i--)                     /* the traces before, older dimmer */
        for (x = 1; x < SC_W; x++)
            line(SC_X + x - 1, SC_Y - was[i][x - 1], SC_X + x, SC_Y - was[i][x], ph[i]);
    for (x = 0; x < SC_W; x++) {
        const float u = (float)x * SC_STEP, f = u - (float)(int)u;
        const int k = t0 + (int)u;
        float v = ((float)s[k] + f * (float)(s[k + 1] - s[k])) * (1.0f / 32000.0f) * gain;
        was[1][x] = was[0][x];
        was[0][x] = (int8_t)(fm_clampf(v, -1.0f, 1.0f) * (float)SC_H + (v < 0.0f ? -0.5f : 0.5f));
    }
    for (x = 1; x < SC_W; x++) {                 /* the trace: a glow, then two pixels bright */
        int32_t y0 = SC_Y - was[0][x - 1], y1 = SC_Y - was[0][x];
        line(SC_X + x - 1, y0 - 1, SC_X + x, y1 - 1, gl);
        line(SC_X + x - 1, y0 + 2, SC_X + x, y1 + 2, gl);
    }
    for (x = 1; x < SC_W; x++) {
        int32_t y0 = SC_Y - was[0][x - 1], y1 = SC_Y - was[0][x];
        line(SC_X + x - 1, y0, SC_X + x, y1, c);
        line(SC_X + x - 1, y0 + 1, SC_X + x, y1 + 1, c);
    }
}

/* the middle as a spectrum (a Winamp's): the output's last 512 samples at 22 kHz, windowed and transformed,
 * in 18 bands a third of an octave or more wide, 43 Hz to 11 kHz; each a column of segments, teal low, brass,
 * amber, red at the top, rising at once and falling slowly, a cap held at its peak a moment */
#define SP_NB 18
#define SP_X 13                                  /* (1.4.0, the user's: nearly the screen's width; was 126 wide at 58) */
#define SP_PITCH 12                              /* (a band's column and its gap) */
#define SP_BW 10
#define SP_BOT 88
#define SP_SEG 18                                /* segments a column, 3 px each */
static void fft512(float *re, float *im)         /* in place, radix 2 */
{
    int n = EN_SPEC_N, i, j = 0, len, k;
    for (i = 1; i < n; i++) {                    /* the bit-reversed order */
        int b = n >> 1;
        for (; j & b; b >>= 1)
            j ^= b;
        j ^= b;
        if (i < j) {
            float t = re[i];
            re[i] = re[j], re[j] = t;
            t = im[i];
            im[i] = im[j], im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        float a = -6.2831853f / (float)len, wr = fm_cosf(a), wi = fm_sinf(a);
        for (i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (k = 0; k < len / 2; k++) {
                int p = i + k, q = p + len / 2;
                float tr = re[q] * cr - im[q] * ci, ti = re[q] * ci + im[q] * cr, nr;
                re[q] = re[p] - tr, im[q] = im[p] - ti;
                re[p] += tr, im[p] += ti;
                nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

static void draw_spec(void)
{
    static const uint16_t EDGE[SP_NB + 1] = {1, 2, 3, 4, 5, 6, 7, 9, 12, 16, 22, 30, 40, 55, 75, 102, 138, 188, 256};
    float *const re = app_scratch.fft[0], *const im = app_scratch.fft[1];   /* (a frame's: the scratch's, app.h) */
    static float lv[SP_NB], cap[SP_NB];
    static uint8_t hold[SP_NB];
    uint32_t w = en_spec_w;
    int i, b, g;
    for (i = 0; i < EN_SPEC_N; i++) {            /* (the oldest first, a Hann window) */
        float h = 0.5f - 0.5f * fm_cosf(6.2831853f * (float)i * (1.0f / (float)(EN_SPEC_N - 1)));
        re[i] = (float)en_spec[(w + (uint32_t)i) & (EN_SPEC_N - 1u)] * (1.0f / 32000.0f) * h;
        im[i] = 0.0f;
    }
    fft512(re, im);
    for (b = 0; b < SP_NB; b++) {
        float pw = 1e-9f, db, f;
        int32_t x = SP_X + b * SP_PITCH, n;
        for (i = EDGE[b]; i < EDGE[b + 1]; i++)
            pw += re[i] * re[i] + im[i] * im[i];
        db = 3.0103f * fm_log2f(pw) - 42.1f + 1.5f * (float)b;   /* dB of full scale, tilted up 1.5 dB a band */
        f = fm_clampf((db + 56.0f) * (1.0f / 48.0f), 0.0f, 1.0f);
        if (!en_playing)
            f = 0.0f;
        lv[b] = f > lv[b] ? f : lv[b] - 0.04f;   /* up at once, down slowly */
        if (lv[b] >= cap[b]) {
            cap[b] = lv[b];
            hold[b] = 12;                        /* (frames: ~0.6 s) */
        } else if (hold[b]) {
            hold[b]--;
        } else {
            cap[b] -= 0.025f;
        }
        n = (int32_t)(lv[b] * (float)SP_SEG + 0.5f);
        for (g = 0; g < SP_SEG; g++) {
            uint16_t c = g >= SP_SEG - 3 ? K_FROST : g >= SP_SEG * 2 / 3 ? K_PULSE : g >= SP_SEG / 3 ? K_BASS : K_PAD;
            if (en_frozen)
                c = K_FROST;
            cv_rect(x, SP_BOT - 3 * g - 2, SP_BW, 2, g < n ? c : g == 0 ? K_LINE : K_BG);
        }
        if (cap[b] > 0.03f)
            cv_rect(x, SP_BOT - 3 * (int32_t)(cap[b] * (float)SP_SEG + 0.5f) - 1, SP_BW, 1, K_TEXT);
    }
}

/* a sound page: its layer's envelope (its level; the chord's filter envelope, the pings' bend, the kick's
 * pitch, dimmer), over the time it lasts */
static void draw_env(int layer_view)
{
    float lv[48], ax[48], secs;
    int i, layer = layer_view == V_CHORD ? 0 : layer_view == V_TEXTURE ? 1 : layer_view == V_BASS ? 2 : 3;
    int32_t x0 = 58, w = 124, y0 = 8, h = 76;
    char t[12];
    en_env_shape(layer, lv, ax, 48, &secs);
    cv_rect(x0, y0 + h, w + 4, 1, K_LINE);
    cv_rect(x0, y0, 1, h, K_LINE);
    for (i = 0; i < 48; i++) {
        int32_t x = x0 + 2 + i * w / 47, ya = y0 + h - (int32_t)(ax[i] * (float)h), yl = y0 + h - (int32_t)(lv[i] * (float)h);
        if (ax[i] > 0.01f)
            cv_rect(x, ya, 2, 2, K_LINE);
        cv_rect(x, yl, 2, y0 + h - yl, mix(K_BG, K_PAD, 5));
        cv_rect(x, yl, 2, 2, K_PAD);
    }
    itoa_u((uint32_t)(secs * 1000.0f + 0.5f), t);
    {
        uint32_t k = 0;
        while (t[k])
            k++;
        t[k++] = ' ', t[k++] = 'm', t[k++] = 's', t[k] = 0;
    }
    cv_text(x0 + w - text_w(&FONT_XS, t), y0 + h + 1, &FONT_XS, t, K_DIM);
}

/* an FM page: the voice's algorithm, as the FM synths draw it: the operators as boxes, each modulator above
 * the one it modulates, the carriers on the bottom line (they're heard), op 4's feedback a loop */
static const int8_t ALG_XY[EN_FM_NALG][4][2] = {   /* each op's place (x in half steps from the middle, y up) */
    {{0, 0}, {0, 1}, {0, 2}, {0, 3}},   {{0, 0}, {0, 1}, {-1, 2}, {1, 2}}, {{0, 0}, {-1, 1}, {-1, 2}, {1, 1}},
    {{0, 0}, {-1, 1}, {1, 1}, {1, 2}},  {{-1, 0}, {-1, 1}, {1, 0}, {1, 1}}, {{-2, 0}, {0, 0}, {2, 0}, {0, 1}},
    {{-2, 0}, {0, 0}, {2, 0}, {2, 1}},  {{-3, 0}, {-1, 0}, {1, 0}, {3, 0}}, {{0, 0}, {0, 1}, {0, -9}, {0, -9}},
};
static const uint16_t ALG_EDGE[EN_FM_NALG] = {   /* bit 4 * from + to (ops 0..3): from modulates to */
    1u << (4 * 1 + 0) | 1u << (4 * 2 + 1) | 1u << (4 * 3 + 2), 1u << (4 * 1 + 0) | 1u << (4 * 2 + 1) | 1u << (4 * 3 + 1),
    1u << (4 * 1 + 0) | 1u << (4 * 2 + 1) | 1u << (4 * 3 + 0), 1u << (4 * 1 + 0) | 1u << (4 * 2 + 0) | 1u << (4 * 3 + 2),
    1u << (4 * 1 + 0) | 1u << (4 * 3 + 2),                     1u << (4 * 3 + 0) | 1u << (4 * 3 + 1) | 1u << (4 * 3 + 2),
    1u << (4 * 3 + 2),                                         0u,
    1u << (4 * 1 + 0),
};
static void line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* Bresenham */
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1,
            sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        int32_t e2 = 2 * e;                      /* (taken before either step: else a short slope can overshoot) */
        cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            e += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}
static void draw_alg(int layer)
{
    static const char *const D[4] = {"1", "2", "3", "4"};
    int alg = en_fm_alg(layer), k, f, t;
    int32_t px[4], py[4];
    char tt[12];
    if (!en_voice_fm(layer)) {                   /* the FM voices, to choose with ALGORITHM */
        char vn[12], t[24];
        en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
        caps(t, vn, 12);
        text_c(120, 28, &FONT_B, t, K_TEXT);
        text_c(120, 48, &FONT_XS, layer == 0 ? "IS NOT FM: ITS COLOUR ALONE" : "IS NOT FM: ITS BITE ALONE", K_DIM);   /* (1.4.0,
                                                  * the user's: the chord's Colour here, the bass's Bite, every voice's) */
        text_c(120, 66, &FONT_XS, "ALGORITHM, FOR ONE:", K_DIM);
        text_c(120, 80, &FONT_XS, layer == 0 ? "KEYS, BELL" : "PUNCH, RUBBER, PLUCK, GROWL", K_ACC);
        return;
    }
    for (k = 0; k < 4; k++) {
        px[k] = 120 + ALG_XY[alg][k][0] * 17;
        py[k] = 80 - ALG_XY[alg][k][1] * 18;
    }
    for (f = 0; f < 4; f++)                      /* the connections, under the boxes */
        for (t = 0; t < 4; t++)
            if (ALG_EDGE[alg] >> (4 * f + t) & 1u)
                line(px[f], py[f] + 7, px[t], py[t] - 7, K_DIM);
    for (k = 0; k < 4; k++) {
        int carrier = ALG_XY[alg][k][1] == 0;
        if (ALG_XY[alg][k][1] < 0)
            continue;                            /* (two operators: 3 and 4 unused) */
        if (carrier)
            line(px[k], py[k] + 7, px[k], 91, K_PAD);
        cv_round(px[k] - 7, py[k] - 7, 15, 15, 3, carrier ? K_PAD : K_BELL);
        text_c(px[k] + 1, py[k] - 7, &FONT_XS, D[k], K_BG);
    }
    if (alg != 8) {                              /* op 4's feedback */
        line(px[3] + 7, py[3] - 3, px[3] + 11, py[3] - 3, K_DIM);
        line(px[3] + 11, py[3] - 3, px[3] + 11, py[3] - 10, K_DIM);
        line(px[3] + 11, py[3] - 10, px[3], py[3] - 10, K_DIM);
        line(px[3], py[3] - 10, px[3], py[3] - 7, K_DIM);
    }
    cv_rect(62, 91, 117, 1, K_PAD);
    tt[0] = 'A', tt[1] = 'L', tt[2] = 'G', tt[3] = 'O', tt[4] = ' ';
    itoa_u((uint32_t)(alg + 1), tt + 5);
    if (alg == 8) {
        tt[5] = '2', tt[6] = '-', tt[7] = 'O', tt[8] = 'P', tt[9] = 0;
    }
    text_c(120, MAIN_SHIFT - 1, &FONT_XS, tt, K_DIM);   /* (where it was on the screen: the canvas's top, 1.4.0) */
}

/* (1.4.0, the user's: "the levels are of course the most important part") the Arrangement page: a desk's meters, a
 * row a part (the chord, the pings, the bass, the kick, the hats, the noise: white 1-6) and the master under them, LED
 * segments left to right, -48 .. 0 dB, 2 dB a segment; the peak held a second, then falling back; the part's level
 * (its strip's Level; OCT on its page) a fader's marker on its row; a part muted dark, soloed out dimmed; under the master the duck, as a
 * compressor's gain reduction, eating segments from the right. The other pages' level strips and duck meter gone (a
 * red dot in the header if the master clips). Two builds to try on the FM-1, the colours apart (EN_METER_CLASSIC):
 * 1 green, yellow, red on every row; 0 each row its layer's colour, its top two amber and red */
#ifndef EN_METER_CLASSIC
#define EN_METER_CLASSIC 1
#endif
#define MT_SEG 24                                /* segments a row, MT_DB dB each, up to 0 dB */
#define MT_DB 2.0f
#define MT_X0 40                                 /* the first segment's left; MT_P apart, MT_W wide */
#define MT_P 8
#define MT_W 6
#define MT_Y0 32                                 /* the first row; MT_ROW apart, MT_H high (1.4.0: six parts, the
                                                  * master and the duck: a little closer) */
#define MT_ROW 14
#define MT_H 10
static struct {
    float db[EN_NMETER], hold[EN_NMETER];        /* each meter as shown (dB), its peak held */
    uint32_t hold_t[EN_NMETER];
    float gr;                                    /* the duck's gain reduction as shown, dB */
    float dr[3];                                 /* (1.4.0) the lanes' drum rows (the kick, the hats, the rim and clap),
                                                  * from their hits, dB */
    uint8_t clap;                                /* (the R/C row: the clap the last to hit) */
    uint32_t t, clip_t;                          /* the last frame; the master's last clip (0: none yet) */
} mt;
static float mt_db(float pk) { return pk > 1e-6f ? 6.0206f * fm_log2f(pk) : -120.0f; }
static int clip_on(uint32_t now) { return mt.clip_t && now - mt.clip_t < 1000u; }
static void meter_take(uint32_t now)             /* every frame: the engine's peaks since the last, into the meters:
                                                  * at once up, 20 dB a second down; the hold a second, then 30 dB a
                                                  * second down; the clip: the master at full scale, lit a second */
{
    const float dt = mt.t ? (float)(now - mt.t) * 0.001f : 0.0f;
    int i;
    mt.t = now;
    en_meter_on = 1;                             /* (1.4.0, the user's: the parts measured on every page now, the lanes' meters at
                                                  * the left theirs) */
    for (i = 0; i < EN_NMETER; i++) {
        const float pk = en_meter[i], d = mt_db(pk);
        en_meter[i] = 0.0f;
        if (i == EN_MT_MASTER && pk >= 1.0f)
            mt.clip_t = now | 1u;
        mt.db[i] = d > mt.db[i] ? d : fm_maxf(mt.db[i] - 20.0f * dt, -120.0f);
        if (d >= mt.hold[i]) {
            mt.hold[i] = d;
            mt.hold_t[i] = now;
        } else if (now - mt.hold_t[i] > 1000u) {
            mt.hold[i] = fm_maxf(mt.hold[i] - 30.0f * dt, mt.db[i]);
        }
    }
    {   /* (1.4.0, the user's) the lanes' drum rows, each from its own hits (the kick; the shaker and the open hat; the
         * rim and the clap): at once up, 48 dB a second down (a hit seen, then gone) */
        const float lv = (float)proj.par[P_DRUMS] * 0.01f,
                    pk[3] = {en_drum_glow[0] * lv, fm_maxf(en_drum_glow[1], en_drum_glow[2]) * lv * 0.5f,
                             fm_maxf(en_drum_glow[3], en_drum_glow[4]) * lv * 0.6f};
        for (i = 0; i < 3; i++) {
            const float d = mt_db(pk[i]);
            mt.dr[i] = d > mt.dr[i] ? d : fm_maxf(mt.dr[i] - 48.0f * dt, -120.0f);
        }
        if (en_drum_glow[3] > 0.9f || en_drum_glow[4] > 0.9f)
            mt.clap = en_drum_glow[4] > en_drum_glow[3];
    }
    {   /* the duck: on the bass while it plays (the deepest, the pump), else on the rest of the music (a few dB) */
        const float g = en_duck_lo[mt.db[EN_MT_BASS] > -60.0f ? 1 : 0], d = -mt_db(fm_maxf(g, 0.01f));
        en_duck_lo[0] = en_duck_lo[1] = 1.0f;
        mt.gr = d > mt.gr ? d : fm_maxf(mt.gr - 80.0f * dt, 0.0f);
    }
}
static int mt_lit(float db)                      /* the segments lit for a level */
{
    int n = db > -MT_DB * MT_SEG ? (int)((db + MT_DB * MT_SEG) * (1.0f / MT_DB)) + 1 : 0;
    return n > MT_SEG ? MT_SEG : n;
}
static uint16_t mt_col(int row, int i)           /* a segment's colour, lit */
{
#if EN_METER_CLASSIC
    (void)row;
    return i >= MT_SEG - 2 ? RGB(236, 64, 56) : i >= MT_SEG - 6 ? RGB(236, 210, 80) : RGB(84, 204, 108);
#else
    static const uint16_t C[EN_NMETER] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_TEXT, K_NOISE, K_TEXT};
    return i == MT_SEG - 1 ? RGB(236, 64, 56) : i == MT_SEG - 2 ? RGB(248, 196, 56) : C[row];
#endif
}
static const char *const MT_TAG[EN_NMETER] = {"CHD", "PNG", "BAS", "KCK", "HAT", "NOI", "MST"};
static const uint16_t MT_TAGC[EN_NMETER] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_TEXT, K_NOISE, K_TEXT};
/* (1.4.0, the user's: six parts, as white 1-6 and the strips: each row its part's mute and level, OCT's tables' 0, 1, 2,
 * the Kick and the Hats strips' 5, 6, the noise's 4) */
static const int8_t MT_OCT[EN_NMETER] = {0, 1, 2, 5, 6, 4, -1};
static int mute_state(uint32_t m)                /* 0 heard, 1 soloed out (dimmed), 2 muted (dark): the parts m */
{
    const uint32_t eng = ui.mutes | (ui.solo ? 0x3Fu & ~ui.solo : 0u);
    return !m ? 0 : (ui.mutes & m) == m ? 2 : (eng & m) == m ? 1 : 0;
}
static int mt_state(int row) { return mute_state(MT_OCT[row] >= 0 ? LAYER_MUTE[MT_OCT[row]] : 0u); }
static int32_t mt_y(int row) { return MT_Y0 + row * MT_ROW + (row == EN_MT_MASTER ? 4 : 0); }
static int32_t mt_mark(int row)                  /* the level's marker: its x (-1: none, the master's on its pot) */
{
    if (MT_OCT[row] < 0)
        return -1;
    {   /* (1.4.0, the user's: the dub mixer's ride moves it, where its level is now) */
        const float v = fm_clampf(ride_knob(row, 0, (float)proj.par[LAYER_LEVEL[MT_OCT[row]]]), 0.0f, 100.0f);
        return MT_X0 + (int32_t)((v * (float)(MT_SEG * MT_P - MT_P + MT_W - 2) + 50.0f) * 0.01f);
    }
}
static uint32_t desk_sig(void)                   /* what the meters show, for the redraw */
{
    uint32_t h = 2166136261u;
    int r;
    for (r = 0; r < EN_NMETER; r++)
        h = hash(h, (uint32_t)mt_lit(mt.db[r]) | (uint32_t)mt_lit(mt.hold[r]) << 6 | (uint32_t)mt_state(r) << 12 |
                    (uint32_t)(mt_mark(r) + 1) << 16);
    return hash(h, (uint32_t)(mt.gr * (1.0f / MT_DB) + 0.5f));
}
static void draw_desk(void)
{
    int r, i;
    for (r = 0; r < EN_NMETER; r++) {
        const int32_t y = mt_y(r), mx = mt_mark(r);
        const int st = mt_state(r), n = st == 2 ? 0 : mt_lit(mt.db[r]), hs = st == 2 ? -1 : mt_lit(mt.hold[r]) - 1;
        if (ui.view == V_STRIP && r == strip_part) {   /* (1.4.0) a strip on the knobs: its part's row lit behind */
            cv_round(3, y - 2, MT_X0 + MT_SEG * MT_P - 1, MT_H + 4, 3, K_PANEL);
            cv_rect(3, y - 2, 2, MT_H + 4, MT_TAGC[r]);
        }
        cv_text(8, y - 3, &FONT_XS, MT_TAG[r], st == 2 ? K_LINE : st == 1 ? mix(K_BG, MT_TAGC[r], 8) : MT_TAGC[r]);
        for (i = 0; i < MT_SEG; i++) {
            const uint16_t c = mt_col(r, i);
            uint16_t f;
            if (st == 2)
                f = mix(K_BG, K_LINE, 5);
            else if (i < n || i == hs)
                f = st == 1 ? mix(K_BG, c, 8) : c;
            else
                f = mix(K_BG, c, st == 1 ? 2 : 3);
            cv_rect(MT_X0 + i * MT_P, y, MT_W, MT_H, f);
        }
        if (mx >= 0) {                           /* the fader's marker: a notch through the row, out above and below */
            const uint16_t mc = st == 2 ? K_DIM : K_TEXT;
            cv_rect(mx - 1, y - 2, 4, MT_H + 4, K_BG);
            cv_rect(mx, y - 2, 2, MT_H + 4, mc);
        }
    }
    {   /* under the master: the duck (its gain reduction), from the right, a segment each MT_DB; a row as the others
         * (1.4.0, the user's: named DUCK, as tall) */
        const int32_t y = mt_y(EN_MT_MASTER) + MT_ROW;
        const int g = (int)(mt.gr * (1.0f / MT_DB) + 0.5f);
        cv_text(8, y - 3, &FONT_XS, "DUCK", proj.par[P_DUCK] ? K_PULSE : K_LINE);
        for (i = 0; i < MT_SEG; i++)
            cv_rect(MT_X0 + i * MT_P, y, MT_W, MT_H, i >= MT_SEG - g ? K_PULSE : mix(K_BG, K_PULSE, 3));
    }
}

/* (1.4.0, the user's) the lanes' seven rows, top to bottom: CHD the stabs, PNG the pings, BAS the bass, KCK the kick,
 * HAT the hats, R/C the rim and the clap, NOI the noise; at each row's left a short level meter, in its colour, its
 * track dim (the chord, the pings, the bass and the noise their meters', -48..0 dB; the kick, the hats, the rim and
 * clap from their own hits); a part muted dark, soloed out dimmed. On the overview with the bar's notes; on a voice
 * page the meters alone, its own row(s) bright, the others dimmed, its lane to the right */
#define LN_ROWS 7
#define LN_Y0 94                                 /* the first row's top; LN_ROW apart, the notes LN_H high */
#define LN_ROW 7
#define LN_H 5
#define LN_X0 40                                 /* the overview's grid: step i at LN_XI(i), 12.4 px apart, the last
                                                  * ending 8 px from the right as the meters start 8 px from the left
                                                  * (1.4.0, the user's: the duck bar's old room filled) */
#define LN_XI(i) (LN_X0 + (int32_t)(i) * 186 / 15)
#define LM_X 8                                   /* the meters: LM_W wide, LM_H high, in the row's middle */
#define LM_W 22
#define LM_H 4                                   /* (1.4.0, the user's: 1 px taller) */
#define VX 50                                    /* a voice page's lane: its grid, right of the meters */
static const uint32_t LN_MUTE[LN_ROWS] = {EN_M_CHORD, EN_M_PINGS, EN_M_BASS, EN_M_KICK, EN_M_HATS, EN_M_HATS, EN_M_NOISE};
static uint16_t ln_col(int r) { static const uint16_t C[LN_ROWS] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_TEXT, K_RIM, K_NOISE};
                                return r == 5 && mt.clap ? K_CLAP : C[r]; }
static int ln_own(int r)                         /* a voice page's row(s): lit (the overview: all) */
{
    static const int8_t LAY[LN_ROWS] = {0, 1, 2, 3, 3, 3, 4};
    const int l = oct_layer(ui.view);
    return l < 0 || LAY[r] == l;
}
static int lm_lit(int r)                         /* a row's meter: its pixels lit */
{
    static const int8_t MT[LN_ROWS] = {EN_MT_CHORD, EN_MT_PINGS, EN_MT_BASS, -1, -1, -1, EN_MT_NOISE};
    const float db = MT[r] >= 0 ? mt.db[MT[r]] : mt.dr[r - 3];
    return mute_state(LN_MUTE[r]) == 2 ? 0 : (int)(fm_clampf((db + 48.0f) * (1.0f / 48.0f), 0.0f, 1.0f) * LM_W + 0.5f);
}
static uint32_t lm_sig(void)                     /* what the meters show, for the redraw */
{
    uint32_t h = 2166136261u;
    int r;
    for (r = 0; r < LN_ROWS; r++)
        h = hash(h, (uint32_t)lm_lit(r) | (uint32_t)mute_state(LN_MUTE[r]) << 8);
    return hash(h, mt.clap);
}
static void draw_lmeters(void)
{
    int r;
    for (r = 0; r < LN_ROWS; r++) {
        const int32_t y = LN_Y0 + r * LN_ROW + (LN_H - LM_H) / 2, n = lm_lit(r);
        const int st = mute_state(LN_MUTE[r]), own = ln_own(r);
        const uint16_t c = ln_col(r);
        if (st == 2) {                           /* (muted: dark) */
            cv_rect(LM_X, y, LM_W, LM_H, mix(K_BG, K_LINE, 5));
            continue;
        }
        cv_rect(LM_X, y, LM_W, LM_H, mix(K_BG, c, own && st == 0 ? 4 : 2));
        if (n > 0)
            cv_rect(LM_X, y, n, LM_H, st == 1 || !own ? mix(K_BG, c, own ? 8 : 7) : c);
    }
}

/* below: the bar, a lane a layer. On a layer's page (and its sound pages) its lanes as ever, the others in grey (the
 * pings and the noise have none: all grey) */
/* (1.4.0, the user asked: "show only the line that matters, the current voice, even make it bigger") the bass's page:
 * its lane alone, large. Each note at its pitch against the root's (the fifth above or below, the octave note up), as
 * long as it holds: this bar's filled, the groove's others (left out by Density, or this bar's variant) as outlines
 * as long as they would be; a turn of Groove or Density shows at once; the playhead's column; A or B (bars 4 and 8,
 * the phrase's variant) at the left, level with the root */
#define BASS_ROOT_Y 120                          /* (the root's row; 1.3 px a semitone, up and down) */
/* (1.4.0, the user's) the phrase's two bars, A and B, a column at the lane's left: the one playing lit in the
 * layer's colour, the other grey */
static void phrase_ab(int b, uint16_t col)
{
    text_c(VX - 11, 99, &FONT_C, "A", b ? K_LINE : col);
    text_c(VX - 11, 119, &FONT_C, "B", b ? col : K_LINE);
}
static int bass_len(uint32_t notes, int i, float gate)   /* a note's length in steps: its gate, to the next note */
{
    int j, len = (int)(gate + 0.5f);
    for (j = i + 1; j < 16 && !(notes >> j & 1u); j++)
        ;
    if (len > j - i)
        len = j - i;
    return len < 1 ? 1 : len;
}
static void draw_bass_lane(void)
{
    const uint32_t bar = en_lane[5], groove = en_groove(en_groove_pat(proj.par[P_GROOVE]));
    const int pos = en_drum_pos[0], b = en_phrase_bar == 3 || en_phrase_bar == 7;
    const float gate = en_bass_gate;
    int i, playing = -1;
    if (en_playing)
        cv_round(VX - 2 + pos * 11, 98, 10, 46, 2, K_PANEL);
    phrase_ab(b, K_BASS);
    for (i = 0; i < 16; i++)                     /* the beats: the grid under it */
        cv_rect(VX + i * 11 + 2, 140, 2, (i & 3) ? 2 : 4, (i & 3) ? K_LINE : K_DIM);
    for (i = 0; i < 16; i++)                     /* the note playing: the one whose length covers the playhead */
        if (bar >> i & 1u && en_playing && pos >= i && pos < i + bass_len(bar, i, gate))
            playing = i;
    for (i = 0; i < 16; i++) {
        const int32_t x = VX + i * 11, y = BASS_ROOT_Y - (int32_t)((float)en_bass_off[i] * 1.3f) - 5;
        if (bar >> i & 1u) {                     /* a note this bar plays */
            cv_round(x, y, bass_len(bar, i, gate) * 11 - 3, 10, 4, i == playing ? K_BASS : mix(K_BASS_T, K_BASS, 9));
        } else if (groove >> i & 1u) {           /* the groove's, not played this bar: an outline, as long as it'd be */
            const int32_t w = bass_len(groove, i, gate) * 11 - 3;
            const uint16_t c = mix(K_BG, K_BASS_T, 14);
            cv_rect(x + 2, y, w - 4, 1, c), cv_rect(x + 2, y + 9, w - 4, 1, c);
            cv_rect(x, y + 2, 1, 6, c), cv_rect(x + w - 1, y + 2, 1, 6, c);
            cv_rect(x + 1, y + 1, 1, 1, c), cv_rect(x + w - 2, y + 1, 1, 1, c);
            cv_rect(x + 1, y + 8, 1, 1, c), cv_rect(x + w - 2, y + 8, 1, 1, c);
        }
    }
}

/* (1.4.0) the chord's page: its lane alone, large. Each stab a bar as tall as it was struck (the ones still to come
 * this bar at a middle height), its tail fading for as long as it rings, its echo's repeats as rings where they land,
 * as faint as they are (a thrown one's brighter, outlined); the playhead's column */
static void draw_chord_lane(void)
{
    const uint32_t bar = en_lane[0];
    const int pos = en_drum_pos[0];
    const float step_s = proj.par[P_TEMPO] > 0 ? 15.0f / (float)proj.par[P_TEMPO] : 0.12f;
    const float tail = en_stab_t60 * 0.5f / step_s;   /* (to about -30 dB, in sixteenths) */
    float ghost[16];
    uint8_t gthr[16];
    int i, k;
    if (en_playing)
        cv_round(VX - 2 + pos * 11, 98, 10, 46, 2, K_PANEL);
    for (i = 0; i < 16; i++)                     /* the beats */
        cv_rect(VX + i * 11 + 2, 140, 2, (i & 3) ? 2 : 4, (i & 3) ? K_LINE : K_DIM);
    for (i = 0; i < 16; i++)
        ghost[i] = 0.0f, gthr[i] = 0;
    if (!en_ext && proj.par[P_TEMPO] > 0) {      /* the echo's repeats: where each lands, as faint as it is */
        const float e = en_echo_ms * (float)proj.par[P_TEMPO] * (1.0f / 15000.0f);
        for (i = 0; i < 16; i++)
            if (bar >> i & 1u) {
                const int thr = en_stab_thr >> i & 1u;
                float a = en_echo_in * (thr ? 1.6f : 1.0f);
                for (k = 1; k <= 8; k++) {
                    const int q = (int)((float)i + (float)k * e + 0.5f) % 16;
                    a *= en_echo_g;
                    if (a < 0.05f)
                        break;
                    if (a > ghost[q])
                        ghost[q] = a, gthr[q] = (uint8_t)thr;
                }
            }
    }
    for (i = 0; i < 16; i++) {                   /* the repeats first, under the stabs */
        const int32_t x = VX + i * 11;
        if (ghost[i] > 0.0f && !(bar >> i & 1u)) {
            const uint16_t c = mix(K_BG, gthr[i] ? K_TEXT : K_PAD, 6 + q16(ghost[i]) * 9 / 16);
            cv_rect(x + 1, 116, 5, 1, c), cv_rect(x + 1, 122, 5, 1, c);
            cv_rect(x, 117, 1, 5, c), cv_rect(x + 6, 117, 1, 5, c);
        }
    }
    for (i = 0; i < 16; i++) {
        const int32_t x = VX + i * 11;
        int j, len, h;
        float v;
        if (!(bar >> i & 1u))
            continue;
        v = en_stab_v[i] ? (float)en_stab_v[i] * (1.0f / 255.0f) : 0.7f;
        h = 8 + (int)(20.0f * v);
        for (j = i + 1; j < 16 && !(bar >> j & 1u); j++)
            ;
        len = (int)(tail + 0.5f);
        if (len > j - i - 1)
            len = j - i - 1;
        for (k = 1; k <= len; k++)               /* its tail, fading */
            cv_rect(x + k * 11 - 4, 119 - 1, 11, 3, mix(K_BG, K_PAD_T, 16 - k * 12 / (len + 1)));
        if (en_stab_thr >> i & 1u) {             /* thrown: the tallest, wider, a white frame two pixels round it */
            h = 30;
            cv_round(x - 3, 120 - h / 2 - 3, 13, h + 6, 4, K_TEXT);
            cv_round(x - 1, 120 - h / 2 - 1, 9, h + 2, 3, K_BG);
            cv_round(x, 120 - h / 2, 7, h, 2, en_playing && i == pos ? K_TEXT : K_PAD);
        } else {
            cv_round(x, 120 - h / 2, 7, h, 2, en_playing && i == pos ? K_TEXT : en_stab_v[i] ? K_PAD : mix(K_PAD_T, K_PAD, 8));
        }
    }
}

/* (1.4.0) the Pings page: its lane alone, large. The four loops a row each (13, 17, 23 and 29 eighths, each across
 * the lane: they drift against each other), each note a dot at its pitch (higher, higher in the row), the step each
 * loop is on lit, brighter as it sounds */
static void draw_pings_lane(void)
{
    int l, j;
    for (l = 0; l < EN_NLOOP; l++) {
        const int len = EN_LOOP_LEN[l], at = en_loop_pos[l];
        const int32_t y = 100 + l * 11;
        const float g = en_playing ? en_loop_glow[l] : 0.0f;
        cv_round(VX - 13, y + 3, 5, 4, 1, mix(K_BELL_T, K_BELL, 6 + 2 * l));
        for (j = 0; j < len; j++) {
            const int32_t x0 = VX + j * 176 / len, x1 = VX + (j + 1) * 176 / len - 2;
            const int n = en_loop_note[l][j];
            const int32_t xc = (x0 + x1) / 2;     /* (each step's middle: the same marks on every row, not stretched) */
            if (en_playing && j == at)           /* the step it's on: the same width on every row */
                cv_round(xc - 4, y - 1, 9, 11, 2, K_PANEL);
            if (n >= 0) {
                const int32_t ny = y + 4 - (n > 4 ? 4 : n);   /* (the higher the note, the higher in its row) */
                const uint16_t c = j == at && en_playing ? mix(K_BELL, K_TEXT, 4 + q16(g) * 12 / 16) : mix(K_BELL_T, K_BELL, 13);
                cv_disc(xc, ny + 2, 2, c);
            } else {
                cv_rect(xc, y + 4, 1, 1, K_LINE);
            }
        }
    }
}

/* (1.4.0) the Drums page: what its knobs move, alone, large (the whole kit is the Pattern page's): the kick, a row
 * (orange), and the hats, a row (white: the shaker filled, the open hat an outline), all the same height (a ghost
 * kick dimmer); the playhead's column; A or B at the left (bars 4 and 8, the phrase's variant) */
static void draw_drums_lane(void)
{
    const uint32_t k = en_lane[1], sh = en_lane_sh, op = en_lane_op;
    const int pos = en_drum_pos[0], b = en_phrase_bar == 3 || en_phrase_bar == 7;
    int i;
    if (en_playing)
        cv_round(VX - 2 + pos * 11, 101, 10, 40, 2, K_PANEL);
    phrase_ab(b, K_PULSE);
    for (i = 0; i < 16; i++) {
        const int32_t x = VX + i * 11;
        const int now = en_playing && i == pos;
        if (k >> i & 1u)                         /* the kick */
            cv_round(x, 106, 8, 12, 2, now ? K_TEXT : en_kick_v[i] < 160 ? mix(K_PULSE_T, K_PULSE, 7) : K_PULSE);
        else
            cv_rect(x + 3, 111, 2, 2, (i & 3) ? K_LINE : K_DIM);
        if (sh >> i & 1u) {                      /* the hats: the shaker filled */
            cv_round(x, 124, 8, 12, 2, now ? K_TEXT : mix(K_LINE, K_TEXT, 12));
        } else if (op >> i & 1u) {               /* the open hat: an outline, the same size */
            const uint16_t c = now ? K_TEXT : mix(K_LINE, K_TEXT, 12);
            cv_rect(x + 1, 124, 6, 1, c), cv_rect(x + 1, 135, 6, 1, c);
            cv_rect(x, 125, 1, 10, c), cv_rect(x + 7, 125, 1, 10, c);
        } else {
            cv_rect(x + 3, 129, 2, 2, (i & 3) ? K_LINE : K_DIM);
        }
    }
}

/* (1.4.0) the Noise pages: its groove alone, large: two bars of sixteenths, a row each (1 above 2), each hit a block
 * as strong as it is (a ghost lower), its ring a tail fading as long as it rings; 1 and 2 at the left, the bar
 * playing lit; the playhead in its row. (The dry clicks come at random: not shown) */
static void draw_noise_lane(void)
{
    const uint32_t p = ((ui.mutes & EN_M_NOISE) || !proj.par[P_CRACKLE]) ? 0u : en_nz_pat;
    const int st = en_nz_step, bar2 = st >= 16;
    const float step_s = proj.par[P_TEMPO] > 0 ? 15.0f / (float)proj.par[P_TEMPO] : 0.12f;
    const int tail = (int)(en_nz_t60 * 0.5f / step_s + 0.5f);   /* (to about -30 dB, in sixteenths) */
    int r, i, k;
    text_c(VX - 11, 99, &FONT_C, "1", bar2 ? K_LINE : K_NOISE);
    text_c(VX - 11, 119, &FONT_C, "2", bar2 ? K_NOISE : K_LINE);
    if (en_playing)                              /* (the step playing, in its bar's row) */
        cv_round(VX - 2 + (st & 15) * 11, (bar2 ? 126 : 106) - 5, 10, 20, 2, K_PANEL);
    for (r = 0; r < 2; r++) {
        const int32_t y = r ? 126 : 106;
        for (i = 0; i < 16; i++) {
            const int s = r * 16 + i;
            const int32_t x = VX + i * 11;
            if (p >> s & 1u) {
                const int h = 4 + en_nz_vel[s] * 10 / 255, now = en_playing && s == st;
                for (k = 1; k <= tail && i + k < 16 && !(p >> (s + k) & 1u); k++)   /* its ring, fading */
                    cv_rect(x + k * 11 - 4, y + 4, 11, 2, mix(K_BG, mix(K_BG, K_NOISE, 9), 16 - k * 14 / (tail + 1)));
                cv_round(x, y + 5 - h / 2, 7, h, 2, now ? K_TEXT : en_nz_vel[s] < 100 ? mix(K_BG, K_NOISE, 9) : K_NOISE);
            } else {
                cv_rect(x + 3, y + 4, 2, 2, (i & 3) ? K_LINE : K_DIM);
            }
        }
    }
}

/* (1.4.0, the user's: the LFO pages' picture across the screen, VARIETY-PLAN.md) an LFO page: its past, now and
 * future, the full width and (zzq, the user's: the knobs say the Rate, the Wave and the Target) the full height, no
 * title; now (a dot on what's heard) in the middle. Left, what it did: what's heard (the wave and the wander together,
 * en_lfo_tr) bold in the layer's colour, older fainter; the wave's part alone a thin white line, the wander's a thin
 * pink one (zzq: solid, a dotted one lost on the FM-1's screen). Right, where the wave goes (dim; the wander can't be
 * known, not drawn ahead). At one scale, the most Drift and the page's Drift reach, so it shrinks to flat with them,
 * or grows; its reach dotted. Under it the bars, the wave's cycles (or, on the wander alone, the phrases) taller. The
 * span each side: 4 bars for a 1- or 2-bar wave, 8 for 4, else 16 (two cycles where they fit). Its reach's ends in
 * its Target's units at the left edge, inside the reach on a dark patch (outside it while it's small): the chord's
 * cutoff in Hz around its knobs' own (before each stab's envelope), Reso's and Colour's and the noise's Tone and Air
 * as their knobs show them, the noise's Filter in Hz */
static void lfo_hz(float f, char *b)             /* (570 Hz, 2.3 kHz, 20 kHz) */
{
    const int hz = (int)(f + 0.5f);
    char t[12];
    int j = 0, k;
    if (hz >= 10000) {
        itoa_u((uint32_t)((hz + 500) / 1000), t);
    } else if (hz >= 1000) {
        const int h = (hz + 50) / 100;
        itoa_u((uint32_t)(h / 10), t);
        for (k = 0; t[k]; k++)
            ;
        t[k++] = '.', t[k++] = (char)('0' + h % 10), t[k] = 0;
    } else {
        itoa_u((uint32_t)(hz >= 100 ? (hz + 5) / 10 * 10 : hz), t);
    }
    for (k = 0; t[k]; k++)
        b[j++] = t[k];
    b[j++] = ' ';
    if (hz >= 1000)
        b[j++] = 'k';
    b[j++] = 'H', b[j++] = 'z', b[j] = 0;
}
static void lfo_units(int k, float re, char *hi, char *lo)   /* (its reach's ends in its Target's units) */
{
    const int dst = proj.par[k ? P_NLDEST : P_CLDEST];
    if (!k && dst == EN_CLD_CUTOFF) {
        const float b = en_chord_cut(), e = fm_exp2f(re);
        lfo_hz(fm_clampf(b * e, 90.0f, 12000.0f), hi);
        lfo_hz(fm_clampf(b / e, 90.0f, 12000.0f), lo);
    } else if (k && dst == EN_NLD_FILTER) {
        lfo_hz(20000.0f, hi);
        lfo_hz(20000.0f * fm_exp2f(-1.9f * fm_minf(2.0f * re, 2.0f)), lo);
    } else {                                     /* (a knob's: as far along it, within its ends) */
        const int p = k ? (dst == EN_NLD_TONE ? P_NCOL : P_NROOM) : dst == EN_CLD_RESO ? P_CRES : P_CCOL,
                  lo0 = p == P_NROOM ? 0 : -50, hi0 = p == P_NROOM ? 100 : 50,
                  dl = (int)((k ? 25.0f * re : 50.0f * (2.0f / 5.5f) * re) + 0.5f), v = proj.par[p];
        en_param_text(p, v + dl > hi0 ? hi0 : v + dl, hi);
        en_param_text(p, v - dl < lo0 ? lo0 : v - dl, lo);
    }
}
static void draw_lfo(int k)
{
    const int32_t xl = 4, xn = 120, xr = 236, half = xn - xl, yc = 44, rr = 36;   /* (the picture's rows, 8..80, the
                                                  * ticks under them: the lanes below kept clear; 127: rr pixels) */
    const uint16_t col = k ? K_NOISE : K_PAD, cw = mix(K_BG, K_TEXT, 14), cf = mix(K_BG, K_TEXT, 6),
                   cd = mix(K_BG, K_RIM, 14);   /* (what's heard the layer's colour; the wave white, the wander pink) */
    const uint32_t wn = en_lfo_w, n = wn < EN_LFO_N ? wn : EN_LFO_N, cur = en_bars ? en_bars - 1u : 0u;
    const float mx = k ? 2.0f : 2.2f, re = en_lfo_reach_of(k);   /* (the scale: the most either reaches, LFO_CR_MAX,
                                                  * LFO_NR_MAX; its reach now) */
    const int rate = proj.par[k ? P_NLRATE : P_CLRATE], bars = en_lfo_bars(rate),
              dw = re > 0.0f, d = dw && proj.par[k ? P_NLWAVE : P_CLWAVE] > 0, wd = dw && proj.par[k ? P_NLWAVE : P_CLWAVE] < 100,
              reach = (int)(re / mx * (float)rr + 0.5f),
              hb = !d || !bars ? 16 : bars <= 2 ? 4 : bars == 4 ? 8 : 16;
    const uint32_t span = (uint32_t)hb * 16u;   /* (sixteenths each side) */
    int32_t x, y0 = yc, yy;
    cv_rect(xl, yc, xr - xl, 1, mix(K_BG, K_LINE, 10));
    for (x = xl; x < xr; x += 4)                 /* (its reach) */
        if (reach > 0) {
            cv_rect(x, yc - reach, 1, 1, K_LINE);
            cv_rect(x, yc + reach, 1, 1, K_LINE);
        }
    for (yy = yc - rr; yy <= yc + rr; yy += 3)   /* (now) */
        cv_rect(xn, yy, 1, 1, K_LINE);
    {   /* (the bars under it: each a tick, the wave's cycles taller, or the phrases on the wander alone) */
        const int32_t yt = yc + rr + 4;
        const uint32_t cb = cur >> 4;
        int64_t sp = -1;
        for (x = xl; x < xr; x++) {
            const int64_t s = x <= xn ? (int64_t)cur - (int64_t)((uint32_t)(xn - x) * span / (uint32_t)half)
                                      : (int64_t)cur + (int64_t)((uint32_t)(x - xn) * span / (uint32_t)half);
            if (x > xl && s >= 0 && (s >> 4) != (sp >> 4)) {
                const uint32_t b = (uint32_t)(s >> 4);
                const int tall = d && bars ? b % (uint32_t)bars == 0u
                                           : ((b + 8u - cb % 8u + en_phrase_bar) & 7u) == 0u;
                cv_rect(x, tall ? yt - 1 : yt + 1, 1, tall ? 5 : 2, tall ? K_DIM : K_LINE);
            }
            sp = s;
        }
    }
    if (d)                                       /* (the wave ahead, dim, as it will go) */
        for (x = xn + 1; x < xr; x++) {
            const uint32_t s = cur + (uint32_t)(x - xn) * span / (uint32_t)half;
            const float wp = en_lfo_wave_part(k, s) / mx * (float)rr;
            int32_t a0, a1;
            yy = yc - (int32_t)(wp + (wp < 0.0f ? -0.5f : 0.5f));
            if (x == xn + 1) {
                const float w0 = en_lfo_wave_part(k, cur) / mx * (float)rr;
                y0 = yc - (int32_t)(w0 + (w0 < 0.0f ? -0.5f : 0.5f));
            }
            a0 = y0 < yy ? y0 : yy, a1 = y0 > yy ? y0 : yy;
            cv_rect(x, a0, 1, a1 - a0 + 1, cf);
            y0 = yy;
        }
    for (x = xl; x <= xn; x++) {                 /* (what was heard, bold, the oldest at the left, fainter) */
        const uint32_t age = (uint32_t)(xn - x) * span / (uint32_t)half;
        int32_t a0, a1;
        if (age >= n)                            /* (not played yet so far back) */
            continue;
        yy = yc - (int32_t)en_lfo_tr[k][(wn - 1u - age) % EN_LFO_N] * rr / 127;
        if (x == xl || age + 1u >= n)
            y0 = yy;
        a0 = y0 < yy ? y0 : yy, a1 = y0 > yy ? y0 : yy;
        cv_rect(x, a0 - 1, 1, a1 - a0 + 3, mix(K_BG, col, 6 + (int)(10u - age * 10u / span)));
        y0 = yy;
    }
    if (wd)                                      /* (the wander's part, a thin pink line over it) */
        for (x = xl; x <= xn; x++) {
            const uint32_t age = (uint32_t)(xn - x) * span / (uint32_t)half;
            int32_t a0, a1;
            if (age >= n)
                continue;
            yy = yc - (int32_t)en_lfo_wd[k][(wn - 1u - age) % EN_LFO_N] * rr / 127;
            if (x == xl || age + 1u >= n)
                y0 = yy;
            a0 = y0 < yy ? y0 : yy, a1 = y0 > yy ? y0 : yy;
            cv_rect(x, a0, 1, a1 - a0 + 1, cd);
            y0 = yy;
        }
    if (d)                                       /* (the wave's part, a thin white line over them) */
        for (x = xl; x <= xn; x++) {
            const uint32_t age = (uint32_t)(xn - x) * span / (uint32_t)half;
            int32_t a0, a1;
            if (age >= n)
                continue;
            yy = yc - (int32_t)en_lfo_wv[k][(wn - 1u - age) % EN_LFO_N] * rr / 127;
            if (x == xl || age + 1u >= n)
                y0 = yy;
            a0 = y0 < yy ? y0 : yy, a1 = y0 > yy ? y0 : yy;
            cv_rect(x, a0, 1, a1 - a0 + 1, cw);
            y0 = yy;
        }
    if (n)                                       /* (now: a dot on what's heard, in the middle) */
        cv_disc(xn, yc - (int32_t)en_lfo_tr[k][(wn - 1u) % EN_LFO_N] * rr / 127, 4, en_playing ? K_TEXT : col);
    if (dw) {                                    /* (its reach's ends, in its Target's units, at the left edge: inside
                                                  * the reach on a dark patch, over the oldest past; outside it while the
                                                  * reach is too small for two; FONT_XS's glyphs its cell's rows 1..12) */
        char hi[12], lo[12];
        const int in = reach >= 16;
        const int32_t yh = in ? yc - reach + 1 : yc - reach - 13, yl = in ? yc + reach - 13 : yc + reach + 1;
        lfo_units(k, re, hi, lo);
        cv_shade(xl - 2, yh + 1, text_w(&FONT_XS, hi) + 4, 12, 2, 12u, K_BG);
        cv_shade(xl - 2, yl + 1, text_w(&FONT_XS, lo) + 4, 12, 2, 12u, K_BG);
        cv_text(xl, yh, &FONT_XS, hi, K_DIM);
        cv_text(xl, yl, &FONT_XS, lo, K_DIM);
    }
}

/* (1.4.0, the user's: song structure) the Song pages' picture, in the lanes' place: the song as set, its intro,
 * main and outro in proportion (a band over the rows, the intro and the outro in the accent), each part's row from
 * where it comes in to where it leaves, in its colour (the Kind's order; brighter once in, fading out), and while a
 * song plays, where it is; in Endless the same, dimmed (the song it would be) */
static void draw_song_lane(void)
{
    static const int8_t PART[LN_ROWS] = {0, 1, 2, 3, 4, 4, 5};   /* (R/C: the hats' part) */
    const int kind = proj.par[P_SKIND] != 0, on = proj.par[P_SMODE] != 0,
              live = en_song_ph != EN_SG_OFF && en_song_be > 0u;
    const float b0 = live ? (float)en_song_b0 : 8.0f * (float)proj.par[P_SINTRO],
                bo = live ? (float)en_song_bo : b0 + 8.0f * (float)proj.par[P_SMAIN],
                be = live ? (float)en_song_be : bo + 8.0f * (float)proj.par[P_SOUTRO], w = (float)(232 - LN_X0),
                k = w / fm_maxf(be, 1.0f);
    int r;
#define SGX(b) (LN_X0 + (int32_t)((b) * k + 0.5f))
    cv_rect(SGX(0.0f), LN_Y0 - 7, SGX(b0) - SGX(0.0f), 3, mix(K_BG, K_ACC, on ? 10 : 5));
    cv_rect(SGX(b0), LN_Y0 - 7, SGX(bo) - SGX(b0), 3, mix(K_BG, K_LINE, on ? 14 : 8));
    cv_rect(SGX(bo), LN_Y0 - 7, SGX(be) - SGX(bo), 3, mix(K_BG, K_ACC, on ? 10 : 5));
    for (r = 0; r < LN_ROWS; r++) {
        const int pt = PART[r];
        const int32_t y = LN_Y0 + r * LN_ROW;
        const float in = en_song_in(kind, pt) * b0, out = bo + en_song_out(pt) * (be - bo);
        const uint16_t c = ln_col(r);
        const int32_t x1 = SGX(in), x2 = SGX(fm_minf(in + 3.0f, bo)), x3 = SGX(out), x4 = SGX(fm_minf(out + 0.2f * (be - bo), be));
        cv_rect(x1, y + 1, x2 - x1, LN_H - 2, mix(K_BG, c, on ? 7 : 4));   /* (fading in) */
        cv_rect(x2, y, x3 - x2, LN_H, mix(K_BG, c, on ? 13 : 6));
        cv_rect(x3, y + 1, x4 - x3, LN_H - 2, mix(K_BG, c, on ? 7 : 4));   /* (leaving) */
    }
    if (live) {                                  /* (where the song is: a white line down through it, on top; the user's,
                                                  * over a grey column: easier to see) */
        const float at = fm_minf((float)en_bars * (1.0f / 16.0f), be);
        cv_rect(SGX(at), LN_Y0 - 9, 1, LN_ROWS * LN_ROW + 9, en_song_ph == EN_SG_END ? K_DIM : K_TEXT);
    }
#undef SGX
}
static void draw_lanes(void)
{
    draw_lmeters();                              /* (1.4.0, the user's: the meters at the left, on every page with lanes) */
    if (ui.view == V_SONG || ui.view == V_SMODE) {   /* (1.4.0) the Song pages: the song's shape */
        draw_song_lane();
        return;
    }
    if (noise_page(ui.view)) {   /* (1.4.0) the Noise pages: its groove alone */
        draw_noise_lane();
        return;
    }
    if (oct_layer(ui.view) == 3 && ui.view != V_PAT) {   /* (1.4.0) the drums' pages: their lanes alone */
        draw_drums_lane();
        return;
    }
    if (oct_layer(ui.view) == 1) {               /* (1.4.0) the pings' pages: its lane alone */
        draw_pings_lane();
        return;
    }
    if (oct_layer(ui.view) == 0) {               /* (1.4.0) the chord's pages: its lane alone */
        draw_chord_lane();
        return;
    }
    if (oct_layer(ui.view) == 2) {               /* (1.4.0) the bass's pages: its lane alone */
        draw_bass_lane();
        return;
    }
    {
        static const uint16_t C[8] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_TEXT, K_RIM, K_NOISE, K_CLAP},
                              T[8] = {K_PAD_T, K_BELL_T, K_BASS_T, K_PULSE_T, K_LINE, K_RIM_T, RGB(86, 82, 76), K_CLAP_T};
        static const int8_t DR[8] = {-1, -1, -1, 0, 1, 3, -1, 4};   /* each row's drum (its chance); the clap the 8th */
        const int pos = en_drum_pos[0], nz2 = en_nz_step >= 16;
        const uint32_t nz = (ui.mutes & EN_M_NOISE) || !proj.par[P_CRACKLE] ? 0u : (en_nz_pat >> (nz2 ? 16 : 0)) & 0xFFFFu;
        float ghost[16];
        int l, i, k;
        if (en_playing)                          /* the step playing: a column */
            cv_round(LN_XI(pos) - 2, LN_Y0 - 2, 10, LN_ROWS * LN_ROW + 2, 2, K_PANEL);
        for (i = 0; i < 16; i++)
            ghost[i] = 0.0f;
        if (!en_ext && proj.par[P_TEMPO] > 0) {  /* the stabs' echoes: where each repeat lands, as faint as it is */
            float e = en_echo_ms * (float)proj.par[P_TEMPO] * (1.0f / 15000.0f), g = en_echo_in;   /* (in sixteenths) */
            for (i = 0; i < 16; i++)
                if (en_lane[0] >> i & 1u) {
                    float a = g;
                    for (k = 1; k <= 6; k++) {
                        int q = (int)((float)i + (float)k * e + 0.5f) % 16;
                        a *= en_echo_g;
                        if (a < 0.06f)
                            break;
                        if (a > ghost[q])
                            ghost[q] = a;
                    }
                }
        }
        for (l = 0; l < LN_ROWS; l++) {
            const int32_t y = LN_Y0 + l * LN_ROW;
            const uint32_t p = l == 0 ? en_lane[0] : l == 1 ? en_lane_png : l == 2 ? en_lane[5] : l == 3 ? en_lane[1]
                             : l == 4 ? en_lane[2] : l == 5 ? en_lane[3] : nz, p2 = l == 5 ? en_lane[4] : 0u;
            for (i = 0; i < 16; i++) {
                const int32_t x = LN_XI(i);
                const int now = en_playing && i == pos, two = (p >> i & 1u) && (p2 >> i & 1u);
                int h;
                if (!((p | p2) >> i & 1u)) {
                    if (l == 0 && ghost[i] > 0.0f) {   /* an echo's repeat: a ring; as the playhead hits it, a flash */
                        if (now) {
                            cv_round(x - 1, y - 1, 8, LN_H + 2, 2, K_TEXT);
                        } else {
                            const uint16_t c = mix(K_BG, K_PAD, 9 + q16(ghost[i]) * 7 / 16);
                            cv_rect(x + 1, y, 4, 1, c), cv_rect(x + 1, y + LN_H - 1, 4, 1, c);
                            cv_rect(x, y + 1, 1, LN_H - 2, c), cv_rect(x + 5, y + 1, 1, LN_H - 2, c);
                        }
                    } else {
                        cv_rect(x + 2, y + 2, 2, 1, (i & 3) ? mix(K_BG, K_LINE, 12) : K_DIM);
                    }
                    continue;
                }
                for (h = 0; h < 2; h++) {        /* (the R/C row: the rim, then the clap; both on a step, side by side) */
                    const int c8 = h ? 7 : l, d = DR[c8];
                    const int32_t xx = two && h ? x + 3 : x, w = two ? 3 : 6;
                    const int maybe = d >= 0 && (proj.par[P_DMODE] ? proj.par[P_DCH0 + d] < 50 : d == 4);
                    if (!((h ? p2 : p) >> i & 1u))
                        continue;
                    if (maybe && !(now && en_drum_glow[d] > 0.5f)) {   /* a hit that only sometimes comes: a ring;
                                                  * filled when it does */
                        const uint16_t c = mix(T[c8], C[c8], 11);
                        cv_rect(xx + 1, y, w - 2, 1, c), cv_rect(xx + 1, y + LN_H - 1, w - 2, 1, c);
                        cv_rect(xx, y + 1, 1, LN_H - 2, c), cv_rect(xx + w - 1, y + 1, 1, LN_H - 2, c);
                    } else if (maybe) {
                        cv_round(xx - (two ? 0 : 1), y - 1, w + (two ? 0 : 2), LN_H + 2, 2, K_TEXT);
                    } else {
                        const uint16_t c = now ? (l == 4 ? K_TEXT : mix(C[c8], K_TEXT, 6))
                                         : l == 6 && en_nz_vel[(nz2 ? 16 : 0) + i] < 100 ? mix(T[c8], C[c8], 6)
                                         : mix(T[c8], C[c8], 11);
                        cv_round(xx, y, w, LN_H, two ? 1 : 2, c);
                    }
                }
            }
        }
    }
}


/* the effects grid, in the main screen's place while REC is held: four rows of four, the first of each lit
 * as the group's (its key lit too), the ones held filled */
static void grid_band(int32_t oy)                 /* the whole screen, in two bands of 240 x 120 */
{
    static const char *const GROUP[4] = {"ECHO", "FILTER", "SPACE", "RHYTHM"};
    static const char *const CELL[EN_NFX] = {"Throw", "Time x2", "Stop", "Swell", "Low", "High", "Wah", "Phone",
                                             "Verb", "Crash", "Freeze", "Wash", "Stutter", "Gate", "Siren", "Riser"};
    int r, c;
    cv_begin(240, 120, K_BG);
    text_c(120, 2 - oy, &FONT_S, "EFFECTS", K_ACC);
    text_c(120, 20 - oy, &FONT_XS, "HOLD A WHITE KEY    TAP REC: DUB HOLD", K_DIM);
    for (r = 0; r < 4; r++) {
        int32_t yr = 38 + r * 50;
        cv_text(6, yr - oy, &FONT_XS, GROUP[r], K_DIM);
        for (c = 0; c < 4; c++) {
            int e = r * 4 + c, on = (int)(ui.fx >> e & 1u);
            int32_t x = 4 + c * 59, y = yr + 13 - oy;
            char t[12];
            cv_round(x, y, 55, 33, 5, on ? K_ACC : c == 0 ? K_PANEL : mix(K_BG, K_PANEL, 8));
            if (c == 0 && !on)                   /* (the first of a group: its key is lit) */
                cv_rect(x + 4, y + 29, 47, 2, K_ACC);
            caps(t, CELL[e], sizeof t);
            text_c(x + 28, y + 8, &FONT_S, t, on ? K_BG : K_TEXT);
        }
    }
    cv_blit(0, (uint32_t)oy);
}

static void draw_grid(void)                      /* REC held: the effects, the whole screen */
{
    uint32_t h = hash(0x47524944u, ui.fx);
    if (h == ui.sig[1] && ui.sig[0] == 0x47524944u)
        return;
    ui.sig[0] = ui.sig[2] = 0x47524944u;         /* (the header and the knobs drawn again after) */
    ui.sig[1] = h;
    grid_band(0);
    grid_band(120);
}

/* the Pattern page: the four lanes, sixteen steps each, the lane edited lit, the accents taller, the step
 * playing a column */
static void draw_pattern(void)
{
    static const uint16_t C[EN_NDRUM] = {K_PULSE, K_TEXT, K_DIM, K_RIM, K_CLAP};
    static const char *const SH[EN_NDRUM] = {"KICK", "SHKR", "OPEN", "RIM", "CLAP"};
    int l, i, pos = en_drum_pos[0];
    text_c(120, MAIN_SHIFT - 1, &FONT_XS, proj.par[P_DMODE] ? "PLAY: THE PATTERN" : "PLAY: AUTO (A KEY TAKES IT OVER)",
           proj.par[P_DMODE] ? K_ACC : K_DIM);
    if (en_playing)
        cv_round(37 + pos * 12, 15, 11, 86, 2, K_PANEL);   /* (five lanes, 17 px apart) */
    for (l = 0; l < EN_NDRUM; l++) {
        uint32_t p = lane_steps(l), a = proj.par[P_DMODE] ? (uint32_t)(uint16_t)proj.par[P_DACC0 + l] : 0u;
        int32_t y = 17 + l * 17, sel = l == ui.lane;
        int maybe = proj.par[P_DMODE] ? proj.par[P_DCH0 + l] < 50 : l == 4;   /* (a chance under half: rings) */
        if (sel)
            cv_round(2, y - 2, 236, 17, 4, mix(K_BG, K_PANEL, 10));
        cv_text(5, y + 1, &FONT_XS, SH[l], sel ? K_TEXT : K_DIM);
        for (i = 0; i < 16; i++) {
            int32_t x = 38 + i * 12;
            if (p >> i & 1u) {
                int ac = (int)(a >> i & 1u);
                uint16_t c = en_playing && i == pos ? K_TEXT : sel ? C[l] : mix(K_BG, C[l], 9);
                int32_t yy = ac ? y : y + 3, hh = ac ? 13 : 7;
                if (maybe) {                     /* a step that only sometimes plays: a ring */
                    cv_rect(x + 1, yy, 7, 1, c), cv_rect(x + 1, yy + hh - 1, 7, 1, c);
                    cv_rect(x, yy + 1, 1, hh - 2, c), cv_rect(x + 8, yy + 1, 1, hh - 2, c);
                } else {
                    cv_round(x, yy, 9, hh, 2, c);
                }
            } else {
                cv_rect(x + 4, y + 6, 2, 2, (i & 3) ? K_LINE : K_DIM);
            }
        }
    }
    text_c(120, 104, &FONT_XS, "WHITE: STEPS   BLACK + WHITE: ACCENT", K_DIM);
}

/* the Chords page: the eight slots, the one edited outlined, the one playing lit; an empty slot inside the
 * progression shows that the chord before it goes on */
static void draw_slots(void)
{
    int i, loop = (proj.par[P_PMODE] & EN_PLOOP) != 0, n = loop ? prog_count() : EN_NPROG;
    char nm[16];
    /* (1.4.0, the user's: no mode line on top; the walk's character on ALGORITHM's ring, the Walk page and the reader) */
    for (i = 0; i < EN_NPROG; i++) {
        int32_t x = 12 + (i & 3) * 55, y = 30 + (i >> 2) * 46;   /* (the two rows in the middle) */
        int filled = !slot_is_gap(i), now = en_playing && i == en_prog_pos && n, sel = i == ui.slot;
        if (sel)
            cv_round(x - 2, y - 2, 54, 38, 5, K_ACC);
        cv_round(x, y, 50, 34, 4, now ? K_PAD : filled ? K_PANEL : mix(K_BG, K_PANEL, 6));
        {
            char d[2];
            d[0] = (char)('1' + i), d[1] = 0;
            cv_text(x + 3, y + 1, &FONT_XS, d, now ? K_BG : K_DIM);
        }
        if (filled) {                            /* (yours in white; the walk's in the scale's teal) */
            const char *t = slot_label(i, nm);
            text_c(x + 25, y + 11, text_w(&FONT_B, t) <= 46 ? &FONT_B : &FONT_XS, t,
                   now ? K_BG : slot_is_walk(i) ? K_PAD : K_TEXT);
        } else if (i < n) {
            text_c(x + 25, y + 13, &FONT_XS, "(HOLDS)", now ? K_BG : K_DIM);
        }
    }
    {   /* (1.4.0, the user's) under the slots: a keyboard, C3 to G5, the notes of the chord the cursor is on lit as the
         * stabs voice it (yours in white, the walk's in teal, as their names), its root marked; a gap: the chord it
         * holds, dimmer; none before it: the keys alone */
        static const int8_t WK[7] = {0, 2, 4, 5, 7, 9, 11};
        int nt[4], held, nn = slot_notes(ui.slot, nt, &held), k, w;
        uint16_t c = K_TEXT;
        const int32_t X0 = 15, Y0 = 116, WW = 11, WH = 24, BW = 7, BH = 14;
        {
            int j = ui.slot;
            while (j >= 0 && slot_is_gap(j))
                j--;
            c = j >= 0 && slot_is_walk(j) ? K_PAD : K_TEXT;
        }
        if (held)
            c = mix(K_BG, c, 9);
        for (w = 0; w < 19; w++) {               /* the white keys */
            const int note = 48 + 12 * (w / 7) + WK[w % 7];
            int lit = 0, root = 0;
            for (k = 0; k < nn; k++)
                if (nt[k] == note)
                    lit = 1, root |= k == 0;
            cv_rect(X0 + w * WW, Y0, WW - 1, WH, lit ? c : mix(K_BG, K_DIM, 6));
            if (root)
                cv_disc(X0 + w * WW + (WW - 1) / 2, Y0 + WH - 5, 2, K_BG);
        }
        for (w = 0; w < 18; w++) {               /* the black keys, over the gaps (none after E and B) */
            const int pc = WK[w % 7] + 1, note = 48 + 12 * (w / 7) + pc;
            int lit = 0, root = 0;
            if (w % 7 == 2 || w % 7 == 6)
                continue;
            for (k = 0; k < nn; k++)
                if (nt[k] == note)
                    lit = 1, root |= k == 0;
            cv_rect(X0 + (w + 1) * WW - 2 - BW / 2, Y0, BW + 2, BH + 1, K_BG);   /* (a dark rim round it) */
            cv_rect(X0 + (w + 1) * WW - 1 - BW / 2, Y0, BW, BH, lit ? c : mix(K_BG, K_PANEL, 10));
            if (root)
                cv_disc(X0 + (w + 1) * WW - 1, Y0 + BH - 4, 1, K_BG);
        }
    }
}

/* the level ring (OCT- / OCT+ on a layer's page): over the middle, the layer's level as a big knob in its colour, the
 * number inside, its name under; the arc glides to the level, a step a brief brightening; muted, grey; it fades out
 * OCT_SHOW after the last press */
#define BIG_R 40                                 /* (1.4.0) the big rings' radius (the level's, a knob's) */
#define BIG_CY 64                                /* (1.4.0, the user's: their name over them, large; the ring a little
                                                  * lower for it, the help under it) */
/* (format 31) a zone's name round the big ring: outside it, at the middle of its arc (pushed off the ring's top to a
 * side, the knob's name over it there), left of the ring to its left, right to its right; its top-left and width */
static void zone_label_at(int k, const zone_t *z, int n, int i, int32_t cy, int32_t r, const char *t, int32_t *x0,
                          int32_t *y0, int32_t *w)
{
    const float f0 = zone_frac(k, z[i].at), f1 = i + 1 < n ? zone_frac(k, z[i + 1].at) : 1.0f;
    float a = (0.375f + 0.75f * 0.5f * (f0 + f1)) * FM_TWO_PI, c = fm_cosf(a), sn = fm_sinf(a);
    float rr = (float)r + 7.0f;
    if (fm_fabsf(c) > 0.85f)                     /* (beside the ring: clear of its handle) */
        rr += 3.0f;
    if (sn < 0.0f && fm_fabsf(c) < 0.7f) {       /* (near the top: to the side it leans to, 45 degrees up) */
        c = c < 0.0f ? -0.7071f : 0.7071f;
        sn = -0.7071f;
    }
    *w = text_w(&FONT_XS, t);
    *y0 = cy + (int32_t)(sn * rr) - 7;
    *x0 = c < -0.3f ? CX + (int32_t)(c * rr) - *w : c > 0.3f ? CX + (int32_t)(c * rr) : CX + (int32_t)(c * rr) - *w / 2;
    if (*x0 < 4)
        *x0 = 4;
    if (*x0 + *w > 236)
        *x0 = 236 - *w;
}
/* over a big ring: its name, large */
static void big_title(const char *name, uint16_t col)
{
    char t[24];
    caps(t, name, sizeof t);
    text_c(CX, 0, &FONT_C, t, col);   /* (1.4.0, the user's: 2 px of air under the panel's top) */
}
/* (1.4.0, the user's: the big ring more conspicuous) a panel behind a big ring, its name and its help: a rounded
 * rectangle, what's behind it darkened to a quarter (f: the ring's fade, 16 shown), a thin edge in its colour; as wide
 * as the widest of the three (wmax: the help's), as tall as they go (y1: its bottom) */
static void big_panel(const char *name, int32_t wmax, int32_t y1, uint16_t col, int f)
{
    char t[24];
    int32_t w, x0;
    caps(t, name, sizeof t);
    w = text_w(&FONT_C, t) + 20;
    if (w < 2 * BIG_R + 24)
        w = 2 * BIG_R + 24;
    if (w < wmax + 16)
        w = wmax + 16;
    if (w > 236)
        w = 236;
    x0 = CX - w / 2;
    cv_shade(x0, 0, w, y1, 6, (uint32_t)(f * 12 / 16), K_BG);
    if (f > 0)
        cv_round_edge(x0, 0, w, y1, 6, mix(K_BG, col, f * 7 / 16));
}
#define HELP_W 232                               /* (the help under the big ring: two lines at most, this wide) */
/* a help line in one line or two, split at the space that keeps the wider of the two narrowest: the lines' count
 * (0: it doesn't fit) */
static int help_lines(const char *h, char *a, char *b, int n)
{
    int i, best = -1, bw = 1 << 30, len = 0;
    while (h[len])
        len++;
    if (len >= n)
        return 0;
    for (i = 0; i <= len; i++)
        a[i] = h[i];
    b[0] = 0;
    if (text_w(&FONT_S, a) <= HELP_W)
        return 1;
    for (i = 1; i < len; i++)
        if (h[i] == ' ') {
            int w1, w2, j;
            for (j = 0; j < i; j++)
                a[j] = h[j];
            a[i] = 0;
            w1 = text_w(&FONT_S, a);
            w2 = text_w(&FONT_S, h + i + 1);
            if ((w1 > w2 ? w1 : w2) < bw)
                bw = w1 > w2 ? w1 : w2, best = i;
        }
    if (best < 0 || bw > HELP_W)
        return 0;
    for (i = 0; i < best; i++)
        a[i] = h[i];
    a[best] = 0;
    for (i = best + 1; i <= len; i++)
        b[i - best - 1] = h[i];
    return 2;
}
static void draw_oct_at(uint32_t now)
{
    static const uint16_t C[NOCT] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_NOISE, K_PULSE, K_TEXT};
    const int32_t r = BIG_R, cy = BIG_CY;   /* (1.4.0: as large as the big ring, and as high) */
    int l = oct_ring_on(now) - 1, f;
    int32_t f2;
    uint16_t c, tr, tx;
    float v;
    char t[8];
    if (l < 0)
        return;
    v = (float)proj.par[LAYER_LEVEL[l]] * 0.01f;
    ui.oct_v += (v - ui.oct_v) * 0.55f;          /* (20 frames a second: there in ~100 ms) */
    if (fm_fabsf(v - ui.oct_v) < 0.004f)
        ui.oct_v = v;
    f = now < ui.oct_until ? 16 : 16 - (int)((now - ui.oct_until) * 16u / OCT_FADE);   /* the fade, 16 .. 0 */
    c = (ui.mutes & LAYER_MUTE[l]) == LAYER_MUTE[l] ? K_DIM : C[l];
    if (now - ui.oct_pulse < 160u)               /* a step: brighter for a moment */
        c = mix(c, K_TEXT, 7 - (int)((now - ui.oct_pulse) * 7u / 160u));
    tr = mix(K_BG, K_LINE, f);
    tx = mix(K_BG, K_TEXT, f);
    big_panel(LAYER_NAME[l], 0, cy + r + 9, C[l], f);   /* (1.4.0, the user's: on its panel, as the big ring) */
    c = mix(K_BG, c, f);
    for (f2 = -(r + 7); f2 <= r + 7; f2++) {     /* (a disc behind it, the middle hidden) */
        int32_t w = (int32_t)fm_sqrtf((float)((r + 7) * (r + 7) - f2 * f2));
        cv_rect(CX - w, cy + f2, 2 * w + 1, 1, K_BG);
    }
    arc_th(CX, cy, r, 6.0f, ui.oct_v, tr, c, K_BG, 0);
    if ((ui.mutes & LAYER_MUTE[l]) == LAYER_MUTE[l]) {
        text_c(CX, cy - 11, &FONT_C, "MUTED", tx);
    } else {
        itoa_u((uint32_t)proj.par[LAYER_LEVEL[l]], t);
        text_c(CX, cy - 16, &FONT_L, t, tx);   /* (the level, in the ring's middle) */
    }
    big_title(LAYER_NAME[l], c);                 /* (its name over the ring, large) */
}

/* (1.4.0) the big ring: a knob turning (or SELECT's filter) over the middle, its value large inside (the filter: the
 * amount, Shut or Open under it), its name under the ring; a 0..100 knob an arc from the start, a centred one from
 * the top, a switch its place; Motion's regions marked; it fades out OCT_SHOW after the last click */
static void draw_big_at(uint32_t now)
{
    const int32_t r = BIG_R, cy = BIG_CY;        /* (1.4.0: its name over it now; a little lower for it) */
    int k = ui.big_k, v, f, lo, hi, zn = 0, za = 0, i;
    int32_t f2;
    uint16_t c, tr, tx;
    char t[16];
    const char *under, *help = ui.big_help;
    const zone_t *zs = 0;
    const felucca_font_t *fn;
    if (!big_on(now))
        return;
    f = now < ui.big_until ? 16 : 16 - (int)((now - ui.big_until) * 16u / OCT_FADE);
    if (ui.big_txt[0]) {                         /* (no knob's: the screen's view) */
        lo = 0, hi = 1, v = 1;
    } else {
        knob_range(k, &lo, &hi);
        v = knob_get(k);
    }
    {
        const float tf = ui.big_txt[0] ? ui.big_tf : big_frac(k);
        ui.big_v += (tf - ui.big_v) * 0.55f;     /* (it glides, ~100 ms) */
        if (fm_fabsf(tf - ui.big_v) < 0.003f)
            ui.big_v = tf;
    }
    c = mix(K_BG, (lo < 0 && !v) ? K_DIM : ui.big_col, f);
    tr = mix(K_BG, K_LINE, f);
    tx = mix(K_BG, K_TEXT, f);
    if (!ui.big_txt[0] && k < P_NPARAMS && (zn = knob_zones(k, &zs)) > 0) {   /* (format 31: a knob's zones: the one
                                                  * it's in, its sentence the help) */
        za = zone_at(k, v);
        help = zone_help(&zs[za], v);
    }
    {   /* (1.4.0, the user's) the panel behind it all: the ring, its name, its help */
        int32_t hw = 0, y1 = cy + r + 6;
        if (help) {
            char a[48], b[48];
            const int nl = help_lines(help, a, b, (int)sizeof a);
            if (nl) {
                hw = text_w(&FONT_S, a);
                if (nl == 2 && text_w(&FONT_S, b) > hw)
                    hw = text_w(&FONT_S, b);
                y1 = cy + r + 3 + nl * 14 + 4;
            }
        }
        for (i = 0; i < zn; i++) {               /* (the zones' names round the ring: the panel wide enough) */
            int32_t x0, y0, w;
            char zt[16];
            caps(zt, zs[i].name, sizeof zt);
            zone_label_at(k, zs, zn, i, cy, r, zt, &x0, &y0, &w);
            if (2 * (CX - x0) - 12 > hw)
                hw = 2 * (CX - x0) - 12;
            if (2 * (x0 + w - CX) - 12 > hw)
                hw = 2 * (x0 + w - CX) - 12;
        }
        big_panel(k == K_FILTER ? "Filter" : ui.big_name, hw, y1, ui.big_col, f);
    }
    for (f2 = -(r + 4); f2 <= r + 4; f2++) {     /* (a disc behind it, the middle hidden) */
        int32_t w = (int32_t)fm_sqrtf((float)((r + 4) * (r + 4) - f2 * f2));
        cv_rect(CX - w, cy + f2, 2 * w + 1, 1, K_BG);
    }
    arc_th(CX, cy, r, 6.0f, ui.big_v, tr, c, K_BG, lo < 0);
    if (zn) {                                    /* (format 31: the zones, the ring in segments, the names round it: the
                                                  * one it's in in the ring's colour) */
        arc_zones_th(CX, cy, r, 6.0f, k, v, mix(K_BG, mix(K_LINE, ui.big_col, 4), f), K_BG);
        for (i = 0; i < zn; i++) {
            int32_t x0, y0, w;
            char zt[16];
            caps(zt, zs[i].name, sizeof zt);
            zone_label_at(k, zs, zn, i, cy, r, zt, &x0, &y0, &w);
            cv_text(x0, y0, &FONT_XS, zt, i == za ? c : mix(K_BG, K_DIM, f));
        }
    }
    under = ui.big_name;
    if (k == K_FILTER) {                         /* (the filter: the amount large, Shut or Open under it) */
        if (!v) {
            t[0] = 'O', t[1] = 'f', t[2] = 'f', t[3] = 0;
        } else {
            itoa_u((uint32_t)(v < 0 ? -2 * v : 2 * v), t);
        }
        under = v < 0 ? "SHUT" : v > 0 ? "OPEN" : "FILTER";
    } else if (ui.big_txt[0]) {
        int q;
        for (q = 0; ui.big_txt[q] && q < 15; q++)
            t[q] = ui.big_txt[q];
        t[q] = 0;
    } else {
        knob_text(k, t);
    }
    fn = text_w(&FONT_L, t) <= 2 * r - 12 ? &FONT_L : text_w(&FONT_C, t) <= 2 * r - 10 ? &FONT_C : &FONT_B;
    text_c(CX, cy - (fn == &FONT_L ? 16 : fn == &FONT_C ? 11 : 8) - ((k == K_FILTER && v) || zn ? 5 : 0), fn, t, tx);
    if (k == K_FILTER && v)                      /* (the filter: Shut or Open under its amount) */
        text_c(CX, cy + 12, &FONT_XS, under, c);
    if (zn) {                                    /* (format 31: the zone's name under the value, as the filter's) */
        char zt[16];
        caps(zt, zs[za].name, sizeof zt);
        text_c(CX, cy + 12, &FONT_XS, zt, c);
    }
    big_title(k == K_FILTER ? "Filter" : ui.big_name, c);   /* (1.4.0, the user's: the name over the ring, large;
                                                  * the header the piece's alone) */
    if (help) {                                  /* what it does, under the name: one line or two, centred */
        char a[48], b[48];
        int nl = help_lines(help, a, b, (int)sizeof a);
        if (nl) {
            const int32_t y0 = cy + r + 3;
            uint16_t hc = mix(K_BG, mix(K_DIM, K_TEXT, 7), f);   /* (a light grey, under the ring) */
            int32_t w = text_w(&FONT_S, a), w2 = nl == 2 ? text_w(&FONT_S, b) : 0;
            if (w2 > w)
                w = w2;
            text_c(CX, y0, &FONT_S, a, hc);
            if (nl == 2)
                text_c(CX, y0 + 14, &FONT_S, b, hc);
        }
    }
}

/* a nudge's card: a rounded panel in the middle, outlined amber (a nudge) or red (a shake), the layer and its move
 * and what changed; it fades out CARD_SHOW after */
/* (1.4.0) the big rings over the main screen stay where they were on the screen, the 4 px it lost taken from below:
 * drawn without main_begin's shift (their names at the canvas's top) */
static void draw_oct(uint32_t now)
{
    const int32_t oy = cv_oy;
    cv_oy = 0;
    draw_oct_at(now);
    cv_oy = oy;
}
static void draw_big(uint32_t now)
{
    const int32_t oy = cv_oy;
    cv_oy = 0;
    draw_big_at(now);
    cv_oy = oy;
}
static void draw_card(uint32_t now)
{
    const int32_t w = 196, h = 50, x = CX - w / 2, y = 34;
    int f;
    uint16_t edge, hc, tc;
    if (!card_on(now))
        return;
    f = now < ui.card_until ? 16 : 16 - (int)((now - ui.card_until) * 16u / CARD_FADE);
    if (now - ui.card_t0 < 120u)                 /* (it comes in brighter, for a moment) */
        f = 16;
    edge = mix(K_BG, ui.card_col, f);
    hc = mix(K_BG, ui.card_col, f);
    tc = mix(K_BG, K_TEXT, f);
    cv_round(x - 2, y - 2, w + 4, h + 4, 6, edge);
    cv_round(x, y, w, h, 5, mix(K_BG, K_PANEL, f / 2));
    text_c(CX, y + 4, &FONT_B, ui.card_head, hc);
    text_c(CX, y + 28, &FONT_XS, ui.card_text, tc);
}

static void draw_where(uint32_t now)           /* (1.4.0, the user's: the main area's corners free for the pictures:
                                                  * the section, its bars and the song in the header; the page's name and
                                                  * its place on the knobs' rule; hold, lock and SOLO in the header) */
{
    (void)now;
}
static uint32_t where_sig(uint32_t h)            /* (what they show, for a redraw) */
{
    h = hash(h, (uint32_t)en_song_ph | (en_bars >> 4) << 4 | en_song_be << 20);   /* (1.4.0: the song line) */
    return hash(h, (uint32_t)(en_drift * 40.0f) | en_section << 8 | (uint32_t)en_phrase_bar << 12 | (uint32_t)en_playing << 16 |
                       ui.solo << 20 | (uint32_t)ui.lock << 28 | (uint32_t)en_frozen << 29 | (uint32_t)ui.view << 30);
}

/* HOME's screen views, on ALGORITHM: the ripples, the scope, the spectrum and (1.4.0) the tape. (1.4.0's echo trail
 * and fifths gone, the user's: the fifths now the Walk page's picture; their numbers, 3 and 4, kept out of use, so a
 * save on one comes back on the ripples: ui_screen_ok) */
static int scr_small_chord(int s) { return s != SCR_RIPPLES; }   /* (the chord small, at the top) */

/* (1.4.0, the user's) the Walk page's picture: the root's journey round the circle of fifths (C at the top, a fifth
 * a step clockwise): the notes named round it, the key's bright, the rest faint; the chord's root a single dot, white
 * with the kick; when the chord changes, an arrow from the root before to the new one, fading over four bars (a
 * neighbour on the circle: close; across it: far); round the outside, the way to the next chord (Change). No chord
 * shape, no slots, no trail (the user's: too crowded, "every chord change the whole figure changes"). At the left,
 * under the section's bars: the chord's name, the walk's character (or Loop and where it is), and the slots in order,
 * the one playing lit (a gap: the chord before holds) */
#define WK_X 166
#define WK_Y 82
#define WK_R 42
static void fq_xy(int pc, int32_t r, int32_t *x, int32_t *y)
{
    const float a = (float)((pc * 7) % 12) * (6.2831853f / 12.0f) - 1.5707963f;
    *x = WK_X + (int32_t)((float)r * fm_cosf(a) + (fm_cosf(a) < 0.0f ? -0.5f : 0.5f));
    *y = WK_Y + (int32_t)((float)r * fm_sinf(a) + (fm_sinf(a) < 0.0f ? -0.5f : 0.5f));
}
static int8_t wk_from = -1, wk_to = -1;        /* the arrow: the root before, the root now (-1: none yet) */
static uint32_t wk_t0;                           /* (when it changed) */
static uint32_t wk_fade_ms(void) { return proj.par[P_TEMPO] > 0 ? 960000u / (uint32_t)proj.par[P_TEMPO] : 8000u; }   /* (4 bars) */
static int walk_fading(uint32_t now) { return wk_from >= 0 && now - wk_t0 < wk_fade_ms(); }
static void wk_root_seen(uint32_t now)           /* (every frame: a new root starts the arrow) */
{
    const int r = en_chord_root % 12;
    if (r != wk_to) {
        wk_from = wk_to, wk_to = (int8_t)r, wk_t0 = now;
    }
}
static void draw_walk(void)
{
    int steps[12], ns = en_scale(proj.pool, steps), i, j, inkey[12] = {0};
    int sr[EN_NPROG], loop = (proj.par[P_PMODE] & EN_PLOOP) != 0, cnt = prog_count();
    int32_t x, y, x0, y0;
    const float kg = en_playing ? en_drum_glow[0] : 0.0f;
    char nm[16];
    for (i = 0; i < ns; i++)
        inkey[(proj.par[P_ROOT] + steps[i]) % 12] = 1;
    circle(WK_X, WK_Y, WK_R, mix(K_BG, K_LINE, 9));   /* the circle */
    if (!en_frozen && en_drift_bars(proj.par[P_DRIFT]))   /* the way to the next chord, clockwise from the top */
        for (j = 0; j < (int)(en_drift * 96.0f + 0.5f); j++) {
            const float a = (float)j * (6.2831853f / 96.0f) - 1.5707963f;
            cv_rect(WK_X + (int32_t)((float)(WK_R + 5) * fm_cosf(a)), WK_Y + (int32_t)((float)(WK_R + 5) * fm_sinf(a)), 2, 2,
                    mix(K_BG, K_PAD, 8));
        }
    for (i = 0; i < 12; i++) {                    /* the notes: named round it, the key's bright; a dot on the circle */
        fq_xy(i, WK_R + 15, &x, &y);
        text_c(x, y - 7, &FONT_XS, EN_NOTE_NAME[i], inkey[i] ? K_DIM : mix(K_BG, K_LINE, 10));
        fq_xy(i, WK_R, &x, &y);
        if (inkey[i])
            cv_disc(x, y, 2, K_DIM);
        else
            cv_disc(x, y, 1, K_LINE);
    }
    for (i = 0; i < EN_NPROG; i++) {              /* the slots' chords, for the list at the left */
        int rr = -1, q, xx;
        if (!slot_is_gap(i) && (!loop || i < cnt)) {
            rr = proj.par[P_CR0 + i] % 12;
            if (slot_is_walk(i))
                walk_written(i, &rr, &q, &xx);
            rr %= 12;
        }
        sr[i] = rr;
    }
    if (walk_fading(plat_ms()) && wk_from != wk_to) {   /* the arrow from the root before, fading */
        const float f = 1.0f - (float)(plat_ms() - wk_t0) / (float)wk_fade_ms();
        const uint16_t c = mix(K_BG, K_ACC, 2 + q16(f * 0.85f));
        float dx, dy, l, ux, uy;
        fq_xy(wk_from, WK_R, &x0, &y0);
        fq_xy(wk_to, WK_R, &x, &y);
        dx = (float)(x - x0), dy = (float)(y - y0), l = fm_maxf(fm_sqrtf(dx * dx + dy * dy), 1.0f);
        ux = dx / l, uy = dy / l;
        {   /* (from the old dot's edge to just short of the new one's; the head two strokes back from its tip) */
            const int32_t xa = x0 + (int32_t)(ux * 4.0f), ya = y0 + (int32_t)(uy * 4.0f),
                          xb = x - (int32_t)(ux * 9.0f), yb = y - (int32_t)(uy * 9.0f);
            line(xa, ya, xb, yb, c);
            line(xa + (int32_t)(-uy), ya + (int32_t)(ux), xb + (int32_t)(-uy), yb + (int32_t)(ux), c);
            line(xb, yb, xb - (int32_t)(ux * 6.0f - uy * 4.0f), yb - (int32_t)(uy * 6.0f + ux * 4.0f), c);
            line(xb, yb, xb - (int32_t)(ux * 6.0f + uy * 4.0f), yb - (int32_t)(uy * 6.0f - ux * 4.0f), c);
        }
        cv_disc(x0, y0, 3, mix(K_BG, K_ACC, 2 + q16(f * 0.6f)));   /* (where it was, fading too) */
    }
    fq_xy(en_chord_root, WK_R, &x, &y);           /* its root */
    cv_disc(x, y, 6, en_frozen ? K_FROST : mix(K_ACC, K_TEXT, q16(kg * 1.4f)));
    chord_now(nm);                                /* at the left: the chord, how it moves, the slots in order */
    if (text_w(&FONT_C, nm) <= 92)
        cv_text(8, 40, &FONT_C, nm, en_frozen ? K_FROST : mix(K_TEXT, K_ACC, 16 - q16(kg * 1.4f)));
    else
        cv_text(8, 44, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
    if (loop && cnt) {
        char t[12] = "LOOP ";
        t[5] = (char)('1' + (en_prog_pos >= 0 ? en_prog_pos : 0)), t[6] = '/', t[7] = (char)('0' + cnt), t[8] = 0;
        cv_text(8, 66, &FONT_XS, t, K_DIM);
    } else {
        char t[12];
        caps(t, EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK], sizeof t);
        cv_text(8, 66, &FONT_XS, t, K_DIM);
    }
    for (i = 0; i < EN_NPROG; i++) {              /* the slots, two columns of four: their chords, the one playing lit */
        const int32_t x = 8 + (i >> 2) * 46, y = 82 + (i & 3) * 14;   /* (clear of the notes' names) */
        const int on = loop && en_playing && i == en_prog_pos;
        char d[2], cn[16];
        d[0] = (char)('1' + i), d[1] = 0;
        if (on)
            cv_round(x - 2, y + 1, 44, 13, 3, K_PAD);
        cv_text(x, y, &FONT_XS, d, on ? K_BG : K_LINE);
        if (sr[i] >= 0) {
            const char *l = slot_label(i, cn);
            cv_text(x + 9, y, &FONT_XS, l, on ? K_BG : slot_is_walk(i) ? K_PAD : K_DIM);
        } else {
            cv_text(x + 9, y, &FONT_XS, "-", K_LINE);
        }
    }
}

/* the Tape: two reels, the tape running from the left one to the right past three heads (erase, record, play: the
 * record head lit with the output, the play head with the echo coming back); the reels turn with the tempo, faster as
 * they're smaller (the tape's speed the same); through a song the tape winds from left to right (where it is), in
 * Endless back and forth over 128 bars */
static void tp_reel(int32_t cx, int32_t cy, int32_t r, float ang, uint16_t tape)
{
    int32_t dy, j;
    for (dy = -r; dy <= r; dy++) {                /* the tape wound on it: a disc, a row at a time */
        const int32_t hw = (int32_t)fm_sqrtf((float)(r * r - dy * dy));
        cv_rect(cx - hw, cy + dy, 2 * hw + 1, 1, tape);
    }
    for (j = r - 4; j > 8; j -= 5)                /* (its layers) */
        circle(cx, cy, j, mix(K_BG, tape, 11));
    circle(cx, cy, 30, K_LINE);                   /* the flange */
    for (j = 0; j < 3; j++) {                     /* the spokes, turning */
        const float a = ang + (float)j * 2.0943951f, c = fm_cosf(a), s = -fm_sinf(a);   /* (anticlockwise, the user's:
                                                  * the tape leaves and joins both at their bottoms, running right) */
        line(cx + (int32_t)(7.0f * c), cy + (int32_t)(7.0f * s), cx + (int32_t)(29.0f * c), cy + (int32_t)(29.0f * s), K_LINE);
    }
    cv_disc(cx, cy, 6, K_PANEL);
    cv_disc(cx, cy, 2, K_DIM);
}
static uint32_t tp_shown;                       /* the tape's TIME, in seconds: kept on every page (tape_clock) */
static void tape_clock(uint32_t now)             /* each frame: from PLAY, a Set's next piece from its own start (before,
                                                  * kept only while the tape was seen: wrong if PLAY came elsewhere) */
{
    static uint32_t t0, last_b;
    static uint8_t was;
    const uint32_t b = en_bars / 16u;
    if ((en_playing && !was) || (en_playing && b < last_b))
        t0 = now;
    was = en_playing, last_b = b;
    if (en_playing)
        tp_shown = (now - t0) / 1000u;
}
static void draw_tape(uint32_t now)
{
    static float al, ar;
    static uint32_t t_last;
    const int32_t lx = 42, rx = 198, cy = 48;   /* (zzr, the user's: the reels apart, the counter between them) */
    const uint32_t dt = t_last ? (now - t_last > 100u ? 100u : now - t_last) : 0u;
    const float bars = (float)en_bars * (1.0f / 16.0f),
                q = en_song_ph != EN_SG_OFF && en_song_be ? fm_minf(bars / (float)en_song_be, 1.0f)
                  : (float)((uint32_t)bars % 256u) * (1.0f / 128.0f),
                p = q <= 1.0f ? q : 2.0f - q;
    const int32_t rl = 9 + (int32_t)(19.0f * (1.0f - p) + 0.5f), rr = 9 + (int32_t)(19.0f * p + 0.5f);
    const float v = en_playing && !en_frozen ? 0.03f * (float)proj.par[P_TEMPO] * (1.0f / 122.0f) : 0.0f;
    /* (zzw, the user's) the tape in its song part's hue, the header line's (the intro teal, the main light, the outro
     * coral); outside a song, the brown tape */
    const int part = en_song_ph == EN_SG_INTRO ? 0 : en_song_ph == EN_SG_MAIN ? 1 : en_song_ph == EN_SG_OUTRO ? 2 : -1;
    const uint16_t tape = part >= 0 ? mix(K_BG, SONG_HUE[part], part == 1 ? 7 : 9) : mix(K_BG, RGB(132, 92, 60), 9), lvl = mix(K_LINE, K_ACC, q16((mt.db[EN_MT_MASTER] + 36.0f) * (1.0f / 36.0f))),
                   ec = mix(K_LINE, K_PAD, q16(fm_minf(epk[(epk_w + EPK_N - 1u) % EPK_N].pk * 4.0f, 1.0f)));
    t_last = now;
    al += v * (float)dt / (float)rl;              /* (the same tape speed: the smaller reel turns faster) */
    ar += v * (float)dt / (float)rr;
    al -= al > 6.2831853f ? 6.2831853f : 0.0f;
    ar -= ar > 6.2831853f ? 6.2831853f : 0.0f;
    line(lx, cy + rl, 84, 86, tape), line(lx + 1, cy + rl, 85, 86, tape);   /* the tape's path, under the reels' edges */
    cv_rect(84, 86, 73, 2, tape);                /* (the deck's run wider: the counter's room above it) */
    line(rx, cy + rr, 156, 86, tape), line(rx - 1, cy + rr, 155, 86, tape);
    tp_reel(lx, cy, rl, al, tape);
    tp_reel(rx, cy, rr, ar, tape);
    cv_rect(96, 88, 10, 5, K_DIM);               /* erase, record, play (each seen, unlit too) */
    cv_rect(115, 88, 10, 5, lvl);
    cv_rect(134, 88, 10, 5, ec);
    {   /* (1.4.0, the user's) the tape's counter, seen nowhere else: at the left, where the song is (INTRO, MAIN,
         * OUTRO; ENDLESS) and the bars left of it (BARS); at its right, the time since PLAY, and in a song its whole length */
        const uint32_t b = en_bars / 16u, shown = tp_shown;
        const int song = en_song_ph == EN_SG_INTRO || en_song_ph == EN_SG_MAIN || en_song_ph == EN_SG_OUTRO;
        const uint16_t tc = en_playing ? K_TEXT : K_DIM;
        enum { TX_L = 95, TX_R = 144, TY_LBL = 20, TY_VAL = 39, TY_SUB = 65 };   /* (zzu, the user's: the three lines evenly spaced, the figures in M) */
        char t[12];
        text_c(TX_L, TY_LBL, &FONT_XS, song ? (en_song_ph == EN_SG_INTRO ? "INTRO" : en_song_ph == EN_SG_MAIN ? "MAIN" : "OUTRO")
                                      : en_song_ph == EN_SG_END ? "END" : "ENDLESS", K_DIM);   /* (TIME's colour) */
        if (song) {
            const uint32_t end = en_song_ph == EN_SG_INTRO ? en_song_b0 : en_song_ph == EN_SG_MAIN ? en_song_bo : en_song_be;
            const uint32_t left = end > b ? end - b : 0u;
            itoa_u(left, t);
            text_c(TX_L, TY_VAL, &FONT_M, t, tc);
            text_c(TX_L, TY_SUB, &FONT_XS, left == 1u ? "BAR" : "BARS", K_DIM);
        }
        {
            uint32_t k = 0, m = shown / 60u, sec = shown % 60u;
            char c[12];
            itoa_u(m, c);
            while (c[k])
                k++;
            c[k++] = ':', c[k++] = (char)('0' + sec / 10u), c[k++] = (char)('0' + sec % 10u), c[k] = 0;
            text_c(TX_R, TY_VAL, &FONT_M, c, tc);
            text_c(TX_R, TY_LBL, &FONT_XS, "TIME", K_DIM);
            if (song && proj.par[P_TEMPO] > 0) {  /* (the song's whole length, at this tempo) */
                const uint32_t tot = en_song_be * 240u / (uint32_t)proj.par[P_TEMPO];
                k = 0;
                c[k++] = 'O', c[k++] = 'F', c[k++] = ' ';
                itoa_u(tot / 60u, c + k);
                while (c[k])
                    k++;
                c[k++] = ':', c[k++] = (char)('0' + tot % 60u / 10u), c[k++] = (char)('0' + tot % 10u), c[k] = 0;
                text_c(TX_R, TY_SUB, &FONT_XS, c, K_DIM);
            }
        }
    }
}

static void draw_main(void)
{
    uint32_t now = plat_ms(), h = hash(2166136261u, (uint32_t)en_frozen << 16 | proj.piece << 24 | (uint32_t)en_playing << 17), i;
    static uint8_t moving;
    en_view_layer = (uint8_t)(scr_layer(ui.view) + 1);   /* (the scope and spectrum: this page's layer, or the whole) */
    echo_take(now);                              /* (the ripples: each frame, so the repeats are measured on time) */
    meter_take(now);                             /* (the meters too, and the clip light) */
    wk_root_seen(now);                           /* (the Walk page's arrow: the root's changes, on any page) */
    ripples_take(now);
    if (ui.frame % 3u)                           /* 20 frames a second is plenty */
        return;
    if (grid_up()) {                             /* REC held: the effects grid (ui_frame draws it, whole;
                                                  * before the Chords and Pattern pages, which would draw over it) */
        if (!ui.grid_said && ann_on()) {         /* the grid in words: the white keys' effects, a group at a time */
            int e;
            sp_n = 0;
            sp_buf[0] = 0;
            sp_cat("EFFECTS GRID. White keys");
            for (e = 0; e < EN_NFX; e++) {
                sp_cat(e == 0 ? ": " : (e & 3) ? ", " : "; ");
                sp_cat(EN_FX_NAME[e]);
            }
            ann_say(sp_buf, 0, ANN_NOW);
            ui.grid_said = 1;
        }
        return;
    }
    ui.grid_said = 0;
    if (ui.view == V_PAT) {                      /* the Pattern page: the steps */
        h = hash(hash(0x50415454u, ui.lane | (uint32_t)proj.par[P_DMODE] << 4 | (uint32_t)en_drum_pos[0] << 8), en_playing);
        for (i = 0; i < EN_NDRUM; i++)
            h = hash(hash(h, lane_steps((int)i)), (uint32_t)(uint16_t)proj.par[P_DACC0 + i]);
        h = hash(h, (uint32_t)oct_ring_on(now) | (uint32_t)big_on(now) << 8);
        if (h != ui.sig[1] || oct_ring_on(now) || big_on(now)) {
            ui.sig[1] = h;
            main_begin(K_BG);
            draw_pattern();
            draw_oct(now);
            draw_big(now);
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_CHORDS) {                   /* the Chords page: the slots */
        h = hash(hash(0x43484F52u, ui.slot | (uint32_t)proj.par[P_PMODE] << 4), (uint32_t)en_prog_pos + 1u);
        for (i = 0; i < EN_NPROG; i++)
            h = hash(h, (uint32_t)proj.par[P_CR0 + i] | (uint32_t)proj.par[P_CQ0 + i] << 4 | (uint32_t)proj.par[P_CX0 + i] << 8);
        if (h != ui.sig[1] || big_on(now)) {
            ui.sig[1] = h;
            main_begin(K_BG);
            draw_slots();
            draw_big(now);
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_WALK) {                     /* (1.4.0) the Walk page: the circle of fifths, the slots' path */
        h = hash(hash(0x57414C4Bu, ui.slot | (uint32_t)proj.par[P_PMODE] << 4), (uint32_t)en_prog_pos + 1u);
        for (i = 0; i < EN_NPROG; i++)
            h = hash(h, (uint32_t)proj.par[P_CR0 + i] | (uint32_t)proj.par[P_CQ0 + i] << 4 | (uint32_t)proj.par[P_CX0 + i] << 8);
        h = hash(where_sig(h), (uint32_t)en_chord_root | (uint32_t)(en_drift * 96.0f) << 4 | (uint32_t)en_frozen << 12 |
                 (uint32_t)proj.par[P_ROOT] << 13 | (uint32_t)proj.par[P_MODE] << 17 | (uint32_t)proj.par[P_DRIFT] << 22);
        for (i = 0; i < EN_NPAD; i++)
            h = hash(h, en_chord[i]);
        if (h != ui.sig[1] || big_on(now) || (en_playing && en_drum_glow[0] > 0.01f) || walk_fading(now)) {   /* (the root
                                                  * white with the kick; the arrow fading) */
            ui.sig[1] = h;
            main_begin(K_BG);
            draw_walk();
            draw_where(now);
            draw_big(now);
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    if (ui.view == V_PERF || ui.view == V_STRIP || ui.view == V_SIREN) {   /* (1.4.0) the Arrangement page: the meters */
        h = hash(where_sig(hash(desk_sig(), ui.view)), (uint32_t)card_on(now) | (uint32_t)big_on(now) << 1 |
                 (en_phrase_bar == 7 && en_change_next ? (now / 200u & 1u) << 2 : 0u) | (uint32_t)en_drift_bars(proj.par[P_DRIFT]) << 3);
        if (h != ui.sig[1] || big_on(now) || card_on(now)) {
            ui.sig[1] = h;
            main_begin(K_BG);
            draw_desk();
            draw_where(now);
            draw_big(now);
            draw_card(now);
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < 6; i++)
        h = hash(h, en_lane[i] | (uint32_t)proj.par[i < 4 ? i : 0] << 16);
    h = hash(h, en_drum_pos[0]);                 /* (1.4.0: the duck meter gone; the lanes' meters at the left, their lit
                                                  * lengths; the pings' and the noise's rows) */
    h = hash(h, lm_sig());
    h = hash(hash(h, en_lane_png), en_nz_pat ^ (uint32_t)(en_nz_step >> 4) << 31 ^ (uint32_t)proj.par[P_CRACKLE] << 24);
    h = hash(h, (uint32_t)(en_prog_pos + 1) << 12);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | en_section << 8 | (uint32_t)en_phrase_bar << 12 | (uint32_t)ui.perf << 16 | ui.solo << 20 |
                (uint32_t)ui.lock << 28);
    for (i = 0; i < EN_NLOOP; i++)
        h = hash(h, (uint32_t)q16(en_loop_glow[i]) | (uint32_t)en_loop_pos[i] << 8);
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, (uint32_t)q16(en_drum_glow[i]));
    h = hash(h, ui.view | (uint32_t)oct_ring_on(now) << 8 | (uint32_t)strip_part << 12);
    if (lfo_page(ui.view))                       /* (the LFO pages: the trace, a sixteenth at a time) */
        h = hash(h, en_lfo_w);
    h = hash(h, (uint32_t)en_song_ph | en_song_be << 4 | (uint32_t)(proj.par[P_SINTRO] | proj.par[P_SMAIN] << 5 |
                                                                     proj.par[P_SOUTRO] << 12 | proj.par[P_SKIND] << 17 |
                                                                     proj.par[P_SMODE] << 18) << 4);   /* (1.4.0: the
                                                  * song line, the Song pages) */
    h = hash(h, en_bars / 16u);                  /* (their playhead, a bar at a time) */
    if (ui.view == V_SETUP) {                    /* (1.4.0: the audio's stages, every ~2 s) */
        uint32_t pk[PLAT_NPROF], mn[PLAT_NPROF], k;
        plat_prof(pk, mn);
        for (k = 0; k < PLAT_NPROF; k++)
            h = hash(h, pk[k] ^ mn[k] << 16);
    }
    if (parent_of(ui.view) >= 0) {               /* a sound or FM page: redrawn as its values move */
        for (i = 0; i < 4; i++)
            h = hash(h, (uint32_t)(knob_get(vknob(ui.view, i)) + 64));   /* (1.4.0: K_NONE's too, no parameter) */
        h = hash(h, (uint32_t)proj.par[view_voice(ui.view)] | (uint32_t)proj.par[P_DECAY] << 8 | (uint32_t)proj.par[P_GROOVE] << 16 |
                    (uint32_t)(proj.par[P_CLDEST] | proj.par[P_NLDEST] << 2) << 24);   /* (1.4.0: the LFO pages'
                                                  * Destinations) */
    }
    h = hash(h, (uint32_t)card_on(now) | (en_phrase_bar == 7 && en_change_next ? (now / 200u & 1u) << 1 : 0u));
    if (h == ui.sig[1] && !moving && !oct_ring_on(now) && !card_on(now))
        return;
    ui.sig[1] = h;
    main_begin(K_BG);
    if (ui.view >= V_CFM && ui.view <= V_BFM) {  /* an FM page: the algorithm */
        draw_alg(layer_of(ui.view));
        moving = 0;
    } else if (lfo_page(ui.view)) {              /* (1.4.0) an LFO page: its path */
        draw_lfo(ui.view == V_NLFO);
        moving = 0;
    } else if (proj.screen == SCR_SCOPE) {       /* (HOME again: a scope, a spectrum, the ripples; 1.4.0: the tape) */
        draw_scope();
        moving = 1;
    } else if (proj.screen == SCR_SPEC) {
        draw_spec();
        moving = 1;
    } else if (proj.screen == SCR_TAPE) {
        cv_clip(0, PIC_Y1);
        draw_tape(now);
        cv_clip(0, MAIN_DH);
        moving = 1;
    } else {
        specks_take(now);
        cv_clip(0, PIC_Y1);                      /* (1.4.0: the rings out to the sides, above the lanes) */
        moving = rip_src < 0 || rip_src == 4 ? (uint8_t)draw_specks(now) : 0;   /* (the noise, under the rings: on
                                                  * the whole, and alone on the Noise pages: they follow HOME's screen
                                                  * as the others do, 1.4.0) */
        moving |= (uint8_t)draw_ripples(now);
        if (rip_src < 0 || rip_src == 1)         /* (the ping loops: on the whole, and the Pings page) */
            draw_orbits();
        cv_clip(0, MAIN_DH);
    }
    const int named = (parent_of(ui.view) < 0 || scr_layer(ui.view) >= 0) && !lfo_page(ui.view);   /* (a sound page:
                                                  * as its layer's; the LFO pages: the trace there) */
    static char ch_last[16];
    static uint32_t ch_t;
    char nm[16];
    uint32_t ch_age;
    uint16_t ch_c;
    chord_now(nm);
    if (!str_begins(nm, ch_last) || !str_begins(ch_last, nm)) {                   /* (1.3.0) a new chord: it comes in big and teal, then settles */
        if (ch_last[0])
            ch_t = now | 1u;
        if (card_on(now) && str_begins(ui.card_head, "NEXT CHORD") && now < ui.card_until)
            ui.card_until = now - CARD_FADE;     /* (the next chord's card gone at once, for it) */
        memcpy(ch_last, nm, sizeof ch_last);
    }
    ch_age = ch_t ? now - ch_t : 1000u;
    ch_c = en_frozen ? K_FROST : ch_age < 900u ? mix(K_ACC, K_TEXT, (int)(ch_age * 16u / 900u)) : K_TEXT;
    if (!en_frozen && ch_age >= 900u) {          /* (1.4.0, the user's: the kick, on every page that names the chord:
                                                  * its name a soft grey, white with each kick, as strong as it is) */
        const float kg = en_playing ? en_drum_glow[0] : 0.0f;
        ch_c = mix(K_DIM, K_TEXT, q16(kg * 1.4f));
        if (kg > 0.01f)
            moving = 1;
    }
    if (ch_age < 900u)
        moving = 1;
    if (named && scr_small_chord(proj.screen)) {   /* the chord: over the scope, the spectrum, the tape: small */
        text_c(CX, MAIN_SHIFT + 1, &FONT_S, nm, ch_c);   /* (where it was: the canvas's top) */
    } else if (named) {                          /* the chord, in the middle */
        if ((ch_age < 200u || (ch_age >= 400u && ch_age < 600u)) && text_w(&FONT_L, nm) <= 96) {   /* (big, small, big,
                                                                                         * small: a ping-pong) */
            int32_t w = text_w(&FONT_L, nm);
            cv_round(CX - w / 2 - 4, CY - 15, w + 8, 30, 6, K_BG);
            text_c(CX, CY - 16, &FONT_L, nm, ch_c);
        } else if (text_w(&FONT_C, nm) <= 66) {  /* (the bigger face, the rings behind it; a long name, smaller) */
            int32_t w = text_w(&FONT_C, nm);
            cv_round(CX - w / 2 - 3, CY - 9, w + 6, 19, 5, K_BG);
            text_c(CX, CY - 11, &FONT_C, nm, ch_c);
        } else if (text_w(&FONT_B, nm) <= 72) {
            int32_t w = text_w(&FONT_B, nm);
            cv_round(CX - w / 2 - 3, CY - 7, w + 6, 15, 4, K_BG);
            text_c(CX, CY - 8, &FONT_B, nm, ch_c);
        }
        else
            text_c(CX, CY - 6, &FONT_XS, nm, ch_c);
        if ((proj.par[P_PMODE] & EN_PLOOP) && en_prog_pos >= 0 && prog_count()) {   /* Loop: where it is ("2/4") */
            char t[8];
            t[0] = (char)('1' + en_prog_pos), t[1] = '/', t[2] = (char)('0' + prog_count()), t[3] = 0;
            text_c(CX, CY + 11, &FONT_XS, t, K_DIM);
        }
    }
    draw_where(now);
    draw_lanes();
    if (ui.view == V_SETUP) {                    /* (1.4.0) Setup: the CPU's clock, measured, and its registers */
        static const char HX[] = "0123456789ABCDEF";
        uint32_t r[3], m = plat_clock_mhz(), i, j, k = 0;
        char t[40];
        plat_clock_regs(r);
        const int stock = r[0] == 6u && r[1] == 0x0940022Bu && r[2] == 0x0750310Cu;   /* (M-VAVE's bootloader's: 360 MHz) */
        if (m) {                                 /* (measured, if the core's cycle counter runs: it doesn't on the
                                                  * user's FM-1) */
            itoa_u(m, t);
            for (k = 0; t[k]; k++)
                ;
            t[k++] = ' ', t[k++] = 'M', t[k++] = 'H', t[k++] = 'z', t[k] = 0;
        } else {                                 /* (else from the registers: the stock words, or not) */
            const char *w = stock ? "360 MHz (stock)" : "OTHER";
            for (k = 0; w[k]; k++)
                t[k] = w[k];
            t[k] = 0;
        }
        /* (the user's: the lines over the moving picture hard to read; the picture kept, it's part of the load) a
         * see-through panel under them, the picture a fifth through */
        cv_shade(4, MAIN_SHIFT + 1, text_w(&FONT_XS, "PEAK 9.9 DRN9.9 BAS9.9 PNG9.9 DRM9.9") + 8, 72, 5, 13u, K_BG);
        cv_text(8, MAIN_SHIFT + 4, &FONT_XS, "CLOCK", K_DIM);
        cv_text(48, MAIN_SHIFT + 4, &FONT_XS, t, (m ? m >= 350u : stock) ? K_PAD : K_CLAP);
        k = 0;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 8; j++)
                t[k++] = HX[r[i] >> (28u - 4u * j) & 15u];
            t[k++] = ' ';
        }
        t[k - 1] = 0;
        cv_text(8, MAIN_SHIFT + 18, &FONT_XS, t, mix(K_BG, K_DIM, 12));
        cv_text(8, MAIN_SHIFT + 32, &FONT_XS, "BUILD " EN_BUILD, mix(K_BG, K_DIM, 12));   /* (which build runs) */
        {   /* (1.4.0, the FM-1's peaks) the audio's time by stage, in ms: the heaviest block of the last ~2 s and the
             * window's mean, each its total and its four biggest stages */
            static const char *const SN[PLAT_NPROF] = {"DRN", "CTL", "FAD", "STB", "BAS", "PNG", "WID", "DRM", "NOI",
                                                       "FX", "RMB", "MIX", "EVT"};
            uint32_t pk[PLAT_NPROF], mn[PLAT_NPROF], *v, row;
            plat_prof(pk, mn);
            for (row = 0; row < 2; row++) {
                uint32_t tot = 0, used = 0, n;
                v = row ? mn : pk;
                for (i = 0; i < PLAT_NPROF; i++)
                    tot += v[i];
                k = 0;
                {
                    const char *h = row ? "MEAN " : "PEAK ";
                    for (j = 0; h[j]; j++)
                        t[k++] = h[j];
                }
                for (n = 0; n < 5; n++) {        /* (the total first, then the four biggest) */
                    uint32_t x = tot, b = 0, best = PLAT_NPROF;
                    if (n) {
                        for (i = 0; i < PLAT_NPROF; i++)
                            if (!(used >> i & 1u) && v[i] >= b)
                                b = v[i], best = i;
                        if (best == PLAT_NPROF || !b)
                            break;
                        used |= 1u << best;
                        x = b;
                        for (j = 0; SN[best][j]; j++)
                            t[k++] = SN[best][j];
                    }
                    x = (x + 50u) / 100u;        /* (tenths of a ms) */
                    t[k++] = (char)('0' + x / 10u % 10u);
                    t[k++] = '.';
                    t[k++] = (char)('0' + x % 10u);
                    t[k++] = ' ';
                    if (k > 34)
                        break;
                }
                t[k ? k - 1 : 0] = 0;
                cv_text(8, MAIN_SHIFT + 46 + 14 * (int32_t)row, &FONT_XS, t, K_DIM);
            }
        }
    }
    draw_oct(now);
    draw_big(now);
    draw_card(now);
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static int ride_on_knob(int k, float *at)        /* a strip's knob the mixer is riding: where it is now (knob units) */
{
    int w;
    if (ui.view != V_STRIP)                      /* (format 30: the rides Drift's, the moves the Mixer's) */
        return 0;
    for (w = 0; w < 3; w++)
        if (STRIP_PAR[strip_part][w] == k) {
            const float v = (float)proj.par[k];
            *at = fm_clampf(ride_knob(strip_part, w, v), 0.0f, 100.0f);
            return fm_fabsf(*at - v) >= 0.5f;
        }
    return 0;
}
/* (1.4.0, the user's: the knobs without their numbers; then the room they freed used) the strip's geometry: a ring's
 * centre, radius and thickness (16 and 5, filling the strip where the numbers were; 14 and 4.2 before, 12 and 3.4 at
 * first), the names' line, the numbers' peek as a page opens and its fade */
#define KNB_CY 46
#define KNB_R 16
#define KNB_TH 5.0f
#define KNB_NAME_Y 13
#define KNB_FADE 300u
static int text_is_number(const char *t)         /* a value that's a number alone ("55", "+20", "-5", "100%"): the ring
                                                  * says it; a word or a unit kept ("Wander", "64 bars", "LP 800 Hz") */
{
    int n = 0;
    for (; *t; t++, n++)
        if (!((*t >= '0' && *t <= '9') || *t == '+' || *t == '-' || *t == '%' || *t == '.'))
            return 0;
    return n > 0;
}
static int text_has_digit(const char *t)
{
    for (; *t; t++)
        if (*t >= '0' && *t <= '9')
            return 1;
    return 0;
}
/* (1.4.0, the user's) the three kinds of knob, each drawn for what it is: a ring alone (a number: its peek inside it),
 * a ring with its unit inside (the strip's Filter's "LP 2.6k", the song's "32 bars", the Feedback's seconds), or a
 * selector (a word: the word large where the ring would be, a dot an option under it). A selector: a few positions
 * (under ten: the old switches, the Slot, the LFO's rate, Change), or named values (a voice, the Root, the Mode);
 * judged by the knob's ends, not its value now (the Filter's "Off" at 0 stays a unit) */
enum { KK_RING, KK_UNIT, KK_SEL };
static void knob_text(int k, char *b);
static int vary_trim(int k)                      /* (format 29: a layer's Vary trim; 30: an LFO's Trim) */
{
    return k == P_CVARY || (k >= P_PVARY && k <= P_NVARY) || k == P_CLTRIM || k == P_NLTRIM;
}
static int knob_kind(int k, int lo, int hi)
{
    char a[16], z[16];
    if (k == K_CPU || vary_trim(k))              /* (a layer's Vary trim: a centred ring, Hold its far left) */
        return KK_RING;
    if (hi - lo < 10)
        return KK_SEL;
    if (k < P_NPARAMS) {
        en_param_text(k, lo, a);
        en_param_text(k, hi, z);
        if (!text_has_digit(a) && !text_has_digit(z))
            return KK_SEL;
        return text_is_number(a) && text_is_number(z) ? KK_RING : KK_UNIT;
    }
    knob_text(k, a);
    return !text_has_digit(a) ? KK_SEL : text_is_number(a) ? KK_RING : KK_UNIT;
}
static void knob_label(int32_t cx, const char *t, uint16_t c)   /* a knob's name: a size up where it fits its cell */
{
    text_c(cx, KNB_NAME_Y, text_w(&FONT_S, t) <= 56 ? &FONT_S : &FONT_XS, t, c);
}
static const felucca_font_t *knob_font(const char *t, int32_t w)   /* the biggest face that fits w */
{
    return text_w(&FONT_B, t) <= w ? &FONT_B : text_w(&FONT_S, t) <= w ? &FONT_S : &FONT_XS;
}
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int peek;
    if (view_voice(ui.view) >= 0)
        h = hash(h, (uint32_t)proj.par[view_voice(ui.view)]);
    if (ui.view == V_CHORDS || ui.view == V_WALK)
        h = hash(h, (uint32_t)proj.par[P_PMODE]);
    if (noise_page(ui.view))
        h = hash(h, (uint32_t)proj.par[P_NCHAR]);
    h = hash(h, (uint32_t)proj.par[P_CLDEST] | (uint32_t)proj.par[P_NLDEST] << 4);   /* (1.4.0: the LFOs' knobs, and the
                                                  * LFO pages' rule) */
    if (ui.view == V_STRIP)
        h = hash(h, strip_part);
    if (ui.view == V_SIREN)
        h = hash(h, sir_page);
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++) {
        h = hash(h, (uint32_t)knob_get(vknob(ui.view, i)));
        float ln, lr;
        if (lfo_on_knob(vknob(ui.view, i), &ln, &lr))   /* (an LFO moving its knob: redrawn as it moves) */
            h = hash(h, (uint32_t)(int32_t)(ln * 440.0f) ^ (uint32_t)(int32_t)(lr * 440.0f) << 12);
        if (ride_on_knob(vknob(ui.view, i), &ln))   /* (the dub mixer riding it: the same) */
            h = hash(h, (uint32_t)(int32_t)(ln * 4.0f));
    }
    {   /* (1.4.0) the numbers' peek: 16 shown, fading over its last KNB_FADE ms, 0 gone; always with Setup's Values */
        const uint32_t now = plat_ms();
        peek = proj.values ? 16 : now >= ui.peek_until ? 0 : ui.peek_until - now >= KNB_FADE ? 16
             : (int)((ui.peek_until - now) * 16u / KNB_FADE);
    }
    h = hash(h, (uint32_t)now_touch | (uint32_t)peek << 4);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule (1.4.0, the user's): at its left, where you are: the page's name in the accent and its place in
         * grey ("CHORD VOICE 2/4"); at its right, the page's voice (the kit, the noise's character, the walk, the
         * LFO's destination); always (the user's: a knob's help line is the big ring's, in the middle) */
        char t[12], pn[20];
        int32_t xl, xr = 234;
        int n, m;
        caps(pn, view_name(ui.view), sizeof pn);
        t[0] = 0;
        if (view_voice(ui.view) >= 0 || ui.view == V_CHORDS || noise_page(ui.view)) {   /* a layer's page: its voice; the
                                                  * Chords page: the walk's; Noise: its character */
            char vn[12];
            if (ui.view == V_CHORDS || noise_page(ui.view) || ui.view == V_CLFO) {
                const char *w = ui.view == V_CLFO ? EN_CLDEST_NAME[proj.par[P_CLDEST] % EN_NCLDEST]
                              : ui.view == V_NLFO ? EN_NLDEST_NAME[proj.par[P_NLDEST] % EN_NNLDEST]
                              : ui.view != V_CHORDS ? EN_NCHAR_NAME[proj.par[P_NCHAR] % EN_NNCHAR]
                                                    : EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK];
                int j = 0;
                if (ui.view == V_CLFO || ui.view == V_NLFO)   /* (1.4.0: ALGORITHM's there, the LFO's Destination: "TO
                                                  * RESO", "TO AIR") */
                    vn[j++] = 'T', vn[j++] = 'o', vn[j++] = ' ';
                for (; j < 11 && *w; j++)
                    vn[j] = *w++;
                vn[j] = 0;
            } else {
                en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
            }
            caps(t, vn, sizeof t);
        }
        if (ui.view == V_SIREN) {                /* (format 34, the user's: "SIREN · WAIL", the dot drawn: the font has
                                                  * none) */
            caps(pn, EN_SIR_NAME[sir_page % EN_NSIR], sizeof pn);
            xl = cv_text(6, 1, &FONT_XS, "SIREN", K_ACC);
            cv_rect(xl + 3, 6, 2, 2, K_ACC);
            xl = cv_text(xl + 8, 1, &FONT_XS, pn, K_ACC) + 5;
            cv_rect(xl, 7, 234 - xl, 1, K_LINE);
        } else {
            xl = cv_text(6, 1, &FONT_XS, pn, K_ACC);   /* (1.4.0, the user's: 2 px more above and below) */
            page_pos(ui.view, &n, &m);
            if (m > 1) {
                char d[6];
                d[0] = (char)('0' + n), d[1] = '/', d[2] = (char)('0' + m), d[3] = 0;
                xl = cv_text(xl + 5, 1, &FONT_XS, d, K_DIM);
            }
            xl += 5;
            if (t[0]) {                           /* (the voice at the right, if it fits: else left out) */
                const int32_t tw = text_w(&FONT_XS, t);
                if (234 - tw - 10 >= xl) {
                    xr = 234 - tw;
                    cv_text(xr, 1, &FONT_XS, t, K_DIM);
                    xr -= 5;
                }
            }
            if (xr > xl)
                cv_rect(xl, 7, xr - xl, 1, K_LINE);
        }
    }
    cv_oy = 4;                                   /* (the knobs under it 4 px lower: the strip 72 px, the rule's room) */
    for (i = 0; i < 4; i++) {
        const int off = fm_knob_off(ui.view, (int)i);   /* (a voice not FM: all grey; Ratio on two ops) */
        int k = vknob(ui.view, i), lo, hi, v, hot = now_touch && ui.touched == (int)i && !off, kind;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        static const uint16_t STC[NSTRIP] = {K_PAD, K_BELL, K_BASS, K_PULSE, K_TEXT, K_NOISE};   /* (a strip: its part's) */
        uint16_t bg = hot ? K_PANEL : K_BG, fill = off ? K_LINE : ui.view == V_STRIP ? STC[strip_part] : ui.view == V_SIREN ? K_FROST
                                                                                                   : hot ? K_ACC : K_PAD;
        if (k == K_NONE && KNOB_LABEL[ui.view][i][0]) {   /* (1.4.0: a knob to come: its name greyed, its ring empty) */
            caps(t, KNOB_LABEL[ui.view][i], sizeof t);
            if (i)
                cv_rect(x, 16, 1, 50, K_LINE);
            knob_label(cx, t, K_LINE);
            arc_th(cx, KNB_CY, KNB_R, KNB_TH, 0.0f, mix(K_BG, K_LINE, 10), K_LINE, K_BG, 0);
            continue;
        }
        if (k == K_NONE)
            continue;
        if (i)
            cv_rect(x, 16, 1, 50, K_LINE);
        if (hot)
            cv_round(x + 3, 10, 54, 58, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = off ? 0 : knob_get(k);
        caps(t, KNOB_LABEL[ui.view][i], sizeof t);
        knob_label(cx, t, off ? K_DIM : K_TEXT);   /* (1.4.0, the user's: the names brighter, a size up, 2 px lower) */
        if (off && k < P_NPARAMS)
            en_param_text(k, 0, t);
        else
            knob_text(k, t);
        kind = k == K_SAVE ? KK_SEL : knob_kind(k, lo, hi);
        if (k == K_SAVE) {                       /* (Save: its word, a dot under it lit while there's something to save) */
            text_c(cx, KNB_CY - 12, &FONT_B, t, off ? K_DIM : hot ? K_ACC : K_TEXT);
            cv_disc(cx, KNB_CY + 12, 3, v ? K_ACC : K_LINE);
        } else if (kind == KK_SEL) {             /* a selector: the word large, where a ring would be; its options as dots
                                                  * under it (a thin bar with its place, past twelve) */
            const int n = hi - lo + 1, at = v - lo;
            const felucca_font_t *f = knob_font(t, 56);
            text_c(cx, KNB_CY - (f == &FONT_XS ? 10 : 12), f, t, off ? K_DIM : hot ? K_ACC : K_TEXT);
            if (n <= 12) {
                const int32_t gap = n > 8 ? 4 : 6, w0 = (n - 1) * gap;
                int j;
                for (j = 0; j < n; j++) {
                    const int32_t dx = cx - w0 / 2 + j * gap;
                    if (j == at)
                        cv_round(dx - 2, KNB_CY + 9, 5, 5, 2, fill);
                    else
                        cv_rect(dx, KNB_CY + 11, 1, 1, K_DIM);
                }
            } else {
                const int32_t w0 = 40, px = cx - w0 / 2 + (n > 1 ? at * (w0 - 4) / (n - 1) : 0);
                cv_rect(cx - w0 / 2, KNB_CY + 11, w0, 1, K_LINE);
                cv_round(px, KNB_CY + 9, 5, 5, 2, fill);
            }
        } else {
            float nw, rr;
            float ra;
            const int lfo_on = lfo_on_knob(k, &nw, &rr) && !off, ride = !lfo_on && ride_on_knob(k, &ra) && hi > lo;   /* (an
                                                  * LFO on it: its dot the one; the dub mixer riding it, as alike) */
            if (lo < 0)                          /* (a centred knob: its 0 marked over the ring's top) */
                cv_rect(cx, KNB_CY - KNB_R - 3, 1, 2, K_DIM);
            arc_th(cx, KNB_CY, KNB_R, KNB_TH, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg,
                   (lo < 0) | (lfo_on || ride ? 2 : 0));
            if (ride) {                          /* (1.4.0, the user's: the dub mixer) from the knob's value to where the
                                                  * ride has it now, a dot there (the LFO's look) */
                const float c = (float)(v - lo) / (float)(hi - lo), r = (ra - (float)lo) / (float)(hi - lo);
                /* (the user's: brighter and thicker, a ride of a few dB read at a glance: the band near white, over 3
                 * px outside the ring and inside it, the dot bigger on a dark rim) */
                arc_span_w(cx, KNB_CY, KNB_R, fm_minf(c, r), fm_maxf(c, r), r, mix(fill, K_TEXT, 14), K_TEXT, 1);
            }
            if (lfo_on) {   /* (1.4.0, the user's) the chord's LFO and drift on it (its Destination's knob):
                                                  * their reach a brighter teal, a dot where it is now (the Cutoff a
                                                  * step of 0.055 octave; Reso and Colour as far along theirs) */
                const float c = (float)(v - lo) / (float)(hi - lo);
                arc_span(cx, KNB_CY, KNB_R, c - rr, c + rr, c + nw, mix(fill, K_TEXT, 11), en_playing ? K_TEXT : mix(fill, K_TEXT, 11));
            }
            if (vary_trim(k))                    /* (1.4.0, format 29: a Vary trim's far left, Hold, marked) */
                arc_mark_th(cx, KNB_CY, KNB_R, KNB_TH, 0.0f, K_TEXT);
            if (!off)                            /* (format 31: a knob's zones, its ring in segments: Motion's) */
                arc_zones_th(cx, KNB_CY, KNB_R, KNB_TH, k, v, mix(K_LINE, fill, 3), bg);
            {   /* (1.4.0, the user's) inside the ring: a unit's value always, small ("LP 2.6k", "32 bars"); a number only
                 * a moment as the page opens (or always, Setup's Values), as big as fits */
                const int32_t in = 2 * (KNB_R - (int32_t)KNB_TH) - 2;
                const int f = kind == KK_UNIT || (vary_trim(k) && v <= lo) ? 16 : peek;   /* (Hold: said always) */
                if (f > 0) {
                    const felucca_font_t *fn = kind == KK_UNIT ? &FONT_XS : knob_font(t, in);
                    if (kind == KK_UNIT && text_w(&FONT_XS, t) > in + 6) {   /* (too wide: the unit's word dropped, the
                                                  * figure kept: "LP 2.6k" as "2.6k", "32 bars" as "32") */
                        char *sp = t;
                        while (*sp && *sp != ' ')
                            sp++;
                        if (*sp == ' ' && sp == t + 2 && t[0] >= 'A' && t[0] <= 'Z') {   /* ("LP 2.6k": the figure) */
                            char *d = t, *q = sp + 1;
                            while ((*d++ = *q++))
                                ;
                        } else if (*sp == ' ') {     /* ("32 bars": the figure alone) */
                            *sp = 0;
                        }
                    }
                    text_c(cx, KNB_CY - (fn == &FONT_XS ? 8 : 9), fn, t, mix(K_BG, off ? K_DIM : hot ? K_ACC : K_TEXT, f));
                }
            }
        }
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    if (VIEW_BTN[page_of(ui.view)] != B_NONE && ((parent_of(ui.view) < 0 && ui.view != V_KEY && ui.view != V_ESND &&
                                                  (!noise_page(ui.view) || ui.view == V_NSND)) || (plat_ms() / 300u) & 1u))
        b |= 1u << VIEW_BTN[page_of(ui.view)];   /* (a sound page, or Key: its button blinks) */
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (ui.view == V_PAT && !ui.perf && !grid_up()) {   /* the Pattern page: the lane's steps on the white keys */
        uint32_t p = lane_steps(ui.lane);
        for (i = 0; i < 16; i++)
            if (p >> i & 1u)
                k |= 1u << STEP_KEY[i];
    } else if (ui.view == V_CHORDS && !ui.perf && !grid_up()) {   /* the Chords page: the slot's root (if it has one) */
        if (!slot_is_gap(ui.slot)) {
            int r = proj.par[P_CR0 + ui.slot], q, x;
            if (slot_is_walk(ui.slot))
                walk_written(ui.slot, &r, &q, &x);
            for (i = 0; i < NKEYS; i++)
                if ((5u + i) % 12u == (uint32_t)r)
                    k |= 1u << i;
        }
    } else if (grid_up()) {                      /* the grid: each group's first key, and the effects held */
        for (i = 0; i < EN_NFX; i++)
            if (!(i & 3) || (ui.fx >> i & 1u))
                k |= 1u << FX_KEY[i];
    } else if (ui.perf) {                        /* PERFORM: the layers heard, the solos, the moves (dark while held) */
        const int blink = (plat_ms() / 250u) & 1u;
        b |= 1u << B_SEL;
        for (i = 0; i < PERF_NP; i++)            /* (white 7-8, the gap: dark) */
            if (perf_sounds((int)i))
                k |= 1u << PERF_KEY[i];
        for (i = 0; i < PERF_NM; i++)
            if (!(ui.moves & PERF_MBIT[i]))
                k |= 1u << PERF_KEY[PERF_NP + i];
        for (i = 0; i < PERF_NF; i++)
            if (!(ui.pfx >> PERF_FX[i] & 1u))
                k |= 1u << PERF_KEY[PERF_F0 + i];
        for (i = 0; i < PERF_NB; i++)            /* (a throw: dark while held; a solo: blinking) */
            if (!(ui.moves & PERF_BTHR[i]) && (!(ui.solo >> PERF_BPART[i] & 1u) || blink))
                k |= 1u << PERF_KEY[PERF_B0 + i];
        for (i = 0; i < PERF_NS; i++)            /* (a siren: dark while it sounds) */
            if (en_sir_on != i + 1)
                k |= 1u << PERF_SKEY[i];

    } else if (proj.leds) {
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    }
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Stabs 30": the name the page gives it, then its value (format 31: and
                                                  * its zone, "Motion 30, Breathes") */
{
    const zone_t *z;
    const int k = vknob(view, i);
    sp_cat(KNOB_LABEL[view][i]);
    sp_cat(" ");
    sp_value(k);
    if (k < P_NPARAMS && knob_zones(k, &z)) {
        sp_cat(", ");
        sp_cat(z[zone_at(k, knob_get(k))].name);
    }
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, playing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(view_name(ui.view));
    sp_cat(" page");
    if (ui.view == V_NLFO) {                     /* (1.4.0: ALGORITHM's there, where the noise's LFO goes) */
        sp_cat(", destination ");
        sp_cat(EN_NLDEST_NAME[proj.par[P_NLDEST] % EN_NNLDEST]);
    } else if (noise_page(ui.view)) {
        sp_cat(", character ");
        sp_cat(EN_NCHAR_NAME[proj.par[P_NCHAR] % EN_NNCHAR]);
    } else if (ui.view == V_CLFO) {              /* (1.4.0: ALGORITHM's there, where the LFO goes) */
        sp_cat(", destination ");
        sp_cat(EN_CLDEST_NAME[proj.par[P_CLDEST] % EN_NCLDEST]);
    } else if (view_voice(ui.view) >= 0) {
        char vn[12];
        en_param_text(view_voice(ui.view), proj.par[view_voice(ui.view)], vn);
        sp_cat(view_voice(ui.view) == P_KIT ? ", kit " : ", voice ");
        sp_cat(vn);
        if (fm_off(ui.view))                     /* (1.4.0: the FM page on a voice not FM: its knobs grey, but the
                                                  * chord's Colour) */
            sp_cat(", not FM");
    }
    if (ui.view == V_SONG) {                     /* (1.4.0: the Song mode it plays in) */
        sp_cat(", ");
        sp_cat(EN_SMODE_NAME[proj.par[P_SMODE] % EN_NSMODE]);
    }
    if (ui.view == V_CHORDS || ui.view == V_WALK) {
        sp_cat(", walk ");
        sp_cat(EN_WALK_NAME[(proj.par[P_PMODE] & 3) % EN_NWALK]);
    }
    {
        int first = 1;
        for (i = 0; i < 4; i++) {
            const int toff = vknob(ui.view, i) == K_TRANS && fm_knob_off(ui.view, i);
            if (vknob(ui.view, i) == K_NONE || (fm_knob_off(ui.view, i) && !toff))
                continue;
            sp_cat(first ? ": " : ", ");
            first = 0;
            sp_knob(ui.view, i);
            if (toff)
                sp_cat(", only in Set mode");
        }
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_KEY) {             /* (the Key page's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", playing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%, peak ");
        sp_int(plat_cpu_peak());
        sp_cat("%");
        if (plat_usb_power()) {
            sp_cat(", on USB power");
        } else if (plat_battery() >= 0) {
            sp_cat(", battery about ");
            sp_int((uint32_t)batt_pct());
            sp_cat("%");
        }
        if (plat_xruns()) {
            sp_cat(", ");
            sp_int(plat_xruns());
            sp_cat(" dropouts");
        }
    }
}

/* the main screen: the chord and how far to the next, the section, the scale from the root up ("F and F
 * sharp": one letter's two notes joined, so they are heard as two) */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", dub hold");
    } else if (en_drift_bars(proj.par[P_DRIFT])) {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    } else {
        sp_cat(", held");
    }
    sp_cat(". Section ");
    sp_cat(EN_SEC_NAME[en_section]);
    sp_cat(", bar ");
    sp_int((uint32_t)en_phrase_bar + 1u);
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (proj.par[P_ROOT] + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
    if (en_pulse_rest && proj.par[P_DRUMS])
        sp_cat(". Kick out");
}


static void sp_piece_text(void)                /* "PIECE Chamber, A Aeolian, tempo 118" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[proj.par[P_ROOT]]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(vknob(ui.view, i));
    sp.view = ui.view;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        sp_page(1);
        ann_say(sp_buf, 0, ANN_NOW);
        sp_garden();
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "PLAYING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get(vknob(ui.view, i));
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef EN_HOST
/* every page, knob value, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"PLAYING", 0}, {"DUB HOLD", "ON"}, {"DUB HOLD", "OFF"}, {"SAVED", 0},
                                         {"ARRANGEMENT", "ON"}, {"ARRANGEMENT", "OFF"}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest, keep_sec = en_section;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS + EN_NSIR - 1; v++) {   /* (the siren page once a siren: past NVIEWS, the others) */
        ui.view = (uint8_t)(v < NVIEWS ? v : V_SIREN);
        sir_page = (uint8_t)(v < NVIEWS ? 0 : v - NVIEWS + 1);
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = vknob(ui.view, i);
            if (k == K_NONE)
                continue;
            knob_range(k, &lo, &hi);
            for (x = lo; x <= hi; x++) {
                if (k >= K_SPITCH && k <= K_SECHO)
                    proj.sir[sir_page][k - K_SPITCH] = (int8_t)x;
                else if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else if (k == K_SLOT)
                    ui.slot = (uint8_t)x;
                else if (k == K_CQ || k == K_CX)
                    proj.par[(k == K_CQ ? P_CQ0 : P_CX0) + ui.slot] = (int16_t)x;
                else if (k == K_LANE)
                    ui.lane = (uint8_t)x;
                else if (k == K_LCH)
                    proj.par[P_DCH0 + ui.lane] = (int16_t)x;
                else if (k == K_VALUES)
                    proj.values = (uint8_t)x;
                else if (k == K_TRANS)
                    proj.trans = (uint8_t)x;
                else
                    proj.leds = (uint8_t)x;
                sp_n = 0;
                sp_knob(ui.view, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: playing, resting, unsaved, a clock, held, custom */
        ui.view = (uint8_t)(x * 3 % NPAGES);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        en_section = (uint8_t)(x % EN_NSEC);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    sir_page = 0;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    en_section = keep_sec;
    ui.view = keep_view;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = PERF_B0; x < PERF_N; x++) {
        msg_text("SOLO", PERF_NAME[x], t);
        sp_print("MSG", t);
        msg_text(PERF_NAME[x], "SOLO OFF", t);
        sp_print("MSG", t);
        msg_text(PERF_THRNAME[x - PERF_B0], 0, t);
        sp_print("MSG", t);
    }
    msg_text("SOLO", "OFF", t);
    sp_print("MSG", t);
    for (x = 0; x < PERF_NS; x++) {
        msg_text(PERF_SNAME[x], 0, t);
        sp_print("MSG", t);
        msg_text(PERF_SNAME[x], "HELD", t);
        sp_print("MSG", t);
    }
    for (x = 0; x < PERF_B0; x++) {
        msg_text(PERF_NAME[x], x < PERF_NP ? "OFF" : 0, t);
        sp_print("MSG", t);
        if (x < PERF_NP) {
            msg_text(PERF_NAME[x], "ON", t);
            sp_print("MSG", t);
        }
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[MODE_ORDER[x]], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NLAYER; x++) {                    /* OCT- / OCT+: a level, a mute */
        msg_text(LAYER_NAME[x], "55", t);
        sp_print("MSG", t);
        msg_text(LAYER_NAME[x], "MUTED", t);
        sp_print("MSG", t);
        msg_text(LAYER_NAME[x], "ON", t);
        sp_print("MSG", t);
    }
    for (x = 90; x <= 140; x += 25) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 122");
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.cue_from = -1;
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
    if (!en_playing)
        en_play(1);                              /* it plays from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

/* (1.4.0, the user's) Set: a song over, the next piece from its intro as the tails die (6 s on); after the last
 * piece, it rests. Transition (the Song mode page's KNOB 2): LP that; Segue and Mix the engine's, told the next
 * piece's tempo and echo time; at its cue (the outro's last bar, the old parts gone) the next piece set, and the go:
 * it starts on the next downbeat, without a stop */
static void set_tick(void)
{
    static uint32_t end_t;
    static uint8_t cued, end_p, by_hand;
    static int16_t sent = -1;
    const uint32_t now = plat_ms();
    {
        const int nx = proj.piece + 1 < NPIECES && proj.par[P_SMODE] == 2 && proj.trans < EN_NTRANS && proj.trans;
        const int m = nx ? proj.trans : 0, tp = nx ? proj.slot[proj.piece + 1].par[P_TEMPO] : 0,
                  et = nx ? proj.slot[proj.piece + 1].par[P_ETIME] : 0, sig = m | tp << 2 | et << 10;
        if (!en_tr_cue) {
            cued = 0;
            ui.cue_from = -1;
            if (sig != sent) {                   /* (the engine told, as it changes) */
                sent = (int16_t)sig;
                en_trans(m, tp, et);
            }
        } else if (!cued) {                      /* (the cue: the next piece now, under the old one's last bar) */
            cued = 1;
            sent = -1;                           /* (told again once it has started: the piece after) */
            if (nx && en_playing) {
                ui.cue_from = (int8_t)proj.piece;
                set_piece(proj.piece + 1);
                en_trans_go();
                sp_tp.play_t = now;
                card_show("SET", piece_name(proj.piece), K_ACC, 0);
            }
        }
    }
    if (proj.par[P_SMODE] != 2 || en_song_ph != EN_SG_END || en_playing) {
        end_t = 0;
        end_p = proj.piece, by_hand = 0;
        return;
    }
    if (proj.piece != end_p)                     /* (a piece chosen by hand, resting after a song: the set left
                                                  * there, till PLAY) */
        by_hand = 1;
    if (by_hand)
        return;
    if (!end_t) {
        end_t = now | 1u;
    } else if (now - end_t >= 6000u && proj.piece + 1 < NPIECES) {
        end_t = 0;
        set_piece(proj.piece + 1);
        sp_tp.playing = 1;
        sp_tp.play_t = now;
        en_play(1);
        card_show("SET", piece_name(proj.piece), K_ACC, 0);
    }
}

void ui_frame(void)
{
    if (en_queue_drops() != ui.q_drops && !en_queue_left()) {   /* (1.4.1: PRESETS spun fast filled the engine's queue
                                                  * and the piece arrived at lost its last commands, playing with
                                                  * the ones it passed through: all of it again, now there's room) */
        ui.q_drops = en_queue_drops();
        project_apply();
        en_regrow();
        ui.prog_from = en_queue_w();
    }
    prog_take();
    groove_tick();
    input();
    set_tick();
    tape_clock(plat_ms());
    autosave();
    if (grid_up()) {                             /* REC held: the effects, the whole screen */
        if (!(ui.frame % 3u))
            draw_grid();
        draw_main();                             /* (it says the grid to a screen reader) */
    } else {
        draw_header();
        draw_main();
        draw_knobs();
    }
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }

/* the launch screen, while the FM-1 starts, as a delay throw: the name struck (a ring spreads from it),
 * then, every four frames as an echo's time, a repeat thrown out of it to its place along the line, each
 * smaller and fainter, in the layers' colours (the chord's teal, the pings' blue, the bass's brass, the
 * drums' amber), each with its own ring; then the line under it and the version. Frame f of UI_SPLASH_N,
 * drawn whole (two bands of 240 x 120) */
static void splash_band(int f, int32_t oy)
{
    static const felucca_font_t *const F[5] = {&FONT_L, &FONT_L, &FONT_M, &FONT_B, &FONT_S};
    static const int32_t FH[5] = {32, 32, 24, 16, 16}, YC = 70;
    const uint16_t C[5] = {K_TEXT, K_PAD, K_BELL, K_BASS, K_PULSE};
    int32_t w[5], x[5], k;
    for (k = 0; k < 5; k++)
        w[k] = text_w(F[k], "DUB-1");
    x[0] = 120 - w[0] / 2;                        /* the name in the middle; the repeats down the screen */
    cv_begin(240, 120, K_BG);
    for (k = 0; k < 5; k++) {                    /* the rings: one a strike, in its colour */
        int born = 1 + k * 4, age = f - born;
        int32_t r = 12 + age * 8;
        float a = 1.0f - (float)r / 140.0f;
        if (age < 0 || a <= 0.0f)
            continue;
        circle(120, YC - oy, r, mix(K_BG, k ? C[k] : K_PAD, q16(a * (k ? 0.8f : 0.6f))));
    }
    for (k = 4; k >= 1; k--) {                   /* the repeats: thrown out of the name, falling to their places */
        int born = 1 + k * 4, age = f - born;
        int32_t yt = YC + 14 + k * 26, y;
        float t;
        if (age < 0)
            continue;
        t = fm_minf((float)age / 3.0f, 1.0f);
        t = t * (2.0f - t);                      /* (eased) */
        y = YC + (int32_t)((float)(yt - YC) * t);
        cv_text(120 - w[k] / 2, y - FH[k] / 2 - oy, F[k], "DUB-1", mix(K_BG, C[k], (int)(16.0f * t) - 2 * k));
    }
    if (f >= 1)                                  /* the name, struck */
        cv_text(x[0], YC - FH[0] / 2 - oy, F[0], "DUB-1", mix(K_BG, K_TEXT, f >= 3 ? 16 : 6 * f));
    if (f >= 20)
        text_c(120, 204 - oy, &FONT_S, "Dub techno for the FM-1", mix(K_BG, K_ACC, f >= 23 ? 16 : (f - 19) * 4));
    if (f >= 22)
        text_c(120, 224 - oy, &FONT_XS, "DUB-1  " EN_VERSION, K_DIM);   /* (two spaces: FONT_XS's one read "DUB-11.4.0") */
    cv_blit(0, (uint32_t)oy);
}

void ui_splash(int f)
{
    splash_band(f, 0);
    splash_band(f, 120);
}
