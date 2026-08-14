# Multi-monitor crash / "everything lands on monitor 2" bug

## Summary

`manage()` in `dwm.c` contains a block that is **not part of upstream dwm**
(vanilla dwm 6.4 has nothing like it — see the diff below). It re-applies a
window's previous tag/monitor assignment from an X property
(`_NET_CLIENT_INFO`) so that windows land back on the correct tag/monitor
after `chadwm` restarts. The idea is fine (it's a common dwm patch, usually
called "restoresession"), but the implementation reads two `unsigned long`
locals without initializing them and only frees the returned buffer
conditionally on a value that may itself be garbage. That's the whole bug —
both symptoms you saw come from it.

This matters more than it looks like on a quick read because **every
`quit` in this config is actually a restart**: `scripts/run.sh` wraps chadwm
in a respawn loop:

```sh
while type dwm >/dev/null; do dwm && continue || break; done
```

`quit()` just sets `running = 0` and returns exit code 0, so the shell loop
immediately relaunches chadwm. On relaunch, `scan()` calls `manage()` for
*every already-mapped window on every monitor in one tight loop* — which is
exactly the code path that hits this bug, and exactly why it shows up as a
multi-monitor problem.

## The buggy code (as committed, `dwm.c`, inside `manage()`)

```c
updatewmhints(c);
{
    int format;
    unsigned long *data, n, extra;        // <-- uninitialized
    Monitor *m;
    Atom atom;
    if (XGetWindowProperty(dpy, c->win, netatom[NetClientInfo], 0L, 2L, False, XA_CARDINAL,
            &atom, &format, &n, &extra, (unsigned char **)&data) == Success && n == 2) {
        c->tags = *data;
        for (m = mons; m; m = m->next) {
            if (m->num == *(data+1)) {
                c->mon = m;
                break;
            }
        }
    }
    if (n > 0)
        XFree(data);                      // <-- reads uninitialized n/data on failure
}
setclienttagprop(c);
```

## What upstream dwm does instead

Upstream dwm 6.4's `manage()` has **no property read-back at all** — it just
picks the monitor from a transient-for hint or `selmon`/`applyrules()` and
moves on:

```c
if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
} else {
    c->mon = selmon;
    applyrules(c);
}
...
updatewmhints(c);
XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
grabbuttons(c, 0);
```

There's no `_NET_CLIENT_INFO`/`NetClientInfo` concept in vanilla dwm at all —
this whole feature was grafted on in the fork, and it's where the divergence
(and the bug) lives.

## Why it crashes

[`XGetWindowProperty(3)`](https://www.x.org/releases/X11R7.7/doc/libX11/libX11/libX11.html#XGetWindowProperty)
only guarantees `nitems_return` (`n` here) and `prop_return` (`data` here)
are set **when it returns `Success`** — including the "property doesn't
exist" case, where it dutifully sets `n = 0`. If the call *doesn't* return
`Success` (e.g. a `BadWindow` from the target window closing in the tiny
race window between the map event and this code running — very plausible
right after a restart when many windows are being (re-)mapped at once),
Xlib is not required to touch `data`, `n`, or `extra` at all. Since they're
plain uninitialized stack locals:

- `if (n > 0) XFree(data)` reads **uninitialized memory**. If the garbage
  happens to look like `n > 0`, dwm calls `XFree()` on a garbage pointer →
  heap corruption → the "random crash" you saw first.
- Conversely, whenever the call *does* succeed with no property present
  (the common case for a genuinely brand-new window), `n` is legitimately
  `0`, so `data` — which Xlib still allocated — is **never freed**. That's
  a slow memory leak on every window you open, independent of the crash.

## Why it then dumps everything on monitor 2

`manage()` runs in a loop from `scan()` for every pre-existing window when
chadwm restarts (which, per the wrapper above, is what `quit` actually
does). The `data`/`n` locals for the `{ ... }` block live in the same stack
slot on every call. If any single iteration's `XGetWindowProperty` call
fails to reach `Success` (per above, plausible under load), `n` and `data`
are simply left holding **whatever the previous call in the loop wrote
there** — because nothing re-initialized them. If an earlier window in the
scan happened to have `n == 2` with monitor index `1` (your second
monitor), a later, unrelated window can spuriously "inherit" that same
`n == 2` / monitor-1 result purely from stack reuse and get reassigned to
monitor 1 — even though its focus, `selmon`, and everything else say
monitor 0. Do this across a whole `scan()` pass and you get exactly what
you described: focus stays on the primary monitor, but window content
piles up on the second one.

Both symptoms — the crash and the misplacement — trace back to the same
two uninitialized variables.

## The fix

Initialize the locals so failure paths are well-defined, and free the
returned buffer whenever Xlib actually allocated one (i.e. whenever the
call succeeded), not just when `n > 0`:

```c
updatewmhints(c);
{
    int format;
    unsigned long *data = NULL, n = 0, extra;
    Monitor *m;
    Atom atom;
    if (XGetWindowProperty(dpy, c->win, netatom[NetClientInfo], 0L, 2L, False, XA_CARDINAL,
            &atom, &format, &n, &extra, (unsigned char **)&data) == Success && n == 2) {
        c->tags = *data;
        for (m = mons; m; m = m->next) {
            if (m->num == *(data+1)) {
                c->mon = m;
                break;
            }
        }
    }
    if (data)
        XFree(data);
}
setclienttagprop(c);
```

Now:
- A failed `XGetWindowProperty` leaves `data == NULL`, `n == 0` — no stale
  values leak in from a previous loop iteration, no garbage `XFree`, no
  spurious monitor reassignment.
- `XFree(data)` runs whenever Xlib actually handed back a buffer (including
  the "property doesn't exist, `n == 0`" case), fixing the per-window leak.

This has been applied in `dwm.c` and the tree builds clean (`make`) with no
new warnings.

## Verifying

1. Rebuild and install (`make` / `sudo make install`, or however you
   normally deploy this config).
2. Open windows across both monitors, then trigger the `quit` keybind (or
   kill/relaunch chadwm) a few times in a row — the scenario that exercises
   `scan()` → `manage()` in a loop. Confirm chadwm doesn't crash and windows
   reopen on the monitor they were actually on.
3. Optionally build with `valgrind` (the tree is currently configured for a
   debug build in `config.mk`: `-g -O0`) to confirm the leaked
   `XGetWindowProperty` buffer is gone.
