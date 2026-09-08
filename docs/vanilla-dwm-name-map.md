# vanilla dwm → chadwm name map

chadwm renamed most of dwm's terse identifiers to `snake_case` / spelled-out names
(commit `d6354ca` "update some names to be more sane"; a few locals in `4aae07b`).
This makes upstream and third-party dwm patches fail to read against this tree.

Use the tables below to translate a patch by hand: look up each identifier a patch
hunk touches, substitute the chadwm name, then apply the hunk manually.

- Commit `d6354ca` is the authoritative rename commit — `git show d6354ca -- dwm.c`.
- Reformatting (`e738f6f`, clang LLVM style) and comments (`befc82e`) are separate
  and do not rename anything.
- The struct/`drw` renames are in this same commit.

---

## Table 1 — global variables & struct fields

| vanilla dwm | chadwm |
|---|---|
| **— file-scope `static`s —** | |
| `stext` | `status_text`  (also grown 256 → 1024) |
| `sw` | `screen_w` |
| `sh` | `screen_h` |
| `bh` | `bar_h` |
| `th` | `tab_h` |
| `lrpad` | `lr_pad` |
| `xerrorxlib` | `orig_xerror_handler` |
| `numlockmask` | `numlock_mask` |
| `handler[]` | `event_handlers[]` |
| `wmatom[]` | `wm_atom[]` |
| `netatom[]` | `net_atom[]` |
| `xatom[]` | `xembed_atom[]` |
| `clrborder` | `border_clr` |
| `dpy` | `display` |
| `mons` | `monitors` |
| `selmon` | `sel_mon` |
| `wmcheckwin` | `wm_check_win` |
| `riodimensions[]` | `rio_dimensions[]` |
| `riopid` | `rio_pid` |
| `hiddenWinStackTop` | `hidden_win_stack_top` |
| `hiddenWinStack[]` | `hidden_win_stack[]` |
| `hiddenWinStackMax` | `HIDDEN_WIN_STACK_MAX` |
| `xcon` | `xcb_conn` |
| _unchanged:_ `broken` `screen` `running` `cursor` `scheme` `drw` `root` | |
| **— `struct Client` —** | |
| `mina` / `maxa` | `min_aspect` / `max_aspect` |
| `oldx` / `oldy` / `oldw` / `oldh` | `old_x` / `old_y` / `old_w` / `old_h` |
| `basew` / `baseh` | `base_w` / `base_h` |
| `incw` / `inch` | `inc_w` / `inc_h` |
| `maxw` / `maxh` | `max_w` / `max_h` |
| `minw` / `minh` | `min_w` / `min_h` |
| `hintsvalid` | `hints_valid` |
| `bw` | `border_w` |
| `oldbw` | `old_border_w` |
| `isfixed` | `is_fixed` |
| `isfloating` | `is_floating` |
| `isurgent` | `is_urgent` |
| `neverfocus` | `never_focus` |
| `oldstate` | `old_state` |
| `isfullscreen` | `is_fullscreen` |
| `snext` | `stack_next` |
| `iscentered` | `is_centered` |
| `isterminal` | `is_terminal` |
| `noswallow` | `no_swallow` |
| `icw` / `ich` | `icon_w` / `icon_h` |
| `beingmoved` | `being_moved` |
| _unchanged:_ `name` `x` `y` `w` `h` `tags` `next` `mon` `win` `cfact` `pid` `icon` `swallowing` | |
| **— `struct Monitor` —** | |
| `ltsymbol` | `layout_symbol` |
| `by` | `bar_y` |
| `ty` | `tab_y` |
| `mx` / `my` / `mw` / `mh` | `mon_x` / `mon_y` / `mon_w` / `mon_h` |
| `wx` / `wy` / `ww` / `wh` | `win_x` / `win_y` / `win_w` / `win_h` |
| `seltags` | `sel_tags` |
| `sellt` | `sel_layout` |
| `lt[2]` | `layout[2]` |
| `barwin` | `bar_win` |
| `tabwin` | `tab_win` |
| `tagwin` | `tag_win` |
| `previewshow` | `preview_show` |
| `ntabs` | `num_tabs` |
| `colorfultag` | `colorful_tag` |
| _unchanged:_ `mfact` `nmaster` `num` `tagset` `showbar` `topbar` `clients` `sel` `stack` `next` `pertag` `showtab` `toptab` `tab_widths` `tab_btn_w` `tagmap` `gappih` `gappiv` `gappoh` `gappov` `borderpx` | |
| **— `struct Pertag` —** | |
| `curtag` | `cur_tag` |
| `prevtag` | `prev_tag` |
| `sellts[]` | `sel_layouts[]` |
| `ltidxs[]` | `layout_idxs[]` |
| _unchanged:_ `nmasters[]` `mfacts[]` `showbars[]` | |
| **— `drw.h` / `drw.c` —** | |
| `Fnt.dpy` | `Fnt.display` |
| `Drw.dpy` | `Drw.display` |
| `drw_create(Display *dpy, …)` | `drw_create(Display *display, …)` |
| _the rest of the drw public API is unchanged_ | |

### Pervasive parameter renames

Every function signature was expanded too. When a hunk touches `c->…` or `m->…`
inside a function body, rename those the same way:

| vanilla | chadwm |
|---|---|
| `Client *c` | `Client *client` |
| `Monitor *m` | `Monitor *mon` |
| `Display *dpy` (in `xerror*`) | `Display *display` |
| `int m` (in `sendevent`) | `int mask` |
| `int bh` (in `drawstatusbar`) | `int bar_h` |

---

## Table 2 — local variables changed, by function

Only the renames *beyond* the pervasive `c`→`client` / `m`→`mon` above.

| vanilla (function :: local) | chadwm |
|---|---|
| `buttonpress` :: `loop` | `btn` |
| `cleanup` :: `foo` | `noop_layout` |
| `cleanupmon` :: local `m` (search ptr) | `prev` |
| `drawstatusbar` :: param `bh` | `bar_h` |
| `drawstatusbar` :: param `stext` | `status_text` |
| `drawstatusbar` :: `p` | `text_start` |
| `drawbar` :: `sw` | `status_w` |
| `drawbar` :: `occ` | `occupied_tags` |
| `drawbar` :: `urg` | `urgent_tags` |
| `geticonprop` :: `bstp` | `best_icon` |
| `geticonprop` :: `bstp32` | `best_icon32` |
| `geticonprop` :: `bstd` | `best_diff` |
| `geticonprop` :: `d` | `diff` |
| `geticonprop` :: `m` (a `uint32_t`, not a Monitor) | `max_dim` |
| `fake_signal` :: `len_str_sig` | `sig_end` |
| `fake_signal` :: `n` | `type_end` |
| `fake_signal` :: `paramn` | `nmatched` |
| `isdescprocess` :: params `p`, `c` | `ancestor`, `descendant` |
| `recttoclient` :: `r` / `a` / `area` | `best` / `overlap` / `max_overlap` |
| `recttomon` :: `r` / `a` / `area` | `best` / `overlap` / `max_overlap` |
| `switchtag` :: `occ` | `occupied_tags` |
| `updategeom` :: `n` / `nn` | `nmons` / `nscreens` |
| `motionnotify` :: `static Monitor *mon` | `prev_mon` |
| `motionnotify` :: local `m` | `mon` |
| `sendevent` :: param `m` | `mask` |
| `winpid` :: `xcb_…_cookie_t c` | `client` |

Functions with **no** non-pervasive local renames (so a patch hunk in them needs
only the `c`/`m` substitution): `dragcfact`, `dragmfact`, `movemouse`,
`resizemouse`, `placemouse`, `monocle`, `focusstack`, `focuswin`, `manage`,
`setborderpx`, `unmanage`, `drawtab`, `systraytomon`.

---

## Essential workflow patches missing from chadwm

chadwm already carries a large set (pertag/perseltag, vanitygaps + cfact +
togglegaps + smartgaps, tab bar, systray, status2d, swallow, winicon, rio
draw/spawn/resize, deck/bstack/centeredmaster layouts, movestack, shiftview,
focusmaster, focusonclick/netactive, noborder, ewmhtags + `_NET_CLIENT_INFO`,
actual + fake fullscreen, hide/restore stack, setborderpx, placemouse/moveorplace,
tag preview, cyclelayout, attach-on-end, dwmc/fsignal IPC).

Still missing, ranked by daily impact:

| # | patch | why it matters daily | port size |
|---|---|---|---|
| 1 | **restartsig** | reload a recompiled binary / config without logging out; `_NET_CLIENT_INFO` already keeps windows on their tags. *(patch drafted: `docs/restartsig.patch`)* | small |
| 2 | **scratchpad** (then multiple / named) | drop-down terminal / notes / calc toggled from any tag | small / medium |
| 3 | **swapfocus / focuslast** | `Mod+Tab` between the two most-recently-focused clients | small |
| 4 | **warp** | pointer follows keyboard focus on focus / view / tag change | small |
| 5 | **sticky** | pin one window (video, chat, reference) visible on every tag | small |
| 6 | **moveresize / aspectresize** | keyboard-driven move & resize of floating windows | medium |
| 7 | **focusurgent** | jump straight to a window that raised its urgency hint | small |
| 8 | **savefloats / exresize** | floating windows remember geometry across float↔tile toggles | small–medium |
| 9 | **winview** | view whichever tag holds the currently selected window | small |
| 10 | **switchtotag** | optionally follow a rule-assigned client to the tag it opened on | small |

### Nice-to-have (missing, not daily-critical)

- **focusdir / focusadjacenttag** — directional focus; mostly pays off with grid layouts.
- **tagallmon / tagswapmon** — multi-monitor bulk tag ops; plain `tagmon` covers the common case.
- **zoomswap** — order-preserving master swap; a refinement of the working `zoom`.
- **keychain** — multi-key chord bindings.
- **autostart** — spawn session programs from within dwm; low value with an external `.xinitrc`.
- **stacker** (absolute stack index) — `movestack` already covers ±1 push.
- **taggrid / tagothermonitor** — niche bar / monitor tag operations.

> Note: chadwm's existing `switchtag()` is the tag-**preview** pixmap-snapshot
> routine, unrelated to the `switchtotag` patch above.
