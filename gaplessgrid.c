void gaplessgrid(Monitor *mon) {
  unsigned int n, cols, rows, cn, rn, i, cx, cy, cw, ch;
  Client *client;

  for (n = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), n++)
    ;
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

  /* window geometries */
  cw = cols ? mon->win_w / cols : mon->win_w;
  cn = 0; /* current column number */
  rn = 0; /* current row number */
  for (i = 0, client = nexttiled(mon->clients); client;
       i++, client = nexttiled(client->next)) {
    if (i / rows + 1 > cols - n % cols)
      rows = n / cols + 1;
    ch = rows ? mon->win_h / rows : mon->win_h;
    cx = mon->win_x + cn * cw;
    cy = mon->win_y + rn * ch;
    resize(client, cx, cy, cw - 2 * client->border_w, ch - 2 * client->border_w,
           False);
    rn++;
    if (rn >= rows) {
      rn = 0;
      cn++;
    }
  }
}
