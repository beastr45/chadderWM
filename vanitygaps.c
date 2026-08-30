/* Settings */
#if !PERTAG_PATCH
static int enablegaps = 1;
#endif // PERTAG_PATCH

void setgaps(int oh, int ov, int ih, int iv) {
  if (oh < 0)
    oh = 0;
  if (ov < 0)
    ov = 0;
  if (ih < 0)
    ih = 0;
  if (iv < 0)
    iv = 0;

  sel_mon->gappoh = oh;
  sel_mon->gappov = ov;
  sel_mon->gappih = ih;
  sel_mon->gappiv = iv;
  arrange(sel_mon);
}

void togglegaps(const Arg *arg) {
#if PERTAG_PATCH
  sel_mon->pertag->enablegaps[sel_mon->pertag->cur_tag] =
      !sel_mon->pertag->enablegaps[sel_mon->pertag->cur_tag];
#else
  enablegaps = !enablegaps;
#endif // PERTAG_PATCH
  arrange(NULL);
}

void defaultgaps(const Arg *arg) { setgaps(gappoh, gappov, gappih, gappiv); }

void incrgaps(const Arg *arg) {
  setgaps(sel_mon->gappoh + arg->i, sel_mon->gappov + arg->i,
          sel_mon->gappih + arg->i, sel_mon->gappiv + arg->i);
}

void incrigaps(const Arg *arg) {
  setgaps(sel_mon->gappoh, sel_mon->gappov, sel_mon->gappih + arg->i,
          sel_mon->gappiv + arg->i);
}

void incrogaps(const Arg *arg) {
  setgaps(sel_mon->gappoh + arg->i, sel_mon->gappov + arg->i, sel_mon->gappih,
          sel_mon->gappiv);
}

void incrohgaps(const Arg *arg) {
  setgaps(sel_mon->gappoh + arg->i, sel_mon->gappov, sel_mon->gappih,
          sel_mon->gappiv);
}

void incrovgaps(const Arg *arg) {
  setgaps(sel_mon->gappoh, sel_mon->gappov + arg->i, sel_mon->gappih,
          sel_mon->gappiv);
}

void incrihgaps(const Arg *arg) {
  setgaps(sel_mon->gappoh, sel_mon->gappov, sel_mon->gappih + arg->i,
          sel_mon->gappiv);
}

void incrivgaps(const Arg *arg) {
  setgaps(sel_mon->gappoh, sel_mon->gappov, sel_mon->gappih,
          sel_mon->gappiv + arg->i);
}

void getgaps(Monitor *mon, int *oh, int *ov, int *ih, int *iv,
             unsigned int *nc) {
  unsigned int n, oe, ie;
#if PERTAG_PATCH
  oe = ie = sel_mon->pertag->enablegaps[sel_mon->pertag->cur_tag];
#else
  oe = ie = enablegaps;
#endif // PERTAG_PATCH
  Client *client;

  for (n = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), n++)
    ;
  if (smartgaps && n == 1) {
    oe = 0; // outer gaps disabled when only one client
  }

  *oh = mon->gappoh * oe; // outer horizontal gap
  *ov = mon->gappov * oe; // outer vertical gap
  *ih = mon->gappih * ie; // inner horizontal gap
  *iv = mon->gappiv * ie; // inner vertical gap
  *nc = n;                // number of clients
}

void getfacts(Monitor *m, int msize, int ssize, float *mf, float *sf, int *mr,
              int *sr) {
  unsigned int n;
  float mfacts = 0, sfacts = 0;
  int mtotal = 0, stotal = 0;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    if (n < m->nmaster)
      mfacts += c->cfact;
    else
      sfacts += c->cfact;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    if (n < m->nmaster)
      mtotal += msize * (c->cfact / mfacts);
    else
      stotal += ssize * (c->cfact / sfacts);

  *mf = mfacts; // total factor of master area
  *sf = sfacts; // total factor of stack area
  *mr = msize -
        mtotal; // the remainder (rest) of pixels after a cfacts master split
  *sr = ssize -
        stotal; // the remainder (rest) of pixels after a cfacts stack split
}

/***
 * Layouts
 */

/*
 * Bottomstack layout + gaps
 * https://dwm.suckless.org/patches/bottomstack/
 */
static void bstack(Monitor *mon) {
  unsigned int i, n;
  int oh, ov, ih, iv;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int sx = 0, sy = 0, sh = 0, sw = 0;
  float mfacts, sfacts;
  int mrest, srest;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  sx = mx = mon->win_x + ov;
  sy = my = mon->win_y + oh;
  sh = mh = mon->win_h - 2 * oh;
  mw = mon->win_w - 2 * ov - iv * (MIN(n, mon->nmaster) - 1);
  sw = mon->win_w - 2 * ov - iv * (n - mon->nmaster - 1);

  if (mon->nmaster && n > mon->nmaster) {
    sh = (mh - ih) * (1 - mon->mfact);
    mh = mh - ih - sh;
    sx = mx;
    sy = my + mh + ih;
  }

  getfacts(mon, mw, sw, &mfacts, &sfacts, &mrest, &srest);

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++) {
    if (i < mon->nmaster) {
      resize(client, mx, my,
             mw * (client->cfact / mfacts) + (i < mrest ? 1 : 0) -
                 (2 * client->border_w),
             mh - (2 * client->border_w), 0);
      mx += WIDTH(client) + iv;
    } else {
      resize(client, sx, sy,
             sw * (client->cfact / sfacts) +
                 ((i - mon->nmaster) < srest ? 1 : 0) - (2 * client->border_w),
             sh - (2 * client->border_w), 0);
      sx += WIDTH(client) + iv;
    }
  }
}

static void bstackhoriz(Monitor *mon) {
  unsigned int i, n;
  int oh, ov, ih, iv;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int sx = 0, sy = 0, sh = 0, sw = 0;
  float mfacts, sfacts;
  int mrest, srest;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  sx = mx = mon->win_x + ov;
  sy = my = mon->win_y + oh;
  mh = mon->win_h - 2 * oh;
  sh = mon->win_h - 2 * oh - ih * (n - mon->nmaster - 1);
  mw = mon->win_w - 2 * ov - iv * (MIN(n, mon->nmaster) - 1);
  sw = mon->win_w - 2 * ov;

  if (mon->nmaster && n > mon->nmaster) {
    sh = (mh - ih) * (1 - mon->mfact);
    mh = mh - ih - sh;
    sy = my + mh + ih;
    sh = mon->win_h - mh - 2 * oh - ih * (n - mon->nmaster);
  }

  getfacts(mon, mw, sh, &mfacts, &sfacts, &mrest, &srest);

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++) {
    if (i < mon->nmaster) {
      resize(client, mx, my,
             mw * (client->cfact / mfacts) + (i < mrest ? 1 : 0) -
                 (2 * client->border_w),
             mh - (2 * client->border_w), 0);
      mx += WIDTH(client) + iv;
    } else {
      resize(client, sx, sy, sw - (2 * client->border_w),
             sh * (client->cfact / sfacts) +
                 ((i - mon->nmaster) < srest ? 1 : 0) - (2 * client->border_w),
             0);
      sy += HEIGHT(client) + ih;
    }
  }
}

/*
 * Centred master layout + gaps
 * https://dwm.suckless.org/patches/centeredmaster/
 */
void centeredmaster(Monitor *mon) {
  unsigned int i, n;
  int oh, ov, ih, iv;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int lx = 0, ly = 0, lw = 0, lh = 0;
  int rx = 0, ry = 0, rw = 0, rh = 0;
  float mfacts = 0, lfacts = 0, rfacts = 0;
  int mtotal = 0, ltotal = 0, rtotal = 0;
  int mrest = 0, lrest = 0, rrest = 0;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  /* initialize areas */
  mx = mon->win_x + ov;
  my = mon->win_y + oh;
  mh = mon->win_h - 2 * oh -
       ih * ((!mon->nmaster ? n : MIN(n, mon->nmaster)) - 1);
  mw = mon->win_w - 2 * ov;
  lh = mon->win_h - 2 * oh - ih * (((n - mon->nmaster) / 2) - 1);
  rh = mon->win_h - 2 * oh -
       ih * (((n - mon->nmaster) / 2) - ((n - mon->nmaster) % 2 ? 0 : 1));

  if (mon->nmaster && n > mon->nmaster) {
    /* go mfact box in the center if more than nmaster clients */
    if (n - mon->nmaster > 1) {
      /* ||<-S->|<---M--->|<-S->|| */
      mw = (mon->win_w - 2 * ov - 2 * iv) * mon->mfact;
      lw = (mon->win_w - mw - 2 * ov - 2 * iv) / 2;
      rw = (mon->win_w - mw - 2 * ov - 2 * iv) - lw;
      mx += lw + iv;
    } else {
      /* ||<---M--->|<-S->|| */
      mw = (mw - iv) * mon->mfact;
      lw = 0;
      rw = mon->win_w - mw - iv - 2 * ov;
    }
    lx = mon->win_x + ov;
    ly = mon->win_y + oh;
    rx = mx + mw + iv;
    ry = mon->win_y + oh;
  }

  /* calculate facts */
  for (n = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), n++) {
    if (!mon->nmaster || n < mon->nmaster)
      mfacts += client->cfact;
    else if ((n - mon->nmaster) % 2)
      lfacts += client->cfact; // total factor of left hand stack area
    else
      rfacts += client->cfact; // total factor of right hand stack area
  }

  for (n = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), n++)
    if (!mon->nmaster || n < mon->nmaster)
      mtotal += mh * (client->cfact / mfacts);
    else if ((n - mon->nmaster) % 2)
      ltotal += lh * (client->cfact / lfacts);
    else
      rtotal += rh * (client->cfact / rfacts);

  mrest = mh - mtotal;
  lrest = lh - ltotal;
  rrest = rh - rtotal;

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++) {
    if (!mon->nmaster || i < mon->nmaster) {
      /* nmaster clients are stacked vertically, in the center of the screen */
      resize(client, mx, my, mw - (2 * client->border_w),
             mh * (client->cfact / mfacts) + (i < mrest ? 1 : 0) -
                 (2 * client->border_w),
             0);
      my += HEIGHT(client) + ih;
    } else {
      /* stack clients are stacked vertically */
      if ((i - mon->nmaster) % 2) {
        resize(client, lx, ly, lw - (2 * client->border_w),
               lh * (client->cfact / lfacts) +
                   ((i - 2 * mon->nmaster) < 2 * lrest ? 1 : 0) -
                   (2 * client->border_w),
               0);
        ly += HEIGHT(client) + ih;
      } else {
        resize(client, rx, ry, rw - (2 * client->border_w),
               rh * (client->cfact / rfacts) +
                   ((i - 2 * mon->nmaster) < 2 * rrest ? 1 : 0) -
                   (2 * client->border_w),
               0);
        ry += HEIGHT(client) + ih;
      }
    }
  }
}

void centeredfloatingmaster(Monitor *mon) {
  unsigned int i, n;
  float mfacts, sfacts;
  float mivf = 1.0; // master inner vertical gap factor
  int oh, ov, ih, iv, mrest, srest;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int sx = 0, sy = 0, sh = 0, sw = 0;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  sx = mx = mon->win_x + ov;
  sy = my = mon->win_y + oh;
  sh = mh = mon->win_h - 2 * oh;
  mw = mon->win_w - 2 * ov - iv * (n - 1);
  sw = mon->win_w - 2 * ov - iv * (n - mon->nmaster - 1);

  if (mon->nmaster && n > mon->nmaster) {
    mivf = 0.8;
    /* go mfact box in the center if more than nmaster clients */
    if (mon->win_w > mon->win_h) {
      mw = mon->win_w * mon->mfact - iv * mivf * (MIN(n, mon->nmaster) - 1);
      mh = mon->win_h * 0.9;
    } else {
      mw = mon->win_w * 0.9 - iv * mivf * (MIN(n, mon->nmaster) - 1);
      mh = mon->win_h * mon->mfact;
    }
    mx = mon->win_x + (mon->win_w - mw) / 2;
    my = mon->win_y + (mon->win_h - mh - 2 * oh) / 2;

    sx = mon->win_x + ov;
    sy = mon->win_y + oh;
    sh = mon->win_h - 2 * oh;
  }

  getfacts(mon, mw, sw, &mfacts, &sfacts, &mrest, &srest);

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++)
    if (i < mon->nmaster) {
      /* nmaster clients are stacked horizontally, in the center of the screen
       */
      resize(client, mx, my,
             mw * (client->cfact / mfacts) + (i < mrest ? 1 : 0) -
                 (2 * client->border_w),
             mh - (2 * client->border_w), 0);
      mx += WIDTH(client) + iv * mivf;
    } else {
      /* stack clients are stacked horizontally */
      resize(client, sx, sy,
             sw * (client->cfact / sfacts) +
                 ((i - mon->nmaster) < srest ? 1 : 0) - (2 * client->border_w),
             sh - (2 * client->border_w), 0);
      sx += WIDTH(client) + iv;
    }
}

/*
 * Deck layout + gaps
 * https://dwm.suckless.org/patches/deck/
 */
void deck(Monitor *mon) {
  unsigned int i, n;
  int oh, ov, ih, iv;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int sx = 0, sy = 0, sh = 0, sw = 0;
  float mfacts, sfacts;
  int mrest, srest;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  sx = mx = mon->win_x + ov;
  sy = my = mon->win_y + oh;
  sh = mh = mon->win_h - 2 * oh - ih * (MIN(n, mon->nmaster) - 1);
  sw = mw = mon->win_w - 2 * ov;

  if (mon->nmaster && n > mon->nmaster) {
    sw = (mw - iv) * (1 - mon->mfact);
    mw = mw - iv - sw;
    sx = mx + mw + iv;
    sh = mon->win_h - 2 * oh;
  }

  getfacts(mon, mh, sh, &mfacts, &sfacts, &mrest, &srest);

  if (n - mon->nmaster > 0) /* override layout symbol */
    snprintf(mon->layout_symbol, sizeof mon->layout_symbol, "D %d",
             n - mon->nmaster);

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++)
    if (i < mon->nmaster) {
      resize(client, mx, my, mw - (2 * client->border_w),
             mh * (client->cfact / mfacts) + (i < mrest ? 1 : 0) -
                 (2 * client->border_w),
             0);
      my += HEIGHT(client) + ih;
    } else {
      resize(client, sx, sy, sw - (2 * client->border_w),
             sh - (2 * client->border_w), 0);
    }
}

/*
 * Fibonacci layout + gaps
 * https://dwm.suckless.org/patches/fibonacci/
 */
void fibonacci(Monitor *mon, int s) {
  unsigned int i, n;
  int nx, ny, nw, nh;
  int oh, ov, ih, iv;
  int nv, hrest = 0, wrest = 0, r = 1;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  nx = mon->win_x + ov;
  ny = mon->win_y + oh;
  nw = mon->win_w - 2 * ov;
  nh = mon->win_h - 2 * oh;

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next)) {
    if (r) {
      if ((i % 2 && (nh - ih) / 2 <= (bar_h + 2 * client->border_w)) ||
          (!(i % 2) && (nw - iv) / 2 <= (bar_h + 2 * client->border_w))) {
        r = 0;
      }
      if (r && i < n - 1) {
        if (i % 2) {
          nv = (nh - ih) / 2;
          hrest = nh - 2 * nv - ih;
          nh = nv;
        } else {
          nv = (nw - iv) / 2;
          wrest = nw - 2 * nv - iv;
          nw = nv;
        }

        if ((i % 4) == 2 && !s)
          nx += nw + iv;
        else if ((i % 4) == 3 && !s)
          ny += nh + ih;
      }

      if ((i % 4) == 0) {
        if (s) {
          ny += nh + ih;
          nh += hrest;
        } else {
          nh -= hrest;
          ny -= nh + ih;
        }
      } else if ((i % 4) == 1) {
        nx += nw + iv;
        nw += wrest;
      } else if ((i % 4) == 2) {
        ny += nh + ih;
        nh += hrest;
        if (i < n - 1)
          nw += wrest;
      } else if ((i % 4) == 3) {
        if (s) {
          nx += nw + iv;
          nw -= wrest;
        } else {
          nw -= wrest;
          nx -= nw + iv;
          nh += hrest;
        }
      }
      if (i == 0) {
        if (n != 1) {
          nw = (mon->win_w - iv - 2 * ov) -
               (mon->win_w - iv - 2 * ov) * (1 - mon->mfact);
          wrest = 0;
        }
        ny = mon->win_y + oh;
      } else if (i == 1)
        nw = mon->win_w - nw - iv - 2 * ov;
      i++;
    }

    resize(client, nx, ny, nw - (2 * client->border_w),
           nh - (2 * client->border_w), False);
  }
}

void dwindle(Monitor *m) { fibonacci(m, 1); }

void spiral(Monitor *m) { fibonacci(m, 0); }

/*
 * Gappless grid layout + gaps (ironically)
 * https://dwm.suckless.org/patches/gaplessgrid/
 */
void gaplessgrid(Monitor *mon) {
  unsigned int i, n;
  int x, y, cols, rows, ch, cw, cn, rn, rrest, crest; // counters
  int oh, ov, ih, iv;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  /* grid dimensions */
  for (cols = 0; cols <= n / 2; cols++)
    if (cols * cols >= n)
      break;
  if (n ==
      5) /* set layout against the general calculation: not 1:2:2, but 2:3 */
    cols = 2;
  rows = n / cols;
  cn = rn = 0; // reset column no, row no, client count

  ch = (mon->win_h - 2 * oh - ih * (rows - 1)) / rows;
  cw = (mon->win_w - 2 * ov - iv * (cols - 1)) / cols;
  rrest = (mon->win_h - 2 * oh - ih * (rows - 1)) - ch * rows;
  crest = (mon->win_w - 2 * ov - iv * (cols - 1)) - cw * cols;
  x = mon->win_x + ov;
  y = mon->win_y + oh;

  for (i = 0, client = nexttiled(mon->clients); client;
       i++, client = nexttiled(client->next)) {
    if (i / rows + 1 > cols - n % cols) {
      rows = n / cols + 1;
      ch = (mon->win_h - 2 * oh - ih * (rows - 1)) / rows;
      rrest = (mon->win_h - 2 * oh - ih * (rows - 1)) - ch * rows;
    }
    resize(client, x, y + rn * (ch + ih) + MIN(rn, rrest),
           cw + (cn < crest ? 1 : 0) - 2 * client->border_w,
           ch + (rn < rrest ? 1 : 0) - 2 * client->border_w, 0);
    rn++;
    if (rn >= rows) {
      rn = 0;
      x += cw + ih + (cn < crest ? 1 : 0);
      cn++;
    }
  }
}

/*
 * Gridmode layout + gaps
 * https://dwm.suckless.org/patches/gridmode/
 */
void grid(Monitor *mon) {
  unsigned int i, n;
  int cx, cy, cw, ch, cc, cr, chrest, cwrest, cols, rows;
  int oh, ov, ih, iv;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);

  /* grid dimensions */
  for (rows = 0; rows <= n / 2; rows++)
    if (rows * rows >= n)
      break;
  cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;

  /* window geoms (cell height/width) */
  ch = (mon->win_h - 2 * oh - ih * (rows - 1)) / (rows ? rows : 1);
  cw = (mon->win_w - 2 * ov - iv * (cols - 1)) / (cols ? cols : 1);
  chrest = (mon->win_h - 2 * oh - ih * (rows - 1)) - ch * rows;
  cwrest = (mon->win_w - 2 * ov - iv * (cols - 1)) - cw * cols;
  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++) {
    cc = i / rows;
    cr = i % rows;
    cx = mon->win_x + ov + cc * (cw + iv) + MIN(cc, cwrest);
    cy = mon->win_y + oh + cr * (ch + ih) + MIN(cr, chrest);
    resize(client, cx, cy, cw + (cc < cwrest ? 1 : 0) - 2 * client->border_w,
           ch + (cr < chrest ? 1 : 0) - 2 * client->border_w, False);
  }
}

/*
 * Horizontal grid layout + gaps
 * https://dwm.suckless.org/patches/horizgrid/
 */
void horizgrid(Monitor *mon) {
  Client *client;
  unsigned int n, i;
  int oh, ov, ih, iv;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int sx = 0, sy = 0, sh = 0, sw = 0;
  int ntop, nbottom = 1;
  float mfacts = 0, sfacts = 0;
  int mrest, srest, mtotal = 0, stotal = 0;

  /* Count windows */
  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  if (n <= 2)
    ntop = n;
  else {
    ntop = n / 2;
    nbottom = n - ntop;
  }
  sx = mx = mon->win_x + ov;
  sy = my = mon->win_y + oh;
  sh = mh = mon->win_h - 2 * oh;
  sw = mw = mon->win_w - 2 * ov;

  if (n > ntop) {
    sh = (mh - ih) / 2;
    mh = mh - ih - sh;
    sy = my + mh + ih;
    mw = mon->win_w - 2 * ov - iv * (ntop - 1);
    sw = mon->win_w - 2 * ov - iv * (nbottom - 1);
  }

  /* calculate facts */
  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++)
    if (i < ntop)
      mfacts += client->cfact;
    else
      sfacts += client->cfact;

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++)
    if (i < ntop)
      mtotal += mh * (client->cfact / mfacts);
    else
      stotal += sw * (client->cfact / sfacts);

  mrest = mh - mtotal;
  srest = sw - stotal;

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++)
    if (i < ntop) {
      resize(client, mx, my,
             mw * (client->cfact / mfacts) + (i < mrest ? 1 : 0) -
                 (2 * client->border_w),
             mh - (2 * client->border_w), 0);
      mx += WIDTH(client) + iv;
    } else {
      resize(client, sx, sy,
             sw * (client->cfact / sfacts) + ((i - ntop) < srest ? 1 : 0) -
                 (2 * client->border_w),
             sh - (2 * client->border_w), 0);
      sx += WIDTH(client) + iv;
    }
}

/*
 * nrowgrid layout + gaps
 * https://dwm.suckless.org/patches/nrowgrid/
 */
void nrowgrid(Monitor *mon) {
  unsigned int n;
  int ri = 0, ci = 0;                  /* counters */
  int oh, ov, ih, iv;                  /* vanitygap settings */
  unsigned int cx, cy, cw, ch;         /* client geometry */
  unsigned int uw = 0, uh = 0, uc = 0; /* utilization trackers */
  unsigned int cols, rows = mon->nmaster + 1;
  Client *client;

  /* count clients */
  getgaps(mon, &oh, &ov, &ih, &iv, &n);

  /* nothing to do here */
  if (n == 0)
    return;

  /* force 2 clients to always split vertically */
  if (FORCE_VSPLIT && n == 2)
    rows = 1;

  /* never allow empty rows */
  if (n < rows)
    rows = n;

  /* define first row */
  cols = n / rows;
  uc = cols;
  cy = mon->win_y + oh;
  ch = (mon->win_h - 2 * oh - ih * (rows - 1)) / rows;
  uh = ch;

  for (client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), ci++) {
    if (ci == cols) {
      uw = 0;
      ci = 0;
      ri++;

      /* next row */
      cols = (n - uc) / (rows - ri);
      uc += cols;
      cy = mon->win_y + oh + uh + ih;
      uh += ch + ih;
    }

    cx = mon->win_x + ov + uw;
    cw = (mon->win_w - 2 * ov - uw) / (cols - ci);
    uw += cw + iv;

    resize(client, cx, cy, cw - (2 * client->border_w),
           ch - (2 * client->border_w), 0);
  }
}

/*
 * Default tile layout + gaps
 */
static void tile(Monitor *mon) {
  unsigned int i, n;
  int oh, ov, ih, iv;
  int mx = 0, my = 0, mh = 0, mw = 0;
  int sx = 0, sy = 0, sh = 0, sw = 0;
  float mfacts, sfacts;
  int mrest, srest;
  Client *client;

  getgaps(mon, &oh, &ov, &ih, &iv, &n);
  if (n == 0)
    return;

  sx = mx = mon->win_x + ov;
  sy = my = mon->win_y + oh;
  mh = mon->win_h - 2 * oh - ih * (MIN(n, mon->nmaster) - 1);
  sh = mon->win_h - 2 * oh - ih * (n - mon->nmaster - 1);
  sw = mw = mon->win_w - 2 * ov;

  if (mon->nmaster && n > mon->nmaster) {
    sw = (mw - iv) * (1 - mon->mfact);
    mw = mw - iv - sw;
    sx = mx + mw + iv;
  }

  getfacts(mon, mh, sh, &mfacts, &sfacts, &mrest, &srest);

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), i++)
    if (i < mon->nmaster) {
      resize(client, mx, my, mw - (2 * client->border_w),
             mh * (client->cfact / mfacts) + (i < mrest ? 1 : 0) -
                 (2 * client->border_w),
             0);
      my += HEIGHT(client) + ih;
    } else {
      resize(client, sx, sy, sw - (2 * client->border_w),
             sh * (client->cfact / sfacts) +
                 ((i - mon->nmaster) < srest ? 1 : 0) - (2 * client->border_w),
             0);
      sy += HEIGHT(client) + ih;
    }
}
