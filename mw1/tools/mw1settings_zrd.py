#!/usr/bin/env python3
"""FIXES 126: writes data/mw1settings.zrd, MW1's SETTINGS (src/mw1_settings.h), its 4:3 layout and its _WIDE one.

    python3 tools/mw1settings_zrd.py > data/mw1settings.zrd

The buttons in mw1_settings.cpp's buttons() order (their ids): the five tabs, DEFAULTS, DONE, KEYS, then each of the
nine row slots' three choice segments, two arrows and ten volume cells. The texts: the title, each row's places (L the
label, H a heading, U unavailable, V a list's value, N its count, P a volume's percent), the small fixes' LIST, the
DISPLAY section's NOTE and the status line. Edit the numbers here, not the layout.
"""

TABS = ["GAME", "DISPLAY", "SOUND", "CONTROLS", "ENHANCEMENTS"]
ROWS, SEGS, CELLS = 9, 3, 10
WIDE = 107  # the wide canvas's 853 less 4:3's 640, halved: everything centred


def row_y(r):
    return 98 + r * 32


def looks(base, off=None):
    off = off or base + "_off"
    return f'BITMAP ("{base}") ROLLOVER (BITMAP ("{base}_lit")) ACTIVATE (BITMAP ("{base}_down")) DISABLE (BITMAP ("{off}"))'


def layout(name, dx):
    anchor = ' ANCHOR ("CENTRE")' if dx else ""
    out = [f"{name} (", '  CURSOR (BITMAP ("mw1_cursor") CENTER (1))', "  PRIMITIVES ("]
    out.append(f'    BACKGROUND (POSITION ({-WIDE + dx} 0) BITMAP ("mw1_options_cockpit") ANCHOR ("CENTRE"))')
    out.append(f'    PANEL (POSITION ({196 + dx} 56) BITMAP ("mw1_panel_436x370"){anchor})')
    out.append(f'    PANEL2 (POSITION ({-WIDE + dx} 454) BITMAP ("mw1_statusbar") ANCHOR ("CENTRE"))')
    out += ["  )", "  BUTTONS ("]

    def button(n, x, y, look, label=None, font="mw1_label", off_font="mw1_label_off", off=None):
        lab = ""
        if label:
            lab = f' LABEL_AT ({label[0] + dx} {label[1]}) LABEL_FONT ("{font}") LABEL_OFF_FONT ("{off_font}")'
        out.append(f"    {n} (POSITION ({x + dx} {y}) {looks(look, off)}{lab}{anchor})")

    out.append("    ; The sections down the left: the chosen one is disabled, showing mw1_tab_off (amber, its lamp lit).")
    for i, t in enumerate(TABS):
        y = 56 + i * 40
        button("TAB_" + t, 16, y, "mw1_tab", (100, y + 6), off_font="mw1_label_on")
    button("DEFAULTS", 16, 352, "mw1_button", (100, 358))
    button("DONE", 16, 392, "mw1_button", (100, 398))
    out.append("    ; CONTROLS' KEY BINDINGS, in its row (slot 3).")
    button("KEYS", 356, row_y(3) - 5, "mw1_button", (440, row_y(3) + 1))
    for r in range(ROWS):
        y = row_y(r)
        out.append(f"    ; Row {r}: a choice's values (the chosen one disabled: _off), a list's arrows, a volume's cells.")
        for k in range(SEGS):
            x = 356 + k * 88
            button(f"SEG{r}_{k}", x, y, "mw1_seg", (x + 42, y + 3), "mw1_seg", "mw1_seg_on")
        button(f"LEFT{r}", 356, y + 1, "mw1_lt")
        button(f"RIGHT{r}", 532, y + 1, "mw1_rt")
        for k in range(CELLS):
            button(f"CELL{r}_{k}", 356 + k * 16, y + 1, "mw1_vol")
    out += ["  )", "  TEXTS ("]

    def text(n, x, y, font, gap=None):
        g = f" GAP ({gap})" if gap else ""
        out.append(f'    {n} (POSITION ({x + dx} {y}) FONT ("{font}"){g}{anchor})')

    text("TITLE", 212, 66, "mw1_panel_title")
    for r in range(ROWS):
        y = row_y(r)
        text(f"L{r}", 212, y + 3, "mw1_panel_line")
        text(f"H{r}", 212, y + 6, "mw1_sub")
        text(f"U{r}", 356, y + 3, "mw1_line_off")
        text(f"V{r}", 454, y + 3, "mw1_value_mid")
        text(f"N{r}", 560, y + 5, "mw1_count")
        text(f"P{r}", 524, y + 3, "mw1_value")
    text("LIST", 212, row_y(5), "mw1_cell_text", 13)
    text("NOTE", 212, 402, "mw1_count")
    # The status line from the left, as every MW1 screen's (not centred).
    out.append(f'    STATUS_WORLD (POSITION ({16 + (4 if dx else 0)} 460) FONT ("mw1_status_world"))')
    out.append(f'    STATUS (POSITION ({132 + (4 if dx else 0)} 461) FONT ("mw1_status"))')
    out += ["  )", ")"]
    return out


def main():
    lines = [
        "; MW1: Critical Hit's SETTINGS (FIXES 126, src/mw1_settings.h): one screen in place of Pirate's Moon's option",
        "; pages, opened from the hub's OPTIONS and the pause. A 4:3 layout and a _WIDE one (853 across).",
        "; Written by tools/mw1settings_zrd.py (not by hand); edit the script's numbers, not these.",
        "(",
    ]
    lines += layout("MW1SETTINGS", 0)
    lines += layout("MW1SETTINGS_WIDE", WIDE)
    lines.append(")")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
