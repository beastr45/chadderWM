void tatami(Monitor *mon) {
  unsigned int i, n, nx, ny, nw, nh, mats, tc, tnx, tny, tnw, tnh;
  Client *client;

  for (n = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), ++n)
    ;
  if (n == 0)
    return;

  nx = mon->win_x;
  ny = 0;
  nw = mon->win_w;
  nh = mon->win_h;

  client = nexttiled(mon->clients);

  if (n != 1)
    nw = mon->win_w * mon->mfact;
  ny = mon->win_y;

  resize(client, nx, ny, nw - 2 * client->border_w, nh - 2 * client->border_w,
         False);

  client = nexttiled(client->next);

  nx += nw;
  nw = mon->win_w - nw;

  if (n > 1) {

    tc = n - 1;
    mats = tc / 5;

    nh /= (mats + (tc % 5 > 0));

    for (i = 0; client && (i < (tc % 5)); client = nexttiled(client->next)) {
      tnw = nw;
      tnx = nx;
      tnh = nh;
      tny = ny;
      switch (tc - (mats * 5)) {
      case 1: // fill
        break;
      case 2:             // up and down
        if ((i % 5) == 0) // up
          tnh /= 2;
        else if ((i % 5) == 1) // down
        {
          tnh /= 2;
          tny += nh / 2;
        }
        break;
      case 3:             // bottom, up-left and up-right
        if ((i % 5) == 0) // up-left
        {
          tnw = nw / 2;
          tnh = (2 * nh) / 3;
        } else if ((i % 5) == 1) // up-right
        {
          tnx += nw / 2;
          tnw = nw / 2;
          tnh = (2 * nh) / 3;
        } else if ((i % 5) == 2) // bottom
        {
          tnh = nh / 3;
          tny += (2 * nh) / 3;
        }
        break;
      case 4:             // bottom, left, right and top
        if ((i % 5) == 0) // top
        {
          tnh = (nh) / 4;
        } else if ((i % 5) == 1) // left
        {
          tnw = nw / 2;
          tny += nh / 4;
          tnh = (nh) / 2;
        } else if ((i % 5) == 2) // right
        {
          tnx += nw / 2;
          tnw = nw / 2;
          tny += nh / 4;
          tnh = (nh) / 2;
        } else if ((i % 5) == 3) // bottom
        {
          tny += (3 * nh) / 4;
          tnh = (nh) / 4;
        }
        break;
      }
      ++i;
      resize(client, tnx, tny, tnw - 2 * client->border_w,
             tnh - 2 * client->border_w, False);
    }

    ++mats;

    for (i = 0; client && (mats > 0); client = nexttiled(client->next)) {

      if ((i % 5) == 0) {
        --mats;
        if (((tc % 5) > 0) || (i >= 5))
          ny += nh;
      }

      tnw = nw;
      tnx = nx;
      tnh = nh;
      tny = ny;

      switch (i % 5) {
      case 0: // top-left-vert
        tnw = (nw) / 3;
        tnh = (nh * 2) / 3;
        break;
      case 1: // top-right-hor
        tnx += (nw) / 3;
        tnw = (nw * 2) / 3;
        tnh = (nh) / 3;
        break;
      case 2: // center
        tnx += (nw) / 3;
        tnw = (nw) / 3;
        tny += (nh) / 3;
        tnh = (nh) / 3;
        break;
      case 3: // bottom-right-vert
        tnx += (nw * 2) / 3;
        tnw = (nw) / 3;
        tny += (nh) / 3;
        tnh = (nh * 2) / 3;
        break;
      case 4: //(oldest) bottom-left-hor
        tnw = (2 * nw) / 3;
        tny += (2 * nh) / 3;
        tnh = (nh) / 3;
        break;
      default:
        break;
      }

      ++i;
      // i%=5;
      resize(client, tnx, tny, tnw - 2 * client->border_w,
             tnh - 2 * client->border_w, False);
    }
  }
}
