void fibonacci(Monitor *mon, int s) {
  unsigned int i, n, nx, ny, nw, nh;
  Client *client;

  for (n = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), n++)
    ;
  if (n == 0)
    return;

  nx = mon->win_x;
  ny = 0;
  nw = mon->win_w;
  nh = mon->win_h;

  for (i = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next)) {
    if ((i % 2 && nh / 2 > 2 * client->border_w) ||
        (!(i % 2) && nw / 2 > 2 * client->border_w)) {
      if (i < n - 1) {
        if (i % 2)
          nh /= 2;
        else
          nw /= 2;
        if ((i % 4) == 2 && !s)
          nx += nw;
        else if ((i % 4) == 3 && !s)
          ny += nh;
      }
      if ((i % 4) == 0) {
        if (s)
          ny += nh;
        else
          ny -= nh;
      } else if ((i % 4) == 1)
        nx += nw;
      else if ((i % 4) == 2)
        ny += nh;
      else if ((i % 4) == 3) {
        if (s)
          nx += nw;
        else
          nx -= nw;
      }
      if (i == 0) {
        if (n != 1)
          nw = mon->win_w * mon->mfact;
        ny = mon->win_y;
      } else if (i == 1)
        nw = mon->win_w - nw;
      i++;
    }
    resize(client, nx, ny, nw - 2 * client->border_w, nh - 2 * client->border_w,
           False);
  }
}

void dwindle(Monitor *mon) { fibonacci(mon, 1); }

void spiral(Monitor *mon) { fibonacci(mon, 0); }
