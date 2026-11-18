# MW1's screen fonts

MW1: Critical Hit's screens (the hub mockup's look, FIXES.md item 3) use three
free faces, unchanged, under the SIL Open Font License 1.1 (each one's licence
is beside it; "Saira" and "Share" are reserved names):

| file | use | source |
| --- | --- | --- |
| SairaCondensed-ExtraBold.ttf | the plaques' big caps, the title | github.com/google/fonts `ofl/sairacondensed` |
| SairaCondensed-Bold.ttf | the tiles' and buttons' labels, panel titles | github.com/google/fonts `ofl/sairacondensed` |
| ShareTechMono-Regular.ttf | the terminal panels and the status line | github.com/google/fonts `ofl/sharetechmono` |
| ShareTech-Regular.ttf | the mech lab's data, lists and small titles (FIXES.md item 17: clean at 12 and 16 px, 1-bit) | github.com/google/fonts `ofl/sharetech` |

`mw1fonts.zrd` (beside this folder) names them for the screens; mw1 adds it
to the 2D host's fonts (`ztwod::Host::add_font_table`, ours), each entry
naming its file and tracking. Package notices: these faces, with their
licences, are to be listed in the package's third-party notices
(THIRD-PARTY.txt) beside the engine's stand-in fonts (assets/fonts).
