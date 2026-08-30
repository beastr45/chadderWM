void movestack(const Arg *arg) {
  // check to avoid crash
  if (!sel_mon->sel || !sel_mon->clients)
    return;

  Client *client = NULL, *p = NULL, *pc = NULL, *i;

  if (arg->i > 0) {
    /* find the client after sel_mon->sel */
    for (client = sel_mon->sel->next;
         client && (!ISVISIBLE(client) || client->is_floating);
         client = client->next)
      ;
    if (!client)
      for (client = sel_mon->clients;
           client && (!ISVISIBLE(client) || client->is_floating);
           client = client->next)
        ;

  } else {
    /* find the client before sel_mon->sel */
    for (i = sel_mon->clients; i != sel_mon->sel; i = i->next)
      if (ISVISIBLE(i) && !i->is_floating)
        client = i;
    if (!client)
      for (; i; i = i->next)
        if (ISVISIBLE(i) && !i->is_floating)
          client = i;
  }
  /* find the client before sel_mon->sel and client */
  for (i = sel_mon->clients; i && (!p || !pc); i = i->next) {
    if (i->next == sel_mon->sel)
      p = i;
    if (i->next == client)
      pc = i;
  }

  /* swap client and sel_mon->sel sel_mon->clients in the sel_mon->clients list
   */
  if (client && client != sel_mon->sel) {
    Client *temp =
        sel_mon->sel->next == client ? sel_mon->sel : sel_mon->sel->next;
    sel_mon->sel->next = client->next == sel_mon->sel ? client : client->next;
    client->next = temp;

    if (p && p != client)
      p->next = client;
    if (pc && pc != sel_mon->sel)
      pc->next = sel_mon->sel;

    if (sel_mon->sel == sel_mon->clients)
      sel_mon->clients = client;
    else if (client == sel_mon->clients)
      sel_mon->clients = sel_mon->sel;

    arrange(sel_mon);
  }
}
