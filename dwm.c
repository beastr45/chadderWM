/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>
#ifdef __OpenBSD__
#include <kvm.h>
#include <sys/sysctl.h>
#endif /* __OpenBSD */
#include "drw.h"
#include "util.h"
#include <Imlib2.h>

/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                                                        \
  (mask & ~(numlock_mask | LockMask) &                                         \
   (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |      \
    Mod5Mask))
#define INTERSECT(x, y, w, h, mon)                                             \
  (MAX(0,                                                                      \
       MIN((x) + (w), (mon)->win_x + (mon)->win_w) - MAX((x), (mon)->win_x)) * \
   MAX(0,                                                                      \
       MIN((y) + (h), (mon)->win_y + (mon)->win_h) - MAX((y), (mon)->win_y)))
#define INTERSECTC(x, y, w, h, z)                                              \
  (MAX(0, MIN((x) + (w), (z)->x + (z)->w) - MAX((x), (z)->x)) *                \
   MAX(0, MIN((y) + (h), (z)->y + (z)->h) - MAX((y), (z)->y)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->sel_tags]))
#define HIDDEN(C) ((getstate(C->win) == IconicState))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->border_w)
#define HEIGHT(X) ((X)->h + 2 * (X)->border_w)
#define TAGMASK ((1 << LENGTH(tags)) - 1)
#define TAGSLENGTH (LENGTH(tags))
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lr_pad)
#define MAXTABS 50

#define SYSTEM_TRAY_REQUEST_DOCK 0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_FOCUS_IN 4
#define XEMBED_MODALITY_ON 10

#define XEMBED_MAPPED (1 << 0)
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum {
  CurNormal,
  CurResize,
  CurMove,
  CurResizeHorzArrow,
  CurResizeVertArrow,
  CurLast
}; /* cursor */
enum {
  SchemeNorm,
  SchemeSel,
  SchemeTitle,
  SchemeTag,
  SchemeTag1,
  SchemeTag2,
  SchemeTag3,
  SchemeTag4,
  SchemeTag5,
  SchemeLayout,
  TabSel,
  TabNorm,
  SchemeBtnPrev,
  SchemeBtnNext,
  SchemeBtnClose
}; /* color schemes */
enum {
  NetSupported,
  NetWMName,
  NetWMIcon,
  NetWMState,
  NetWMCheck,
  NetSystemTray,
  NetSystemTrayOP,
  NetSystemTrayOrientation,
  NetSystemTrayOrientationHorz,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetClientInfo,
  NetDesktopNames,
  NetDesktopViewport,
  NetNumberOfDesktops,
  NetCurrentDesktop,
  NetLast
}; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum {
  WMProtocols,
  WMDelete,
  WMState,
  WMTakeFocus,
  WMLast
}; /* default atoms */
enum {
  ClkTagBar,
  ClkTabBar,
  ClkTabPrev,
  ClkTabNext,
  ClkTabClose,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkClientWin,
  ClkRootWin,
  ClkLast
}; /* clicks */

enum showtab_modes {
  showtab_never,
  showtab_auto,
  showtab_nmodes,
  showtab_always
}; /* tab modes */

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
  char name[256];
  float min_aspect, max_aspect;
  float cfact;
  int x, y, w, h;
  int old_x, old_y, old_w, old_h;
  int base_w, base_h, inc_w, inc_h, max_w, max_h, min_w, min_h, hints_valid;
  int border_w, old_border_w;
  unsigned int tags;
  int is_fixed, is_centered, is_floating, is_urgent, never_focus, old_state,
      is_fullscreen, is_terminal, no_swallow;
  pid_t pid;
  unsigned int icon_w, icon_h;
  Picture icon;
  int being_moved;
  Client *next;
  Client *stack_next;
  Client *swallowing;
  Monitor *mon;
  Window win;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *sig;
  void (*func)(const Arg *);
} Signal;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  int is_centered;
  int is_floating;
  int is_terminal;
  int no_swallow;
  int monitor;
} Rule;

typedef struct Systray Systray;
struct Systray {
  Window win;
  Client *icons;
};

typedef struct {
  const char **command;
  const char *name;
} Launcher;

/* function declarations */
static void applyrules(Client *client);
static int applysizehints(Client *client, int *x, int *y, int *w, int *h,
                          int interact);
static void arrange(Monitor *mon);
static void arrangemon(Monitor *mon);
static void attach(Client *client);
static void attachstack(Client *client);
static int fake_signal(void);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *client);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void cyclelayout(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *client);
static void detachstack(Client *client);
static Monitor *dirtomon(int dir);
static void dragmfact(const Arg *arg);
static void dragcfact(const Arg *arg);
static void drawbar(Monitor *mon);
static void drawbars(void);
static int drawstatusbar(Monitor *mon, int bar_h, char *text);
static void drawtab(Monitor *mon);
static void drawtabs(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *client);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void focuswin(const Arg *arg);
static Atom getatomprop(Client *client, Atom prop);
static Picture geticonprop(Window w, unsigned int *icw, unsigned int *ich);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth();
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *client, int focused);
static void grabkeys(void);
static void hide(Client *client);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *mon);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static void moveorplace(const Arg *arg);
static Client *nexttiled(Client *client);
static void placemouse(const Arg *arg);
static void pop(Client *client);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static void sighup(int unused);
static void sigterm(int unused);
static Client *recttoclient(int x, int y, int w, int h);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void resize(Client *client, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *mon);
static void resizeclient(Client *client, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *mon);
static int riodraw(Client *client, const char slopstyle[]);
static void rioposition(Client *client, int x, int y, int w, int h);
static void rioresize(const Arg *arg);
static void riospawn(const Arg *arg);
static void run(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2,
                     long d3, long d4);
static void sendmon(Client *client, Monitor *mon);
static void setborderpx(const Arg *arg);
static void setclientstate(Client *client, long state);
static void setclienttagprop(Client *client);
static void setcurrentdesktop(void);
static void setdesktopnames(void);
static void setfocus(Client *client);
static void setfullscreen(Client *client, int fullscreen);
static void setlayout(const Arg *arg);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setnumdesktops(void);
static void setup(void);
static void setviewport(void);
static void seturgent(Client *client, int urg);
static void show(Client *client);
static void showhide(Client *client);
static void showtagpreview(int tag);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static pid_t spawncmd(const Arg *arg);
static void switchtag(void);
static Monitor *systraytomon(Monitor *mon);
static void tabmode(const Arg *arg);
static void tatami(Monitor *mon);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void freeicon(Client *client);
static void hidewin(const Arg *arg);
static void restorewin(const Arg *arg);
static void unfocus(Client *client, int setfocus);
static void unmanage(Client *client, int destroyed);
static void unmapnotify(XEvent *e);
static void updatecurrentdesktop(void);
static void updatebarpos(Monitor *mon);
static void updatebars(void);
static void updatepreview(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *client);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *client);
static void updateicon(Client *client);
static void updatewindowtype(Client *client);
static void updatewmhints(Client *client);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *display, XErrorEvent *ee);
static int xerrordummy(Display *display, XErrorEvent *ee);
static int xerrorstart(Display *display, XErrorEvent *ee);
static void zoom(const Arg *arg);

static void focusmaster(const Arg *arg);

static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t ancestor, pid_t descendant);
static Client *swallowingclient(Window w);
static Client *termforwin(const Client *client);
static pid_t winpid(Window w);

/* variables */
static Systray *systray = NULL;
static const char broken[] = "broken";
static char status_text[1024];
static int screen;
static int screen_w, screen_h;
static int bar_h;
static int tab_h = 0;
static int lr_pad; /* left + right text padding */
static int (*orig_xerror_handler)(Display *, XErrorEvent *);
static unsigned int numlock_mask = 0;
static int rio_dimensions[4] = {-1, -1, -1, -1};
static pid_t rio_pid = 0;
static void (*event_handlers[LASTEvent])(XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [ResizeRequest] = resizerequest,
    [UnmapNotify] = unmapnotify};
static Atom wm_atom[WMLast], net_atom[NetLast], xembed_atom[XLast];
static int running = 1;
static int restart = 0;
static Cur *cursor[CurLast];
static Clr **scheme, border_clr;
static Display *display;
static Drw *drw;
static Monitor *monitors, *sel_mon;
static Window root, wm_check_win;

#define HIDDEN_WIN_STACK_MAX 100
static int hidden_win_stack_top = -1;
static Client *hidden_win_stack[HIDDEN_WIN_STACK_MAX];

static xcb_connection_t *xcb_conn;

/* configuration, allows nested code to access above variables */
#include "config.h"

typedef struct Pertag Pertag;
struct Monitor {
  char layout_symbol[16];
  float mfact;
  int nmaster;
  int num;
  int bar_y;
  int tab_y;
  int mon_x, mon_y, mon_w, mon_h; /* full monitor rectangle */
  int win_x, win_y, win_w, win_h; /* usable area: monitor minus the bar */
  int gappih;                     /* inner gap, horizontal */
  int gappiv;                     /* inner gap, vertical */
  int gappoh;                     /* outer gap, horizontal */
  int gappov;                     /* outer gap, vertical */
  unsigned int borderpx;
  unsigned int sel_tags;   /* index into tagset[] of the shown set */
  unsigned int sel_layout; /* index into layout[] of the active layout */
  unsigned int tagset[2];
  unsigned int colorful_tag;
  int showbar, showtab;
  int topbar, toptab;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window bar_win;
  Window tab_win;
  Window tag_win;
  Pixmap tagmap[LENGTH(tags)];
  int preview_show;
  int num_tabs;
  int tab_widths[MAXTABS];
  int tab_btn_w[3];
  const Layout *layout[2];
  Pertag *pertag;
};

#include "movestack.c"
#include "shiftview.c"
#include "tatami.c"
#include "vanitygaps.c"

struct Pertag {
  unsigned int cur_tag, prev_tag;
  int nmasters[LENGTH(tags) + 1];
  float mfacts[LENGTH(tags) + 1];
  unsigned int sel_layouts[LENGTH(tags) + 1];
  const Layout *layout_idxs[LENGTH(tags) + 1][2];
  int showbars[LENGTH(tags) + 1];
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

/* function implementations */
/* apply the config.h rules to a new client (tags, floating, monitor, ...) */
void applyrules(Client *client) {
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *mon;
  XClassHint ch = {NULL, NULL};

  /* rule matching */
  client->is_centered = 0;
  client->is_floating = 0;
  client->tags = 0;
  XGetClassHint(display, client->win, &ch);
  class = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(client->name, r->title)) &&
        (!r->class || strstr(class, r->class)) &&
        (!r->instance || strstr(instance, r->instance))) {
      client->is_centered = r->is_centered;
      client->is_terminal = r->is_terminal;
      client->no_swallow = r->no_swallow;
      client->is_floating = r->is_floating;
      client->tags |= r->tags;
      for (mon = monitors; mon && mon->num != r->monitor; mon = mon->next)
        ;
      if (mon)
        client->mon = mon;
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
  client->tags = client->tags & TAGMASK
                     ? client->tags & TAGMASK
                     : client->mon->tagset[client->mon->sel_tags];
}

/* clamp a requested geometry to the client's size hints / screen; returns
 * whether it changed */
int applysizehints(Client *client, int *x, int *y, int *w, int *h,
                   int interact) {
  int baseismin;
  Monitor *mon = client->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > screen_w)
      *x = screen_w - WIDTH(client);
    if (*y > screen_h)
      *y = screen_h - HEIGHT(client);
    if (*x + *w + 2 * client->border_w < 0)
      *x = 0;
    if (*y + *h + 2 * client->border_w < 0)
      *y = 0;
  } else {
    if (*x >= mon->win_x + mon->win_w)
      *x = mon->win_x + mon->win_w - WIDTH(client);
    if (*y >= mon->win_y + mon->win_h)
      *y = mon->win_y + mon->win_h - HEIGHT(client);
    if (*x + *w + 2 * client->border_w <= mon->win_x)
      *x = mon->win_x;
    if (*y + *h + 2 * client->border_w <= mon->win_y)
      *y = mon->win_y;
  }
  if (*h < bar_h)
    *h = bar_h;
  if (*w < bar_h)
    *w = bar_h;
  if (resizehints || client->is_floating ||
      !client->mon->layout[client->mon->sel_layout]->arrange) {
    if (!client->hints_valid)
      updatesizehints(client);
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin =
        client->base_w == client->min_w && client->base_h == client->min_h;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= client->base_w;
      *h -= client->base_h;
    }
    /* adjust for aspect limits */
    if (client->min_aspect > 0 && client->max_aspect > 0) {
      if (client->max_aspect < (float)*w / *h)
        *w = *h * client->max_aspect + 0.5;
      else if (client->min_aspect < (float)*h / *w)
        *h = *w * client->min_aspect + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= client->base_w;
      *h -= client->base_h;
    }
    /* adjust for increment value */
    if (client->inc_w)
      *w -= *w % client->inc_w;
    if (client->inc_h)
      *h -= *h % client->inc_h;
    /* restore base dimensions */
    *w = MAX(*w + client->base_w, client->min_w);
    *h = MAX(*h + client->base_h, client->min_h);
    if (client->max_w)
      *w = MIN(*w, client->max_w);
    if (client->max_h)
      *h = MIN(*h, client->max_h);
  }
  return *x != client->x || *y != client->y || *w != client->w ||
         *h != client->h;
}

/* re-tile and restack one monitor, or every monitor when mon is NULL */
void arrange(Monitor *mon) {
  if (mon)
    showhide(mon->stack);
  else
    for (mon = monitors; mon; mon = mon->next)
      showhide(mon->stack);
  if (mon) {
    arrangemon(mon);
    restack(mon);
  } else
    for (mon = monitors; mon; mon = mon->next)
      arrangemon(mon);
}

/* run the monitor's current layout function and reposition its bar */
void arrangemon(Monitor *mon) {
  updatebarpos(mon);
  XMoveResizeWindow(display, mon->tab_win, mon->win_x + mon->gappov, mon->tab_y,
                    mon->win_w - 2 * mon->gappov, tab_h);
  XMoveWindow(display, mon->tag_win, mon->win_x + mon->gappov,
              mon->bar_y +
                  (mon->topbar ? (bar_h + mon->gappoh)
                               : (-(mon->mon_h / scalepreview) - mon->gappoh)));
  strncpy(mon->layout_symbol, mon->layout[mon->sel_layout]->symbol,
          sizeof mon->layout_symbol);
  if (mon->layout[mon->sel_layout]->arrange)
    mon->layout[mon->sel_layout]->arrange(mon);
}

/* link a client into the head (or tail) of its monitor's client list */
void attach(Client *client) {
  if (new_window_attach_on_end) {
    Client **tmp = &client->mon->clients;
    while (*tmp)
      tmp = &(*tmp)->next;
    *tmp = client;
  } else {
    client->next = client->mon->clients;
    client->mon->clients = client;
  }
}

/* push a client onto the top of its monitor's focus stack */
void attachstack(Client *client) {
  client->stack_next = client->mon->stack;
  client->mon->stack = client;
}

/* dispatch mouse clicks on the bar, tab bar, or a client window */
void buttonpress(XEvent *e) {
  unsigned int i, x, click;
  int btn; /* which tab button: 2 = close, 1 = next, 0 = prev (scanned from the
              right) */
  Arg arg = {0};
  Client *client;
  Monitor *mon;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  //  if ((mon = wintomon(ev->window)) && mon != sel_mon) {
  if ((mon = wintomon(ev->window)) && mon != sel_mon &&
      (focusonwheel || (ev->button != Button4 && ev->button != Button5))) {
    unfocus(sel_mon->sel, 1);
    sel_mon = mon;
    focus(NULL);
  }
  if (ev->window == sel_mon->bar_win) {
    if (sel_mon->preview_show) {
      XUnmapWindow(display, sel_mon->tag_win);
      sel_mon->preview_show = 0;
    }
    i = x = 0;
    do
      x += TEXTW(tags[i]);
    while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
      goto execute_handler;
    } else if (ev->x < x + TEXTW(sel_mon->layout_symbol)) {
      click = ClkLtSymbol;
      goto execute_handler;
    }

    x += TEXTW(sel_mon->layout_symbol);

    for (i = 0; i < LENGTH(launchers); i++) {
      x += TEXTW(launchers[i].name);

      if (ev->x < x) {
        Arg a;
        a.v = launchers[i].command;
        spawn(&a);
        return;
      }
    }

    if (ev->x > sel_mon->win_w - (int)TEXTW(status_text))
      click = ClkStatusText;
    else
      click = ClkWinTitle;
  } else if (ev->window == sel_mon->tab_win) {
    i = 0;
    x = 0;
    for (client = sel_mon->clients; client; client = client->next) {
      if (!ISVISIBLE(client))
        continue;
      x += sel_mon->tab_widths[i];
      if (ev->x > x)
        ++i;
      else
        break;
      if (i >= mon->num_tabs)
        break;
    }
    if (client && ev->x <= x) {
      click = ClkTabBar;
      arg.ui = i;
    } else {
      x = sel_mon->win_w - 2 * mon->gappov;
      for (btn = 2; btn >= 0; btn--) {
        x -= sel_mon->tab_btn_w[btn];
        if (ev->x > x)
          break;
      }
      if (ev->x >= x)
        click = ClkTabPrev + btn;
    }
  } else if ((client = wintoclient(ev->window))) {
    //    focus(client);
    restack(sel_mon);
    if (focusonwheel || (ev->button != Button4 && ev->button != Button5))
      focus(client);
    XAllowEvents(display, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }

execute_handler:

  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == ev->button &&
        CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func(
          ((click == ClkTagBar || click == ClkTabBar) && buttons[i].arg.i == 0)
              ? &arg
              : &buttons[i].arg);
}

/* abort at startup if another window manager already owns the root window */
void checkotherwm(void) {
  orig_xerror_handler = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(display, DefaultRootWindow(display), SubstructureRedirectMask);
  XSync(display, False);
  XSetErrorHandler(xerror);
  XSync(display, False);
}

/* unmanage every client and free all resources before exit */
void cleanup(void) {
  Arg a = {.ui = ~0};
  Layout noop_layout = {
      "", NULL}; /* NULL arrange fn: nothing re-tiles during teardown */
  Monitor *mon;
  size_t i;

  view(&a);
  sel_mon->layout[sel_mon->sel_layout] = &noop_layout;
  for (mon = monitors; mon; mon = mon->next)
    while (mon->stack)
      unmanage(mon->stack, 0);
  XUngrabKey(display, AnyKey, AnyModifier, root);
  while (monitors)
    cleanupmon(monitors);
  if (showsystray && systray) {
    XUnmapWindow(display, systray->win);
    XDestroyWindow(display, systray->win);
    free(systray);
    systray = NULL;
  }
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors) + 1; i++)
    free(scheme[i]);
  free(scheme);
  XDestroyWindow(display, wm_check_win);
  drw_free(drw);
  XSync(display, False);
  XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(display, root, net_atom[NetActiveWindow]);
}

/* unlink a monitor and destroy its bar / tab / preview windows */
void cleanupmon(Monitor *mon) {
  Monitor *prev;
  size_t i;

  if (mon == monitors)
    monitors = monitors->next;
  else {
    for (prev = monitors; prev && prev->next != mon; prev = prev->next)
      ;
    prev->next = mon->next;
  }
  for (i = 0; i < LENGTH(tags); i++) {
    if (mon->tagmap[i])
      XFreePixmap(display, mon->tagmap[i]);
  }
  XUnmapWindow(display, mon->bar_win);
  XDestroyWindow(display, mon->bar_win);
  XUnmapWindow(display, mon->tab_win);
  XDestroyWindow(display, mon->tab_win);
  XUnmapWindow(display, mon->tag_win);
  XDestroyWindow(display, mon->tag_win);
  free(mon);
}

/* handle EWMH client messages (fullscreen, activate window, systray docking) */
void clientmessage(XEvent *e) {
  XWindowAttributes wa;
  XSetWindowAttributes swa;
  XClientMessageEvent *cme = &e->xclient;
  Client *client = wintoclient(cme->window);

  unsigned int i;

  if (showsystray && systray && cme->window == systray->win &&
      cme->message_type == net_atom[NetSystemTrayOP]) {
    /* add systray icons */
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      if (!(client = (Client *)calloc(1, sizeof(Client))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Client));
      if (!(client->win = cme->data.l[2])) {
        free(client);
        return;
      }
      client->mon = sel_mon;
      client->next = systray->icons;
      systray->icons = client;
      if (!XGetWindowAttributes(display, client->win, &wa)) {
        /* use sane defaults */
        wa.width = bar_h;
        wa.height = bar_h;
        wa.border_width = 0;
      }
      client->x = client->old_x = client->y = client->old_y = 0;
      client->w = client->old_w = wa.width;
      client->h = client->old_h = wa.height;
      client->old_border_w = wa.border_width;
      client->border_w = 0;
      client->is_floating = True;
      /* reuse tags field as mapped status */
      client->tags = 1;
      updatesizehints(client);
      updatesystrayicongeom(client, wa.width, wa.height);
      XAddToSaveSet(display, client->win);
      XSelectInput(display, client->win,
                   StructureNotifyMask | PropertyChangeMask |
                       ResizeRedirectMask);
      XClassHint ch = {"dwmsystray", "dwmsystray"};
      XSetClassHint(display, client->win, &ch);
      XReparentWindow(display, client->win, systray->win, 0, 0);
      /* use parents background color */
      swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(display, client->win, CWBackPixel, &swa);
      sendevent(client->win, net_atom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_EMBEDDED_NOTIFY, 0, systray->win,
                XEMBED_EMBEDDED_VERSION);
      /* FIXME not sure if I have to send these events, too */
      sendevent(client->win, net_atom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_FOCUS_IN, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(client->win, net_atom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_WINDOW_ACTIVATE, 0, systray->win,
                XEMBED_EMBEDDED_VERSION);
      sendevent(client->win, net_atom[Xembed], StructureNotifyMask, CurrentTime,
                XEMBED_MODALITY_ON, 0, systray->win, XEMBED_EMBEDDED_VERSION);
      XSync(display, False);
      resizebarwin(sel_mon);
      updatesystray();
      setclientstate(client, NormalState);
    }
    return;
  }
  if (!client)
    return;
  if (cme->message_type == net_atom[NetWMState]) {
    if (cme->data.l[1] == net_atom[NetWMFullscreen] ||
        cme->data.l[2] == net_atom[NetWMFullscreen])
      setfullscreen(client,
                    (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                     || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ &&
                         !client->is_fullscreen)));
  } else if (cme->message_type == net_atom[NetActiveWindow]) {
    if (client != sel_mon->sel && !client->is_urgent)
      seturgent(client, 1);
    for (i = 0; i < LENGTH(tags) && !((1 << i) & client->tags); i++)
      ;
    if (i < LENGTH(tags)) {
      const Arg a = {.ui = 1 << i};
      sel_mon = client->mon;
      view(&a);
      focus(client);
      restack(sel_mon);
    }
  }
}

/* send a client a synthetic ConfigureNotify with its current geometry */
void configure(Client *client) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = display;
  ce.event = client->win;
  ce.window = client->win;
  ce.x = client->x;
  ce.y = client->y;
  ce.width = client->w;
  ce.height = client->h;
  ce.border_width = client->border_w;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(display, client->win, False, StructureNotifyMask, (XEvent *)&ce);
}

/* react to root-window size changes by reconfiguring monitors and bars */
void configurenotify(XEvent *e) {
  Monitor *mon;
  Client *client;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty; /* whether the screen resolution actually changed */

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (screen_w != ev->width || screen_h != ev->height);
    screen_w = ev->width;
    screen_h = ev->height;
    if (updategeom() || dirty) {
      system("$HOME/.fehbg &");
      drw_resize(drw, screen_w, bar_h);
      updatebars();
      for (mon = monitors; mon; mon = mon->next) {
        for (client = mon->clients; client; client = client->next)
          if (client->is_fullscreen)
            resizeclient(client, mon->mon_x, mon->mon_y, mon->mon_w,
                         mon->mon_h);
        resizebarwin(mon);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

/* honour or ignore a client's request to change its own geometry */
void configurerequest(XEvent *e) {
  Client *client;
  Monitor *mon;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((client = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      client->border_w = ev->border_width;
    else if (client->is_floating ||
             !sel_mon->layout[sel_mon->sel_layout]->arrange) {
      mon = client->mon;
      if (ev->value_mask & CWX) {
        client->old_x = client->x;
        client->x = mon->mon_x + ev->x;
      }
      if (ev->value_mask & CWY) {
        client->old_y = client->y;
        client->y = mon->mon_y + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        client->old_w = client->w;
        client->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        client->old_h = client->h;
        client->h = ev->height;
      }
      if ((client->x + client->w) > mon->mon_x + mon->mon_w &&
          client->is_floating)
        client->x =
            mon->mon_x +
            (mon->mon_w / 2 - WIDTH(client) / 2); /* center in x direction */
      if ((client->y + client->h) > mon->mon_y + mon->mon_h &&
          client->is_floating)
        client->y =
            mon->mon_y +
            (mon->mon_h / 2 - HEIGHT(client) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX | CWY)) &&
          !(ev->value_mask & (CWWidth | CWHeight)))
        configure(client);
      if (ISVISIBLE(client))
        XMoveResizeWindow(display, client->win, client->x, client->y, client->w,
                          client->h);
    } else
      configure(client);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(display, ev->window, ev->value_mask, &wc);
  }
  XSync(display, False);
}

/* allocate and initialise a Monitor with defaults from config.h */
Monitor *createmon(void) {
  Monitor *mon;
  size_t i;

  mon = ecalloc(1, sizeof(Monitor));
  mon->tagset[0] = mon->tagset[1] = 1;
  mon->mfact = mfact;
  mon->nmaster = nmaster;
  mon->showbar = showbar;
  mon->showtab = showtab;
  mon->topbar = topbar;
  mon->toptab = toptab;
  mon->num_tabs = 0;
  mon->colorful_tag = colorfultag ? colorfultag : 0;
  mon->gappih = gappih;
  mon->gappiv = gappiv;
  mon->gappoh = gappoh;
  mon->gappov = gappov;
  mon->borderpx = borderpx;
  mon->layout[0] = &layouts[0];
  mon->layout[1] = &layouts[1 % LENGTH(layouts)];
  for (i = 0; i < LENGTH(tags); i++)
    mon->tagmap[i] = 0;
  mon->preview_show = 0;
  strncpy(mon->layout_symbol, layouts[0].symbol, sizeof mon->layout_symbol);
  mon->pertag = ecalloc(1, sizeof(Pertag));
  mon->pertag->cur_tag = mon->pertag->prev_tag = 1;

  for (i = 0; i <= LENGTH(tags); i++) {
    mon->pertag->nmasters[i] = mon->nmaster;
    mon->pertag->mfacts[i] = mon->mfact;

    mon->pertag->layout_idxs[i][0] = mon->layout[0];
    mon->pertag->layout_idxs[i][1] = mon->layout[1];
    mon->pertag->sel_layouts[i] = mon->sel_layout;

    mon->pertag->showbars[i] = mon->showbar;
  }

  return mon;
}

/* switch to the next or previous entry in the layouts array */
void cyclelayout(const Arg *arg) {
  Layout *l;
  for (l = (Layout *)layouts; l != sel_mon->layout[sel_mon->sel_layout]; l++)
    ;
  if (arg->i > 0) {
    if (l->symbol && (l + 1)->symbol)
      setlayout(&((Arg){.v = (l + 1)}));
    else
      setlayout(&((Arg){.v = layouts}));
  } else {
    if (l != layouts && (l - 1)->symbol)
      setlayout(&((Arg){.v = (l - 1)}));
    else
      setlayout(&((Arg){.v = &layouts[LENGTH(layouts) - 2]}));
  }
}

/* unmanage a client (or systray icon) whose window was destroyed */
void destroynotify(XEvent *e) {
  Client *client;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((client = wintoclient(ev->window)))
    unmanage(client, 1);
  else if ((client = wintosystrayicon(ev->window))) {
    removesystrayicon(client);
    resizebarwin(sel_mon);
    updatesystray();
  }

  else if ((client = swallowingclient(ev->window)))
    unmanage(client->swallowing, 1);
}

/* remove a client from its monitor's client list */
void detach(Client *client) {
  Client **tc;

  for (tc = &client->mon->clients; *tc && *tc != client; tc = &(*tc)->next)
    ;
  *tc = client->next;
}

/* remove a client from the focus stack, choosing a new selection if needed */
void detachstack(Client *client) {
  Client **tc, *t;

  for (tc = &client->mon->stack; *tc && *tc != client; tc = &(*tc)->stack_next)
    ;
  *tc = client->stack_next;

  if (client == client->mon->sel) {
    for (t = client->mon->stack; t && !ISVISIBLE(t); t = t->stack_next)
      ;
    client->mon->sel = t;
  }
}

/* return the next or previous monitor in the given direction */
Monitor *dirtomon(int dir) {
  Monitor *mon = NULL;

  if (dir > 0) {
    if (!(mon = sel_mon->next))
      mon = monitors;
  } else if (sel_mon == monitors)
    for (mon = monitors; mon->next; mon = mon->next)
      ;
  else
    for (mon = monitors; mon->next != sel_mon; mon = mon->next)
      ;
  return mon;
}

/* render the status text, including ^c/^b/^r/^f markup, into the bar */
int drawstatusbar(Monitor *mon, int bar_h, char *status_text) {
  int ret, i, w, x, len;
  short isCode = 0;
  char *text;
  char *text_start; /* the malloc'd copy; `text` is walked past markup, then
                       restored */

  len = strlen(status_text) + 1;
  if (!(text = (char *)malloc(sizeof(char) * len)))
    die("malloc");
  text_start = text;
  memcpy(text, status_text, len);

  /* compute width of the status text */
  w = 0;
  i = -1;
  while (text[++i]) {
    if (text[i] == '^') {
      if (!isCode) {
        isCode = 1;
        text[i] = '\0';
        w += TEXTW(text) - lr_pad;
        text[i] = '^';
        if (text[++i] == 'f')
          w += atoi(text + ++i);
      } else {
        isCode = 0;
        text = text + i + 1;
        i = -1;
      }
    }
  }
  if (!isCode)
    w += TEXTW(text) - lr_pad;
  else
    isCode = 0;
  text = text_start;

  w += horizpadbar;
  if (floatbar) {
    ret = x = mon->win_w - mon->gappov * 2 - borderpx - w;
    x = mon->win_w - mon->gappov * 2 - borderpx - w - getsystraywidth();
  } else {
    ret = x = mon->win_w - borderpx - w;
    x = mon->win_w - w - getsystraywidth();
  }

  drw_setscheme(drw, scheme[LENGTH(colors)]);
  drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
  drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
  drw_rect(drw, x, borderpx, w, bar_h, 1, 1);
  x += horizpadbar / 2;

  /* process status text */
  i = -1;
  while (text[++i]) {
    if (text[i] == '^' && !isCode) {
      isCode = 1;

      text[i] = '\0';
      w = TEXTW(text) - lr_pad;
      drw_text(drw, x, borderpx + vertpadbar / 2, w, bar_h - vertpadbar, 0,
               text, 0);

      x += w;

      /* process code */
      while (text[++i] != '^') {
        if (text[i] == 'c') {
          char buf[8];
          memcpy(buf, (char *)text + i + 1, 7);
          buf[7] = '\0';
          drw_clr_create(drw, &drw->scheme[ColFg], buf);
          i += 7;
        } else if (text[i] == 'b') {
          char buf[8];
          memcpy(buf, (char *)text + i + 1, 7);
          buf[7] = '\0';
          drw_clr_create(drw, &drw->scheme[ColBg], buf);
          i += 7;
        } else if (text[i] == 'd') {
          drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
          drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
        } else if (text[i] == 'r') {
          int rx = atoi(text + ++i);
          while (text[++i] != ',')
            ;
          int ry = atoi(text + ++i);
          while (text[++i] != ',')
            ;
          int rw = atoi(text + ++i);
          while (text[++i] != ',')
            ;
          int rh = atoi(text + ++i);

          drw_rect(drw, rx + x, ry + borderpx + vertpadbar / 2, rw, rh, 1, 0);
        } else if (text[i] == 'f') {
          x += atoi(text + ++i);
        }
      }

      text = text + i + 1;
      i = -1;
      isCode = 0;
    }
  }

  if (!isCode) {
    w = TEXTW(text) - lr_pad;
    drw_text(drw, x, borderpx + vertpadbar / 2, w, bar_h - vertpadbar, 0, text,
             0);
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  free(text_start);

  return ret;
}

/* resize the selected tiled client's cfact by dragging the mouse */
void dragcfact(const Arg *arg) {
  int prev_x, prev_y, dist_x, dist_y;
  float fact;
  Client *client;
  XEvent ev;
  Time lasttime = 0;

  if (!(client = sel_mon->sel))
    return;
  if (client->is_floating) {
    resizemouse(arg);
    return;
  }
#if !FAKEFULLSCREEN_PATCH
#if FAKEFULLSCREEN_CLIENT_PATCH
  if (client->is_fullscreen &&
      !client->fakefullscreen) /* no support resizing fullscreen windows by
                                  mouse */
    return;
#else
  if (client
          ->is_fullscreen) /* no support resizing fullscreen windows by mouse */
    return;
#endif // FAKEFULLSCREEN_CLIENT_PATCH
#endif // !FAKEFULLSCREEN_PATCH
  restack(sel_mon);

  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CurResize]->cursor,
                   CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(display, None, client->win, 0, 0, 0, 0, client->w / 2,
               client->h / 2);

  prev_x = prev_y = -999999;

  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      event_handlers[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 120))
        continue;
      lasttime = ev.xmotion.time;
      if (prev_x == -999999) {
        prev_x = ev.xmotion.x_root;
        prev_y = ev.xmotion.y_root;
      }

      dist_x = ev.xmotion.x - prev_x;
      dist_y = ev.xmotion.y - prev_y;

      if (abs(dist_x) > abs(dist_y)) {
        fact = (float)4.0 * dist_x / client->mon->win_w;
      } else {
        fact = (float)-4.0 * dist_y / client->mon->win_h;
      }

      if (fact)
        setcfact(&((Arg){.f = fact}));

      prev_x = ev.xmotion.x;
      prev_y = ev.xmotion.y;
      break;
    }
  } while (ev.type != ButtonRelease);

  XWarpPointer(display, None, client->win, 0, 0, 0, 0, client->w / 2,
               client->h / 2);

  XUngrabPointer(display, CurrentTime);
  while (XCheckMaskEvent(display, EnterWindowMask, &ev))
    ;
}

/* adjust the master/stack split (mfact) by dragging the mouse */
void dragmfact(const Arg *arg) {
  unsigned int n;
  int py, px;         // pointer coordinates
  int ax, ay, aw, ah; // area position, width and height
  int center = 0, horizontal = 0, mirror = 0, fixed = 0; // layout configuration
  double fact;
  Monitor *mon;
  XEvent ev;
  Time lasttime = 0;

  mon = sel_mon;

#if VANITYGAPS_PATCH
  int oh, ov, ih, iv;
  getgaps(mon, &oh, &ov, &ih, &iv, &n);
#else
  Client *client;
  for (n = 0, client = nexttiled(mon->clients); client;
       client = nexttiled(client->next), n++)
    ;
#endif // VANITYGAPS_PATCH

  ax = mon->win_x;
  ay = mon->win_y;
  ah = mon->win_h;
  aw = mon->win_w;

  if (!n)
    return;
#if FLEXTILE_DELUXE_LAYOUT
  else if (mon->layout[mon->sel_layout]->arrange == &flextile) {
    int layout = mon->ltaxis[LAYOUT];
    if (layout < 0) {
      mirror = 1;
      layout *= -1;
    }
    if (layout > FLOATING_MASTER) {
      layout -= FLOATING_MASTER;
      fixed = 1;
    }

    if (layout == SPLIT_HORIZONTAL || layout == SPLIT_HORIZONTAL_DUAL_STACK)
      horizontal = 1;
    else if (layout == SPLIT_CENTERED_VERTICAL &&
             (fixed || n - mon->nmaster > 1))
      center = 1;
    else if (layout == FLOATING_MASTER) {
      center = 1;
      if (aw < ah)
        horizontal = 1;
    } else if (layout == SPLIT_CENTERED_HORIZONTAL) {
      if (fixed || n - mon->nmaster > 1)
        center = 1;
      horizontal = 1;
    }
  }
#endif // FLEXTILE_DELUXE_LAYOUT
#if CENTEREDMASTER_LAYOUT
  else if (mon->layout[mon->sel_layout]->arrange == &centeredmaster &&
           (fixed || n - mon->nmaster > 1))
    center = 1;
#endif // CENTEREDMASTER_LAYOUT
#if CENTEREDFLOATINGMASTER_LAYOUT
  else if (mon->layout[mon->sel_layout]->arrange == &centeredfloatingmaster)
    center = 1;
#endif // CENTEREDFLOATINGMASTER_LAYOUT
#if BSTACK_LAYOUT
  else if (mon->layout[mon->sel_layout]->arrange == &bstack)
    horizontal = 1;
#endif // BSTACK_LAYOUT
#if BSTACKHORIZ_LAYOUT
  else if (mon->layout[mon->sel_layout]->arrange == &bstackhoriz)
    horizontal = 1;
#endif // BSTACKHORIZ_LAYOUT

  /* do not allow mfact to be modified under certain conditions */
  if (!mon->layout[mon->sel_layout]->arrange           // floating layout
      || (!fixed && mon->nmaster && n <= mon->nmaster) // no master
#if MONOCLE_LAYOUT
      || mon->layout[mon->sel_layout]->arrange == &monocle
#endif // MONOCLE_LAYOUT
#if GRIDMODE_LAYOUT
      || mon->layout[mon->sel_layout]->arrange == &grid
#endif // GRIDMODE_LAYOUT
#if HORIZGRID_LAYOUT
      || mon->layout[mon->sel_layout]->arrange == &horizgrid
#endif // HORIZGRID_LAYOUT
#if GAPPLESSGRID_LAYOUT
      || mon->layout[mon->sel_layout]->arrange == &gaplessgrid
#endif // GAPPLESSGRID_LAYOUT
#if NROWGRID_LAYOUT
      || mon->layout[mon->sel_layout]->arrange == &nrowgrid
#endif // NROWGRID_LAYOUT
#if FLEXTILE_DELUXE_LAYOUT
      || (mon->layout[mon->sel_layout]->arrange == &flextile &&
          mon->ltaxis[LAYOUT] == NO_SPLIT)
#endif // FLEXTILE_DELUXE_LAYOUT
  )
    return;

#if VANITYGAPS_PATCH
  ay += oh;
  ax += ov;
  aw -= 2 * ov;
  ah -= 2 * oh;
#endif // VANITYGAPS_PATCH

  if (center) {
    if (horizontal) {
      px = ax + aw / 2;
#if VANITYGAPS_PATCH
      py = ay + ah / 2 + (ah - 2 * ih) * (mon->mfact / 2.0) + ih / 2;
#else
      py = ay + ah / 2 + ah * mon->mfact / 2.0;
#endif       // VANITYGAPS_PATCH
    } else { // vertical split
#if VANITYGAPS_PATCH
      px = ax + aw / 2 + (aw - 2 * iv) * mon->mfact / 2.0 + iv / 2;
#else
      px = ax + aw / 2 + aw * mon->mfact / 2.0;
#endif // VANITYGAPS_PATCH
      py = ay + ah / 2;
    }
  } else if (horizontal) {
    px = ax + aw / 2;
    if (mirror)
#if VANITYGAPS_PATCH
      py = ay + (ah - ih) * (1.0 - mon->mfact) + ih / 2;
#else
      py = ay + (ah * (1.0 - mon->mfact));
#endif // VANITYGAPS_PATCH
    else
#if VANITYGAPS_PATCH
      py = ay + ((ah - ih) * mon->mfact) + ih / 2;
#else
      py = ay + (ah * mon->mfact);
#endif     // VANITYGAPS_PATCH
  } else { // vertical split
    if (mirror)
#if VANITYGAPS_PATCH
      px = ax + (aw - iv) * (1.0 - mon->mfact) + iv / 2;
#else
      px = ax + (aw * mon->mfact);
#endif // VANITYGAPS_PATCH
    else
#if VANITYGAPS_PATCH
      px = ax + ((aw - iv) * mon->mfact) + iv / 2;
#else
      px = ax + (aw * mon->mfact);
#endif // VANITYGAPS_PATCH
    py = ay + ah / 2;
  }

  if (XGrabPointer(
          display, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None,
          cursor[horizontal ? CurResizeVertArrow : CurResizeHorzArrow]->cursor,
          CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(display, None, root, 0, 0, 0, 0, px, py);

  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      event_handlers[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 40))
        continue;
      if (lasttime != 0) {
        px = ev.xmotion.x;
        py = ev.xmotion.y;
      }
      lasttime = ev.xmotion.time;

#if VANITYGAPS_PATCH
      if (center)
        if (horizontal)
          if (py - ay > ah / 2)
            fact = (double)1.0 -
                   (ay + ah - py - ih / 2) * 2 / (double)(ah - 2 * ih);
          else
            fact = (double)1.0 - (py - ay - ih / 2) * 2 / (double)(ah - 2 * ih);
        else if (px - ax > aw / 2)
          fact =
              (double)1.0 - (ax + aw - px - iv / 2) * 2 / (double)(aw - 2 * iv);
        else
          fact = (double)1.0 - (px - ax - iv / 2) * 2 / (double)(aw - 2 * iv);
      else if (horizontal)
        fact = (double)(py - ay - ih / 2) / (double)(ah - ih);
      else
        fact = (double)(px - ax - iv / 2) / (double)(aw - iv);
#else
      if (center)
        if (horizontal)
          if (py - ay > ah / 2)
            fact = (double)1.0 - (ay + ah - py) * 2 / (double)ah;
          else
            fact = (double)1.0 - (py - ay) * 2 / (double)ah;
        else if (px - ax > aw / 2)
          fact = (double)1.0 - (ax + aw - px) * 2 / (double)aw;
        else
          fact = (double)1.0 - (px - ax) * 2 / (double)aw;
      else if (horizontal)
        fact = (double)(py - ay) / (double)ah;
      else
        fact = (double)(px - ax) / (double)aw;
#endif // VANITYGAPS_PATCH

      if (!center && mirror)
        fact = 1.0 - fact;

      setmfact(&((Arg){.f = 1.0 + fact}));
      px = ev.xmotion.x;
      py = ev.xmotion.y;
      break;
    }
  } while (ev.type != ButtonRelease);

  XUngrabPointer(display, CurrentTime);
  while (XCheckMaskEvent(display, EnterWindowMask, &ev))
    ;
}

/* draw one monitor's bar: tags, layout symbol, launchers, title, status */
void drawbar(Monitor *mon) {
  int x, y = borderpx, w, status_w = 0, stw = 0; /* stw: system tray width */
  int bh_n = bar_h - borderpx * 2; /* bh_n: bar height, borders excluded */
  int mw;                          /* mw: bar width available to draw in */
  if (floatbar) {
    mw = mon->win_w - mon->gappov * 2 - borderpx * 2;
  } else {
    mw = mon->win_w - borderpx * 2;
  }
  /* boxs/boxw: "box start" offset and "box width" of the floating-window marker
   */
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  unsigned int i, occupied_tags = 0, urgent_tags = 0;
  Client *client;

  XSetForeground(drw->display, drw->gc, border_clr.pixel);
  if (floatbar) {
    XFillRectangle(drw->display, drw->drawable, drw->gc, 0, 0,
                   mon->win_w - mon->gappov * 2, bar_h);
  } else {
    XFillRectangle(drw->display, drw->drawable, drw->gc, 0, 0, mon->win_w,
                   bar_h);
  }

  if (showsystray && mon == systraytomon(mon))
    stw = getsystraywidth();

  if (!mon->showbar)
    return;

  /* draw status first so it can be overdrawn by tags later */
  if (mon == sel_mon) { /* status is only drawn on selected monitor */
    status_w = mw - drawstatusbar(mon, bh_n, status_text);
  }

  resizebarwin(mon);
  for (client = mon->clients; client; client = client->next) {
    occupied_tags |= client->tags;
    if (client->is_urgent)
      urgent_tags |= client->tags;
  }
  x = borderpx;
  for (i = 0; i < LENGTH(tags); i++) {
    w = TEXTW(tags[i]);
    drw_setscheme(drw,
                  scheme[occupied_tags & 1 << i
                             ? (mon->colorful_tag ? tagschemes[i] : SchemeSel)
                             : SchemeTag]);
    drw_text(drw, x, y, w, bh_n, lr_pad / 2, tags[i], urgent_tags & 1 << i);
    if (ulineall ||
        mon->tagset[mon->sel_tags] &
            1 << i) /* if there are conflicts, just move these lines directly
                       underneath both 'drw_setscheme' and 'drw_text' :) */
      drw_rect(drw, x + ulinepad, bh_n - ulinestroke - ulinevoffset,
               w - (ulinepad * 2), ulinestroke, 1, 0);
    /*if (occupied_tags & 1 << i)
      drw_rect(drw, x + boxs, y + boxs, boxw, boxw,
               mon == sel_mon && sel_mon->sel && sel_mon->sel->tags & 1 << i,
               urgent_tags & 1 << i); */
    x += w;
  }
  w = TEXTW(mon->layout_symbol);
  drw_setscheme(drw, scheme[SchemeLayout]);
  x = drw_text(drw, x, 0, w, bar_h, lr_pad / 2, mon->layout_symbol, 0);

  for (i = 0; i < LENGTH(launchers); i++) {
    w = TEXTW(launchers[i].name);
    drw_text(drw, x, 0, w, bar_h, lr_pad / 2, launchers[i].name,
             urgent_tags & 1 << i);
    x += w;
  }

  w = floatbar ? mw + mon->gappov * 2 - status_w - stw - x
               : mw - status_w - stw - x;
  if (w > bh_n) {
    if (mon->sel) {
      drw_setscheme(drw, scheme[mon == sel_mon ? SchemeTitle : SchemeNorm]);
      drw_text(drw, x, 0, w, bar_h,
               lr_pad / 2 +
                   (mon->sel->icon ? mon->sel->icon_w + ICONSPACING : 0),
               mon->sel->name, 0);
      if (mon->sel->icon)
        drw_pic(drw, x + lr_pad / 2, (bar_h - mon->sel->icon_h) / 2,
                mon->sel->icon_w, mon->sel->icon_h, mon->sel->icon);
      if (mon->sel->is_floating)
        drw_rect(drw, x + boxs, boxs, boxw, boxw, mon->sel->is_fixed, 0);
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      if (floatbar) {
        drw_rect(drw, x, y, w - mon->gappov * 2, bh_n, 1, 1);
      } else {
        drw_rect(drw, x, y, w, bh_n, 1, 1);
      }
    }
  }
  drw_map(drw, mon->bar_win, 0, 0, mon->win_w - stw, bar_h);
}

/* premultiply an ARGB pixel's colour channels by its alpha */
static uint32_t prealpha(uint32_t p) {
  uint8_t a = p >> 24u;
  uint32_t rb = (a * (p & 0xFF00FFu)) >> 8u;
  uint32_t g = (a * (p & 0x00FF00u)) >> 8u;
  return (rb & 0xFF00FFu) | (g & 0x00FF00u) | (a << 24u);
}

/* read _NET_WM_ICON and return a scaled Picture of the best-fitting icon */
Picture geticonprop(Window win, unsigned int *picw, unsigned int *pich) {
  int format;
  unsigned long n, extra, *p = NULL;
  Atom real;

  if (XGetWindowProperty(display, win, net_atom[NetWMIcon], 0L, LONG_MAX, False,
                         AnyPropertyType, &real, &format, &n, &extra,
                         (unsigned char **)&p) != Success)
    return None;
  if (n == 0 || format != 32) {
    XFree(p);
    return None;
  }

  /* best_icon points at the pixel data of the closest-sized icon seen so far;
   * best_diff is how far its larger dimension is from ICONSIZE */
  unsigned long *best_icon = NULL;
  uint32_t w, h, sz;
  {
    unsigned long *i;
    const unsigned long *end = p + n;
    uint32_t best_diff = UINT32_MAX, diff, max_dim;
    for (i = p; i < end - 1; i += sz) {
      if ((w = *i++) >= 16384 || (h = *i++) >= 16384) {
        XFree(p);
        return None;
      }
      if ((sz = w * h) > end - i)
        break;
      if ((max_dim = w > h ? w : h) >= ICONSIZE &&
          (diff = max_dim - ICONSIZE) < best_diff) {
        best_diff = diff;
        best_icon = i;
      }
    }
    if (!best_icon) {
      for (i = p; i < end - 1; i += sz) {
        if ((w = *i++) >= 16384 || (h = *i++) >= 16384) {
          XFree(p);
          return None;
        }
        if ((sz = w * h) > end - i)
          break;
        if ((diff = ICONSIZE - (w > h ? w : h)) < best_diff) {
          best_diff = diff;
          best_icon = i;
        }
      }
    }
    if (!best_icon) {
      XFree(p);
      return None;
    }
  }

  if ((w = *(best_icon - 2)) == 0 || (h = *(best_icon - 1)) == 0) {
    XFree(p);
    return None;
  }

  uint32_t icw, ich;
  if (w <= h) {
    ich = ICONSIZE;
    icw = w * ICONSIZE / h;
    if (icw == 0)
      icw = 1;
  } else {
    icw = ICONSIZE;
    ich = h * ICONSIZE / w;
    if (ich == 0)
      ich = 1;
  }
  *picw = icw;
  *pich = ich;

  uint32_t i, *best_icon32 = (uint32_t *)best_icon;
  for (sz = w * h, i = 0; i < sz; ++i)
    best_icon32[i] = prealpha(best_icon[i]);

  Picture ret =
      drw_picture_create_resized(drw, (char *)best_icon, w, h, icw, ich);
  XFree(p);

  return ret;
}

/* redraw the bar on every monitor */
void drawbars(void) {
  Monitor *mon;

  for (mon = monitors; mon; mon = mon->next)
    drawbar(mon);
}

/* parse an "fsignal:" root-window name and run it as a bound action */
int fake_signal(void) {
  char fsignal[256];
  char indicator[9] = "fsignal:";
  char str_sig[50];
  char param[16];
  /* nmatched: number of tokens sscanf matched. sig_end / type_end: byte offsets
   * (via %n) just past the signal name and just past the type token, so
   * (type_end - sig_end) is the length of the type token. */
  int i, sig_end, type_end, nmatched;
  size_t len_fsignal, len_indicator = strlen(indicator);
  Arg arg;

  // Get root name property
  if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
    len_fsignal = strlen(fsignal);

    // Check if this is indeed a fake signal
    if (len_indicator > len_fsignal
            ? 0
            : strncmp(indicator, fsignal, len_indicator) == 0) {
      nmatched = sscanf(fsignal + len_indicator, "%s%n%s%n", str_sig, &sig_end,
                        param, &type_end);

      if (nmatched == 1)
        arg = (Arg){0};
      else if (nmatched > 2)
        return 1;
      else if (strncmp(param, "i", type_end - sig_end) == 0)
        sscanf(fsignal + len_indicator + type_end, "%i", &(arg.i));
      else if (strncmp(param, "ui", type_end - sig_end) == 0)
        sscanf(fsignal + len_indicator + type_end, "%u", &(arg.ui));
      else if (strncmp(param, "f", type_end - sig_end) == 0)
        sscanf(fsignal + len_indicator + type_end, "%f", &(arg.f));
      else
        return 1;

      // Check if a signal was found, and if so handle it
      for (i = 0; i < LENGTH(signals); i++)
        if (strncmp(str_sig, signals[i].sig, sig_end) == 0 && signals[i].func)
          signals[i].func(&(arg));

      // A fake signal was sent
      return 1;
    }
  }

  // No fake signal was sent, so proceed with update
  return 0;
}

/* run slop to select a screen region and store / apply its dimensions */
int riodraw(Client *client, const char slopstyle[]) {
  int i;
  char str[100];
  char strout[100];
  char tmpstring[30] = {0};
  char slopcmd[100] = "slop -f x%xx%yx%wx%hx ";
  int firstchar = 0;
  int counter = 0;

  strcat(slopcmd, slopstyle);
  FILE *fp = popen(slopcmd, "r");

  while (fgets(str, 100, fp) != NULL)
    strcat(strout, str);

  pclose(fp);

  if (strlen(strout) < 6)
    return 0;

  for (i = 0; i < strlen(strout); i++) {
    if (!firstchar) {
      if (strout[i] == 'x')
        firstchar = 1;
      continue;
    }

    if (strout[i] != 'x')
      tmpstring[strlen(tmpstring)] = strout[i];
    else {
      rio_dimensions[counter] = atoi(tmpstring);
      counter++;
      memset(tmpstring, 0, strlen(tmpstring));
    }
  }

  if (rio_dimensions[0] <= -40 || rio_dimensions[1] <= -40 ||
      rio_dimensions[2] <= 50 || rio_dimensions[3] <= 50) {
    rio_dimensions[3] = -1;
    return 0;
  }

  if (client) {
    rioposition(client, rio_dimensions[0], rio_dimensions[1], rio_dimensions[2],
                rio_dimensions[3]);
    return 0;
  }

  return 1;
}

/* float a client and move it onto a slop-selected region */
void rioposition(Client *client, int x, int y, int w, int h) {
  Monitor *mon;
  if ((mon = recttomon(x, y, w, h)) && mon != client->mon) {
    detach(client);
    detachstack(client);
    arrange(client->mon);
    client->mon = mon;
    client->tags = mon->tagset[mon->sel_tags];
    attach(client);
    attachstack(client);
    sel_mon = mon;
    focus(client);
  }

  client->is_floating = 1;
  if (riodraw_borders)
    resizeclient(client, x, y, w - (client->border_w * 2),
                 h - (client->border_w * 2));
  else
    resizeclient(client, x - client->border_w, y - client->border_w, w, h);
  drawbar(client->mon);
  arrange(client->mon);

  rio_dimensions[3] = -1;
  rio_pid = 0;
}

/* drag out an area using slop and resize the selected window to it */
void rioresize(const Arg *arg) {
  Client *client = (arg && arg->v ? (Client *)arg->v : sel_mon->sel);
  if (client)
    riodraw(client, slopresizestyle);
}

/* spawn a new window and drag out an area using slop to postiion it */
void riospawn(const Arg *arg) {
  if (riodraw_spawnasync) {
    rio_pid = spawncmd(arg);
    riodraw(NULL, slopspawnstyle);
  } else if (riodraw(NULL, slopspawnstyle))
    rio_pid = spawncmd(arg);
}

/* have a terminal swallow the window it spawned by swapping X windows */
void swallow(Client *p, Client *client) {

  if (client->no_swallow || client->is_terminal)
    return;
  if (client->no_swallow && !swallowfloating && client->is_floating)
    return;

  detach(client);
  detachstack(client);

  setclientstate(client, WithdrawnState);
  XUnmapWindow(display, p->win);

  p->swallowing = client;
  client->mon = p->mon;

  Window w = p->win;
  p->win = client->win;
  client->win = w;
  updatetitle(p);
  XMoveResizeWindow(display, p->win, p->x, p->y, p->w, p->h);
  arrange(p->mon);
  configure(p);
  updateclientlist();
}

/* restore a swallowed terminal once its child window is gone */
void unswallow(Client *client) {
  client->win = client->swallowing->win;

  free(client->swallowing);
  client->swallowing = NULL;

  /* unfullscreen the client */
  setfullscreen(client, 0);
  updatetitle(client);
  arrange(client->mon);
  XMapWindow(display, client->win);
  XMoveResizeWindow(display, client->win, client->x, client->y, client->w,
                    client->h);
  setclientstate(client, NormalState);
  focus(NULL);
  arrange(client->mon);
}

/* redraw the tab bar on every monitor */
void drawtabs(void) {
  Monitor *mon;

  for (mon = monitors; mon; mon = mon->next)
    drawtab(mon);
}

/* qsort comparator for ints (used to sort tab label widths) */
static int cmpint(const void *p1, const void *p2) {
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference */
  return *((int *)p1) > *(int *)p2;
}

/* draw a monitor's tab bar: one label per visible client plus prev/next/close
 */
void drawtab(Monitor *mon) {
  Client *client;
  int i;
  char *btn_prev = "";
  char *btn_next = "";
  char *btn_close = " ";
  int buttons_w = 0;
  int sorted_label_widths[MAXTABS];
  int tot_width = 0;
  int maxsize = bar_h;
  int x = 0;
  int w = 0;
  int mw = floatbar ? mon->win_w - 2 * mon->gappov : mon->win_w;
  buttons_w += TEXTW(btn_prev) - lr_pad + horizpadtabo;
  buttons_w += TEXTW(btn_next) - lr_pad + horizpadtabo;
  buttons_w += TEXTW(btn_close) - lr_pad + horizpadtabo;
  tot_width = buttons_w;

  /* Calculates number of labels and their width */
  mon->num_tabs = 0;
  for (client = mon->clients; client; client = client->next) {
    if (!ISVISIBLE(client))
      continue;
    mon->tab_widths[mon->num_tabs] =
        MIN(TEXTW(client->name) - lr_pad + horizpadtabi + horizpadtabo, 250);
    tot_width += mon->tab_widths[mon->num_tabs];
    ++mon->num_tabs;
    if (mon->num_tabs >= MAXTABS)
      break;
  }

  if (tot_width >
      mw) { // not enough space to display the labels, they need to be truncated
    memcpy(sorted_label_widths, mon->tab_widths, sizeof(int) * mon->num_tabs);
    qsort(sorted_label_widths, mon->num_tabs, sizeof(int), cmpint);
    for (i = 0; i < mon->num_tabs; ++i) {
      if (tot_width + (mon->num_tabs - i) * sorted_label_widths[i] > mw)
        break;
      tot_width += sorted_label_widths[i];
    }
    maxsize = (mw - tot_width) / (mon->num_tabs - i);
    maxsize = (mon->win_w - tot_width) / (mon->num_tabs - i);
  } else {
    maxsize = mw;
  }
  i = 0;

  /* cleans window */
  drw_setscheme(drw, scheme[TabNorm]);
  drw_rect(drw, 0, 0, mw, tab_h, 1, 1);

  for (client = mon->clients; client; client = client->next) {
    if (!ISVISIBLE(client))
      continue;
    if (i >= mon->num_tabs)
      break;
    if (mon->tab_widths[i] > maxsize)
      mon->tab_widths[i] = maxsize;
    w = mon->tab_widths[i];
    drw_setscheme(drw, scheme[(client == mon->sel) ? TabSel : TabNorm]);
    drw_text(drw, x + horizpadtabo / 2, vertpadbar / 2, w - horizpadtabo,
             tab_h - vertpadbar, horizpadtabi / 2, client->name, 0);
    x += w;
    ++i;
  }

  w = mw - buttons_w - x;
  x += w;
  drw_setscheme(drw, scheme[SchemeBtnPrev]);
  w = TEXTW(btn_prev) - lr_pad + horizpadtabo;
  mon->tab_btn_w[0] = w;
  drw_text(drw, x + horizpadtabo / 2, vertpadbar / 2, w, tab_h - vertpadbar, 0,
           btn_prev, 0);
  x += w;
  drw_setscheme(drw, scheme[SchemeBtnNext]);
  w = TEXTW(btn_next) - lr_pad + horizpadtabo;
  mon->tab_btn_w[1] = w;
  drw_text(drw, x + horizpadtabo / 2, vertpadbar / 2, w, tab_h - vertpadbar, 0,
           btn_next, 0);
  x += w;
  drw_setscheme(drw, scheme[SchemeBtnClose]);
  w = TEXTW(btn_close) - lr_pad + horizpadtabo;
  mon->tab_btn_w[2] = w;
  drw_text(drw, x + horizpadtabo / 2, vertpadbar / 2, w, tab_h - vertpadbar, 0,
           btn_close, 0);
  x += w;

  drw_map(drw, mon->tab_win, 0, 0, mon->win_w, tab_h);
}

/* focus the client or monitor the pointer just entered */
void enternotify(XEvent *e) {
  // return;
  Client *client;
  Monitor *mon;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
      ev->window != root)
    return;
  client = wintoclient(ev->window);
  mon = client ? client->mon : wintomon(ev->window);
  if (mon != sel_mon) {
    unfocus(sel_mon->sel, 1);
    sel_mon = mon;
    focus(client);
  } else if (!client || client == sel_mon->sel)
    return;
  // Disable client focus on mouse pointer as its a bit annoying.
  //  focus(client);
}

/* redraw a bar when its window is exposed */
void expose(XEvent *e) {
  Monitor *mon;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && (mon = wintomon(ev->window))) {
    drawbar(mon);
    if (mon == sel_mon)
      updatesystray();
  }
}

/* give input focus to a client (or the stack top) and update borders and bar */
void focus(Client *client) {
  if (!client || (!ISVISIBLE(client) || HIDDEN(client)))
    for (client = sel_mon->stack;
         client && (!ISVISIBLE(client) || HIDDEN(client));
         client = client->stack_next)
      ;
  if (sel_mon->sel && sel_mon->sel != client)
    unfocus(sel_mon->sel, 0);
  if (client) {
    if (client->mon != sel_mon)
      sel_mon = client->mon;
    if (client->is_urgent)
      seturgent(client, 0);
    detachstack(client);
    attachstack(client);
    grabbuttons(client, 1);
    XSetWindowBorder(display, client->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(client);
  } else {
    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(display, root, net_atom[NetActiveWindow]);
  }
  sel_mon->sel = client;
  drawbars();
  drawtabs();
}

/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;

  if (sel_mon->sel && ev->window != sel_mon->sel->win)
    setfocus(sel_mon->sel);
}

/* move focus to the monitor in the given direction */
void focusmon(const Arg *arg) {
  Monitor *mon;

  if (!monitors->next)
    return;
  if ((mon = dirtomon(arg->i)) == sel_mon)
    return;
  unfocus(sel_mon->sel, 0);
  sel_mon = mon;
  focus(NULL);
}

/* move focus to the next / previous visible client on the current monitor */
void focusstack(const Arg *arg) {
  Client *client = NULL, *i;

  if (!sel_mon->sel || (sel_mon->sel->is_fullscreen && lockfullscreen))
    return;
  if (arg->i > 0) {
    for (client = sel_mon->sel->next;
         client && (!ISVISIBLE(client) || HIDDEN(client));
         client = client->next)
      ;
    if (!client)
      for (client = sel_mon->clients;
           client && (!ISVISIBLE(client) || HIDDEN(client));
           client = client->next)
        ;
  } else {
    for (i = sel_mon->clients; i != sel_mon->sel; i = i->next)
      if (ISVISIBLE(i) && !HIDDEN(i))
        client = i;
    if (!client)
      for (; i; i = i->next)
        if (ISVISIBLE(i) && !HIDDEN(i))
          client = i;
  }
  if (client) {
    focus(client);
    restack(sel_mon);
  }
}

/* focus the nth visible client on the current monitor */
void focuswin(const Arg *arg) {
  int iwin = arg->i;
  Client *client = NULL;
  for (client = sel_mon->clients; client && (iwin || !ISVISIBLE(client));
       client = client->next) {
    if (ISVISIBLE(client))
      --iwin;
  };
  if (client) {
    focus(client);
    restack(sel_mon);
  }
  updatecurrentdesktop();
}

/* return a single Atom-valued window property (with an XEmbed special case) */
Atom getatomprop(Client *client, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;
  /* FIXME getatomprop should return the number of items and a pointer to
   * the stored data instead of this workaround */
  Atom req = XA_ATOM;
  if (prop == xembed_atom[XembedInfo])
    req = xembed_atom[XembedInfo];

  if (XGetWindowProperty(display, client->win, prop, 0L, sizeof atom, False,
                         req, &da, &di, &dl, &dl, &p) == Success &&
      p) {
    atom = *(Atom *)p;
    if (da == xembed_atom[XembedInfo] && dl == 2)
      atom = ((Atom *)p)[1];
    XFree(p);
  }
  return atom;
}

/* get the pointer's current position on the root window */
int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(display, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

/* return a window's WM_STATE value, or -1 if it has none */
long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(display, w, wm_atom[WMState], 0L, 2L, False,
                         wm_atom[WMState], &real, &format, &n, &extra,
                         (unsigned char **)&p) != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

/* total pixel width of the current system tray icons */
unsigned int getsystraywidth() {
  unsigned int w = 0;
  Client *i;
  if (showsystray && systray)
    for (i = systray->icons; i; w += i->w + systrayspacing, i = i->next)
      ;
  return w ? w + systrayspacing : 1;
}

/* copy a window text property (e.g. a title) into a fixed buffer */
int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(display, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(display, &name, &list, &n) >= Success &&
             n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

/* (re)grab the mouse buttons dwm needs on a client window */
void grabbuttons(Client *client, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlock_mask,
                                numlock_mask | LockMask};
    XUngrabButton(display, AnyButton, AnyModifier, client->win);
    if (!focused)
      XGrabButton(display, AnyButton, AnyModifier, client->win, False,
                  BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(display, buttons[i].button,
                      buttons[i].mask | modifiers[j], client->win, False,
                      BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
  }
}

/* (re)grab every configured keybinding on the root window */
void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlock_mask,
                                numlock_mask | LockMask};
    KeyCode code;

    XUngrabKey(display, AnyKey, AnyModifier, root);
    for (i = 0; i < LENGTH(keys); i++)
      if ((code = XKeysymToKeycode(display, keys[i].keysym)))
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(display, code, keys[i].mod | modifiers[j], root, True,
                   GrabModeAsync, GrabModeAsync);
  }
}

/* free a client's cached icon Picture */
void freeicon(Client *client) {
  if (client->icon) {
    XRenderFreePicture(display, client->icon);
    client->icon = None;
  }
  updatecurrentdesktop();
}

/* unmap a client and mark it iconic without unmanaging it */
void hide(Client *client) {
  if (!client || HIDDEN(client))
    return;

  Window w = client->win;
  static XWindowAttributes ra, ca;

  // more or less taken directly from blackbox's hide() function
  XGrabServer(display);
  XGetWindowAttributes(display, root, &ra);
  XGetWindowAttributes(display, w, &ca);
  // prevent UnmapNotify events
  XSelectInput(display, root, ra.your_event_mask & ~SubstructureNotifyMask);
  XSelectInput(display, w, ca.your_event_mask & ~StructureNotifyMask);
  XUnmapWindow(display, w);
  setclientstate(client, IconicState);
  XSelectInput(display, root, ra.your_event_mask);
  XSelectInput(display, w, ca.your_event_mask);
  XUngrabServer(display);

  focus(client->stack_next);
  arrange(client->mon);
}

/* change how many clients occupy the master area for the current tag */
void incnmaster(const Arg *arg) {
  sel_mon->nmaster = sel_mon->pertag->nmasters[sel_mon->pertag->cur_tag] =
      MAX(sel_mon->nmaster + arg->i, 0);
  arrange(sel_mon);
}

#ifdef XINERAMA
/* check whether a Xinerama screen geometry is not already in the list */
static int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                        XineramaScreenInfo *info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org &&
        unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

/* look up a key event in the keys table and run its action */
void keypress(XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XkbKeycodeToKeysym(display, (KeyCode)ev->keycode, 0, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

/* ask the selected client to close, killing it outright if it refuses */
void killclient(const Arg *arg) {
  if (!sel_mon->sel)
    return;
  if (!sendevent(sel_mon->sel->win, wm_atom[WMDelete], NoEventMask,
                 wm_atom[WMDelete], CurrentTime, 0, 0, 0)) {
    XGrabServer(display);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(display, DestroyAll);
    XKillClient(display, sel_mon->sel->win);
    XSync(display, False);
    XSetErrorHandler(xerror);
    XUngrabServer(display);
  }
}

/* start managing a newly mapped window: build its Client, apply rules, attach,
 * map */
void manage(Window w, XWindowAttributes *wa) {
  Client *client, *t = NULL, *term = NULL;
  Window trans = None;
  XWindowChanges wc;

  client = ecalloc(1, sizeof(Client));
  client->win = w;
  client->pid = winpid(w);
  /* geometry */
  client->x = client->old_x = wa->x;
  client->y = client->old_y = wa->y;
  client->w = client->old_w = wa->width;
  client->h = client->old_h = wa->height;
  client->old_border_w = wa->border_width;
  client->cfact = 1.0;

  updateicon(client);
  updatetitle(client);
  if (XGetTransientForHint(display, w, &trans) && (t = wintoclient(trans))) {
    client->mon = t->mon;
    client->tags = t->tags;
  } else {
    client->mon = sel_mon;
    applyrules(client);
    term = termforwin(client);
  }

  if (client->x + WIDTH(client) > client->mon->win_x + client->mon->win_w)
    client->x = client->mon->win_x + client->mon->win_w - WIDTH(client);
  if (client->y + HEIGHT(client) > client->mon->win_y + client->mon->win_h)
    client->y = client->mon->win_y + client->mon->win_h - HEIGHT(client);
  client->x = MAX(client->x, client->mon->win_x);
  client->y = MAX(client->y, client->mon->win_y);
  client->border_w = client->mon->borderpx;

  wc.border_width = client->border_w;
  XConfigureWindow(display, w, CWBorderWidth, &wc);
  XSetWindowBorder(display, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(client); /* propagates border_width, if size doesn't change */
  updatewindowtype(client);
  updatesizehints(client);
  updatewmhints(client);
  {
    int format;
    unsigned long *data = NULL, n = 0, extra;
    Monitor *mon;
    Atom atom;
    if (XGetWindowProperty(display, client->win, net_atom[NetClientInfo], 0L,
                           2L, False, XA_CARDINAL, &atom, &format, &n, &extra,
                           (unsigned char **)&data) == Success &&
        n == 2) {
      client->tags = *data;
      for (mon = monitors; mon; mon = mon->next) {
        if (mon->num == *(data + 1)) {
          client->mon = mon;
          break;
        }
      }
    }
    if (data)
      XFree(data);
  }
  setclienttagprop(client);

  if (client->is_centered) {
    client->x = client->mon->mon_x + (client->mon->mon_w - WIDTH(client)) / 2;
    client->y = client->mon->mon_y + (client->mon->mon_h - HEIGHT(client)) / 2;
  }
  XSelectInput(display, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(client, 0);
  if (!client->is_floating)
    client->is_floating = client->old_state = trans != None || client->is_fixed;
  if (client->is_floating)
    XRaiseWindow(display, client->win);
  attach(client);
  attachstack(client);
  XChangeProperty(display, root, net_atom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(client->win), 1);
  XMoveResizeWindow(display, client->win, client->x + 2 * screen_w, client->y,
                    client->w, client->h); /* some windows require this */
  if (!HIDDEN(client))
    setclientstate(client, NormalState);
  if (client->mon == sel_mon)
    unfocus(sel_mon->sel, 0);
  client->mon->sel = client;

  if (!client->swallowing) {
    if (rio_pid && (!riodraw_matchpid || isdescprocess(rio_pid, client->pid))) {
      if (rio_dimensions[3] != -1)
        rioposition(client, rio_dimensions[0], rio_dimensions[1],
                    rio_dimensions[2], rio_dimensions[3]);
      else {
        killclient(&((Arg){.v = client}));
        return;
      }
    }
  }

  arrange(client->mon);
  if (!HIDDEN(client))
    XMapWindow(display, client->win);
  if (term)
    swallow(term, client);
  focus(NULL);
}

/* re-grab keys after a keyboard mapping change */
void mappingnotify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

/* manage a window that asks to be mapped (or activate a systray icon) */
void maprequest(XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;
  Client *i;
  if ((i = wintosystrayicon(ev->window))) {
    sendevent(i->win, net_atom[Xembed], StructureNotifyMask, CurrentTime,
              XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
    resizebarwin(sel_mon);
    updatesystray();
  }

  if (!XGetWindowAttributes(display, ev->window, &wa) || wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

/* layout: stack every client at full window size */
void monocle(Monitor *mon) {
  unsigned int n = 0;

  Client *client;

  for (client = mon->clients; client; client = client->next)
    if (ISVISIBLE(client))
      n++;

  if (n > 0) /* override layout symbol */
    snprintf(mon->layout_symbol, sizeof mon->layout_symbol, "[%d]", n);

  int newx, newy, neww, newh;

  for (client = nexttiled(mon->clients); client;
       client = nexttiled(client->next)) {
    newx = mon->win_x + mon->gappov - client->border_w;
    newy = mon->win_y + mon->gappoh - client->border_w;
    neww = mon->win_w - 2 * (mon->gappov + client->border_w);
    newh = mon->win_h - 2 * (mon->gappoh + client->border_w);

    applysizehints(client, &newx, &newy, &neww, &newh, 0);

    if (neww < mon->win_w)
      newx = mon->win_x + (mon->win_w - (neww + 2 * client->border_w)) / 2;

    if (newh < mon->win_h)
      newy = mon->win_y + (mon->win_h - (newh + 2 * client->border_w)) / 2;

    resize(client, newx, newy, neww, newh, 0);
  }
}

/* handle pointer motion: tag-preview popups and follow-mouse monitor focus */
void motionnotify(XEvent *e) {
  unsigned int i, x;
  static Monitor *prev_mon = NULL;
  Monitor *mon;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window == sel_mon->bar_win) {
    i = x = 0;
    do
      x += TEXTW(tags[i]);
    while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      if ((i + 1) != sel_mon->preview_show &&
          !(sel_mon->tagset[sel_mon->sel_tags] & 1 << i)) {
        sel_mon->preview_show = i + 1;
        showtagpreview(i);
      } else if (sel_mon->tagset[sel_mon->sel_tags] & 1 << i) {
        sel_mon->preview_show = 0;
        showtagpreview(0);
      }
    } else if (sel_mon->preview_show != 0) {
      sel_mon->preview_show = 0;
      showtagpreview(0);
    }
  } else if (sel_mon->preview_show != 0) {
    sel_mon->preview_show = 0;
    showtagpreview(0);
  }

  if (ev->window != root)
    return;
  if ((mon = recttomon(ev->x_root, ev->y_root, 1, 1)) != prev_mon && prev_mon) {
    unfocus(sel_mon->sel, 1);
    sel_mon = mon;
    focus(NULL);
  }
  prev_mon = mon;
}

/* refresh a client's cached icon from its current _NET_WM_ICON */
void updateicon(Client *client) {
  freeicon(client);
  client->icon = geticonprop(client->win, &client->icon_w, &client->icon_h);
}

/* movemouse when floating, otherwise placemouse into the tiling */
void moveorplace(const Arg *arg) {
  if ((!sel_mon->layout[sel_mon->sel_layout]->arrange ||
       (sel_mon->sel && sel_mon->sel->is_floating)))
    movemouse(arg);
  else
    placemouse(arg);
}

/* drag the selected client with the mouse (floating it if tiled) */
void movemouse(const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *client;
  Monitor *mon;
  XEvent ev;
  Time lasttime = 0;

  if (!(client = sel_mon->sel))
    return;
  if (client->is_fullscreen) /* no support moving fullscreen windows by mouse */
    return;
  restack(sel_mon);
  ocx = client->x;
  ocy = client->y;
  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CurMove]->cursor,
                   CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      event_handlers[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(sel_mon->win_x - nx) < snap)
        nx = sel_mon->win_x;
      else if (abs((sel_mon->win_x + sel_mon->win_w) - (nx + WIDTH(client))) <
               snap)
        nx = sel_mon->win_x + sel_mon->win_w - WIDTH(client);
      if (abs(sel_mon->win_y - ny) < snap)
        ny = sel_mon->win_y;
      else if (abs((sel_mon->win_y + sel_mon->win_h) - (ny + HEIGHT(client))) <
               snap)
        ny = sel_mon->win_y + sel_mon->win_h - HEIGHT(client);
      if (!client->is_floating &&
          sel_mon->layout[sel_mon->sel_layout]->arrange &&
          (abs(nx - client->x) > snap || abs(ny - client->y) > snap))
        togglefloating(NULL);
      if (!sel_mon->layout[sel_mon->sel_layout]->arrange || client->is_floating)
        resize(client, nx, ny, client->w, client->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(display, CurrentTime);
  if ((mon = recttomon(client->x, client->y, client->w, client->h)) !=
      sel_mon) {
    sendmon(client, mon);
    sel_mon = mon;
    focus(NULL);
  }
}

/* return the next visible, non-floating client in a list */
Client *nexttiled(Client *client) {
  for (; client &&
         (client->is_floating || (!ISVISIBLE(client) || HIDDEN(client)));
       client = client->next)
    ;
  return client;
}

/* drag a tiled client and re-insert it into the stack under the pointer */
void placemouse(const Arg *arg) {
  int x, y, px, py, ocx, ocy, nx = -9999, ny = -9999, freemove = 0;
  Client *client, *r = NULL, *at, *prevr;
  Monitor *mon;
  XEvent ev;
  XWindowAttributes wa;
  Time lasttime = 0;
  int attachmode, prevattachmode;
  attachmode = prevattachmode = -1;

  if (!(client = sel_mon->sel) || !client->mon->layout[client->mon->sel_layout]
                                       ->arrange) /* no support for placemouse
                                  when floating layout is used */
    return;
  if (client
          ->is_fullscreen) /* no support placing fullscreen windows by mouse */
    return;
  restack(sel_mon);
  prevr = client;
  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CurMove]->cursor,
                   CurrentTime) != GrabSuccess)
    return;

  client->is_floating = 0;
  client->being_moved = 1;

  if (!XGetWindowAttributes(display, client->win, &wa)) {
    client->being_moved = 0;
    XUngrabPointer(display, CurrentTime);
    return;
  }
  ocx = wa.x;
  ocy = wa.y;

  if (arg->i == 2) // warp cursor to client center
    XWarpPointer(display, None, client->win, 0, 0, 0, 0, WIDTH(client) / 2,
                 HEIGHT(client) / 2);

  if (!getrootptr(&x, &y)) {
    client->being_moved = 0;
    XUngrabPointer(display, CurrentTime);
    return;
  }

  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      event_handlers[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);

      if (!freemove && (abs(nx - ocx) > snap || abs(ny - ocy) > snap))
        freemove = 1;

      if (freemove)
        XMoveWindow(display, client->win, nx, ny);

      if ((mon = recttomon(ev.xmotion.x, ev.xmotion.y, 1, 1)) && mon != sel_mon)
        sel_mon = mon;

      if (arg->i ==
          1) { // tiled position is relative to the client window center point
        px = nx + wa.width / 2;
        py = ny + wa.height / 2;
      } else { // tiled position is relative to the mouse cursor
        px = ev.xmotion.x;
        py = ev.xmotion.y;
      }

      r = recttoclient(px, py, 1, 1);

      if (!r || r == client)
        break;

      attachmode = 0; // below
      if (((float)(r->y + r->h - py) / r->h) >
          ((float)(r->x + r->w - px) / r->w)) {
        if (abs(r->y - py) < r->h / 2)
          attachmode = 1; // above
      } else if (abs(r->x - px) < r->w / 2)
        attachmode = 1; // above

      if ((r && r != prevr) || (attachmode != prevattachmode)) {
        detachstack(client);
        detach(client);
        if (client->mon != r->mon) {
          arrangemon(client->mon);
          client->tags = r->mon->tagset[r->mon->sel_tags];
        }

        client->mon = r->mon;
        r->mon->sel = r;

        if (attachmode) {
          if (r == r->mon->clients)
            attach(client);
          else {
            for (at = r->mon->clients; at->next != r; at = at->next)
              ;
            client->next = at->next;
            at->next = client;
          }
        } else {
          client->next = r->next;
          r->next = client;
        }

        attachstack(client);
        arrangemon(r->mon);
        prevr = r;
        prevattachmode = attachmode;
      }
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(display, CurrentTime);

  if ((mon = recttomon(ev.xmotion.x, ev.xmotion.y, 1, 1)) &&
      mon != client->mon) {
    detach(client);
    detachstack(client);
    arrangemon(client->mon);
    client->mon = mon;
    client->tags = mon->tagset[mon->sel_tags];
    attach(client);
    attachstack(client);
    sel_mon = mon;
  }

  focus(client);
  client->being_moved = 0;

  if (nx != -9999)
    resize(client, nx, ny, client->w, client->h, 0);
  arrangemon(client->mon);
}

/* move a client to the top of the master area and focus it */
void pop(Client *client) {
  detach(client);
  attach(client);
  focus(client);
  arrange(client->mon);
}

/* react to window property changes (title, hints, icon, root status text) */
void propertynotify(XEvent *e) {
  Client *client;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((client = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS) {
      updatesizehints(client);
      updatesystrayicongeom(client, client->w, client->h);
    } else
      updatesystrayiconstate(client, ev);
    resizebarwin(sel_mon);
    updatesystray();
  }
  // if ((ev->window == root) && (ev->atom == XA_WM_NAME))
  //   updatestatus();
  if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
    if (!fake_signal())
      updatestatus();
  } else if (ev->state == PropertyDelete)
    return; /* ignore */
  else if ((client = wintoclient(ev->window))) {
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!client->is_floating &&
          (XGetTransientForHint(display, client->win, &trans)) &&
          (client->is_floating = (wintoclient(trans)) != NULL))
        arrange(client->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      client->hints_valid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(client);
      drawbars();
      drawtabs();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == net_atom[NetWMName]) {
      updatetitle(client);
      if (client == client->mon->sel)
        drawbar(client->mon);
      drawtab(client->mon);
    }

    else if (ev->atom == net_atom[NetWMIcon]) {
      updateicon(client);
      if (client == client->mon->sel)
        drawbar(client->mon);
    }

    if (ev->atom == net_atom[NetWMWindowType])
      updatewindowtype(client);
  }
}

/* stop the main event loop */
// void quit(const Arg *arg) { running = 0; }
/* stop the main event loop; arg->i != 0 asks main() to re-exec dwm afterwards
 */
void quit(const Arg *arg) {
  if (arg->i)
    restart = 1;
  running = 0;
}

/* SIGHUP: restart dwm in place (re-exec after cleanup) */
void sighup(int unused) {
  Arg a = {.i = 1};
  quit(&a);
}

/* SIGTERM: exit cleanly (e.g. on logout) instead of being killed */
void sigterm(int unused) {
  Arg a = {.i = 0};
  quit(&a);
}

/* return the PID that owns an X window */
pid_t winpid(Window w) {

  pid_t result = 0;

#ifdef __linux__
  xcb_res_client_id_spec_t spec = {0};
  spec.client = w;
  spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

  xcb_generic_error_t *e = NULL;
  xcb_res_query_client_ids_cookie_t client =
      xcb_res_query_client_ids(xcb_conn, 1, &spec);
  xcb_res_query_client_ids_reply_t *r =
      xcb_res_query_client_ids_reply(xcb_conn, client, &e);

  if (!r)
    return (pid_t)0;

  xcb_res_client_id_value_iterator_t i =
      xcb_res_query_client_ids_ids_iterator(r);
  for (; i.rem; xcb_res_client_id_value_next(&i)) {
    spec = i.data->spec;
    if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
      uint32_t *t = xcb_res_client_id_value_value(i.data);
      result = *t;
      break;
    }
  }

  free(r);

  if (result == (pid_t)-1)
    result = 0;

#endif /* __linux__ */

#ifdef __OpenBSD__
  Atom type;
  int format;
  unsigned long len, bytes;
  unsigned char *prop;
  pid_t ret;

  if (XGetWindowProperty(display, w, XInternAtom(display, "_NET_WM_PID", 0), 0,
                         1, False, AnyPropertyType, &type, &format, &len,
                         &bytes, &prop) != Success ||
      !prop)
    return 0;

  ret = *(pid_t *)prop;
  XFree(prop);
  result = ret;

#endif /* __OpenBSD__ */
  return result;
}

/* return the parent PID of a process */
pid_t getparentprocess(pid_t p) {
  unsigned int v = 0;

#ifdef __linux__
  FILE *f;
  char buf[256];
  snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

  if (!(f = fopen(buf, "r")))
    return 0;

  fscanf(f, "%*u %*s %*client %u", &v);
  fclose(f);
#endif /* __linux__*/

#ifdef __OpenBSD__
  int n;
  kvm_t *kd;
  struct kinfo_proc *kp;

  kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
  if (!kd)
    return 0;

  kp = kvm_getprocs(kd, KERN_PROC_PID, p, sizeof(*kp), &n);
  v = kp->p_ppid;
#endif /* __OpenBSD__ */

  return (pid_t)v;
}

/* true if process `descendant` is (transitively) a child of process `ancestor`
 */
int isdescprocess(pid_t ancestor, pid_t descendant) {
  while (ancestor != descendant && descendant != 0)
    descendant = getparentprocess(descendant);

  return (int)descendant;
}

/* find the terminal client that spawned the given window, if any */
Client *termforwin(const Client *w) {
  Client *client;
  Monitor *mon;

  if (!w->pid || w->is_terminal)
    return NULL;

  for (mon = monitors; mon; mon = mon->next) {
    for (client = mon->clients; client; client = client->next) {
      if (client->is_terminal && !client->swallowing && client->pid &&
          isdescprocess(client->pid, w->pid))
        return client;
    }
  }

  return NULL;
}

/* find the client currently swallowing the given window */
Client *swallowingclient(Window w) {
  Client *client;
  Monitor *mon;

  for (mon = monitors; mon; mon = mon->next) {
    for (client = mon->clients; client; client = client->next) {
      if (client->swallowing && client->swallowing->win == w)
        return client;
    }
  }

  return NULL;
}

/* return the tiled client that overlaps a rectangle the most */
Client *recttoclient(int x, int y, int w, int h) {
  Client *client, *best = NULL;
  int overlap, max_overlap = 0;

  for (client = nexttiled(sel_mon->clients); client;
       client = nexttiled(client->next)) {
    if ((overlap = INTERSECTC(x, y, w, h, client)) > max_overlap) {
      max_overlap = overlap;
      best = client;
    }
  }
  return best;
}

/* return the monitor that overlaps a rectangle the most */
Monitor *recttomon(int x, int y, int w, int h) {
  Monitor *mon, *best = sel_mon;
  int overlap, max_overlap = 0;

  for (mon = monitors; mon; mon = mon->next)
    if ((overlap = INTERSECT(x, y, w, h, mon)) > max_overlap) {
      max_overlap = overlap;
      best = mon;
    }
  return best;
}

/* unlink a systray icon from the tray and free it */
void removesystrayicon(Client *i) {
  Client **ii;

  if (!showsystray || !systray || !i)
    return;
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next)
    ;
  if (ii)
    *ii = i->next;
  free(i);
}

/* resize a client to a geometry after applying its size hints */
void resize(Client *client, int x, int y, int w, int h, int interact) {
  if (applysizehints(client, &x, &y, &w, &h, interact))
    resizeclient(client, x, y, w, h);
}

/* move / resize a monitor's bar window, accounting for gaps and the systray */
void resizebarwin(Monitor *mon) {
  unsigned int w = floatbar ? mon->win_w - 2 * mon->gappov : mon->win_w;
  if (showsystray && mon == systraytomon(mon)) {
    unsigned int stw = getsystraywidth();
    w = stw < w ? w - stw : 1;
  }
  if (floatbar) {
    XMoveResizeWindow(display, mon->bar_win, mon->win_x + mon->gappov,
                      mon->bar_y, w, bar_h);
  } else {
    XMoveResizeWindow(display, mon->bar_win, mon->win_x, mon->bar_y, w, bar_h);
  }
}

/* apply a new geometry to a client and notify it via ConfigureNotify */
void resizeclient(Client *client, int x, int y, int w, int h) {
  XWindowChanges wc;

  client->old_x = client->x;
  client->x = wc.x = x;
  client->old_y = client->y;
  client->y = wc.y = y;
  client->old_w = client->w;
  client->w = wc.width = w;
  client->old_h = client->h;
  client->h = wc.height = h;

  if (client->being_moved)
    return;

  wc.border_width = client->border_w;
  /// patch///
  if (((nexttiled(client->mon->clients) == client &&
        !nexttiled(client->next)) ||
       &monocle == client->mon->layout[client->mon->sel_layout]->arrange) &&
      !client->is_fullscreen && !client->is_floating) {
    client->w = wc.width += client->border_w * 2;
    client->h = wc.height += client->border_w * 2;
    wc.border_width = 0;
  }
  /// patch///
  XConfigureWindow(display, client->win,
                   CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
  configure(client);
  XSync(display, False);
}

/* resize the selected client with the mouse from its bottom-right corner */
void resizemouse(const Arg *arg) {
  int ocx, ocy, nw, nh;
  Client *client;
  Monitor *mon;
  XEvent ev;
  Time lasttime = 0;

  if (!(client = sel_mon->sel))
    return;
  if (client
          ->is_fullscreen) /* no support resizing fullscreen windows by mouse */
    return;
  restack(sel_mon);
  ocx = client->x;
  ocy = client->y;
  if (XGrabPointer(display, root, False, MOUSEMASK, GrabModeAsync,
                   GrabModeAsync, None, cursor[CurResize]->cursor,
                   CurrentTime) != GrabSuccess)
    return;
  XWarpPointer(display, None, client->win, 0, 0, 0, 0,
               client->w + client->border_w - 1,
               client->h + client->border_w - 1);
  do {
    XMaskEvent(display, MOUSEMASK | ExposureMask | SubstructureRedirectMask,
               &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      event_handlers[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nw = MAX(ev.xmotion.x - ocx - 2 * client->border_w + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * client->border_w + 1, 1);
      if (client->mon->win_x + nw >= sel_mon->win_x &&
          client->mon->win_x + nw <= sel_mon->win_x + sel_mon->win_w &&
          client->mon->win_y + nh >= sel_mon->win_y &&
          client->mon->win_y + nh <= sel_mon->win_y + sel_mon->win_h) {
        if (!client->is_floating &&
            sel_mon->layout[sel_mon->sel_layout]->arrange &&
            (abs(nw - client->w) > snap || abs(nh - client->h) > snap))
          togglefloating(NULL);
      }
      if (!sel_mon->layout[sel_mon->sel_layout]->arrange || client->is_floating)
        resize(client, client->x, client->y, nw, nh, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(display, None, client->win, 0, 0, 0, 0,
               client->w + client->border_w - 1,
               client->h + client->border_w - 1);
  XUngrabPointer(display, CurrentTime);
  while (XCheckMaskEvent(display, EnterWindowMask, &ev))
    ;
  if ((mon = recttomon(client->x, client->y, client->w, client->h)) !=
      sel_mon) {
    sendmon(client, mon);
    sel_mon = mon;
    focus(NULL);
  }
}

/* honour a systray icon's request to resize itself */
void resizerequest(XEvent *e) {
  XResizeRequestEvent *ev = &e->xresizerequest;
  Client *i;

  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    resizebarwin(sel_mon);
    updatesystray();
  }
}

/* raise the floating/selected window and restore tiled stacking order */
void restack(Monitor *mon) {
  Client *client;
  XEvent ev;
  XWindowChanges wc;

  drawbar(mon);
  drawtab(mon);
  if (!mon->sel)
    return;
  if (mon->sel->is_floating || !mon->layout[mon->sel_layout]->arrange)
    XRaiseWindow(display, mon->sel->win);
  if (mon->layout[mon->sel_layout]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = mon->bar_win;
    for (client = mon->stack; client; client = client->stack_next)
      if (!client->is_floating && ISVISIBLE(client)) {
        XConfigureWindow(display, client->win, CWSibling | CWStackMode, &wc);
        wc.sibling = client->win;
      }
  }
  XSync(display, False);
  while (XCheckMaskEvent(display, EnterWindowMask, &ev))
    ;
}

/* the main event loop: read X events and dispatch them to handlers */
void run(void) {
  XEvent ev;
  /* main event loop */
  XSync(display, False);
  while (running && !XNextEvent(display, &ev))
    if (event_handlers[ev.type])
      event_handlers[ev.type](&ev); /* call handler */
}

/* at startup, adopt windows that already exist on the display */
void scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(display, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(display, wins[i], &wa) ||
          wa.override_redirect || XGetTransientForHint(display, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(display, wins[i], &wa))
        continue;
      if (XGetTransientForHint(display, wins[i], &d1) &&
          (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

/* move a client to another monitor */
void sendmon(Client *client, Monitor *mon) {
  if (client->mon == mon)
    return;
  unfocus(client, 1);
  detach(client);
  detachstack(client);
  client->mon = mon;
  client->tags = mon->tagset[mon->sel_tags]; /* assign tags of target monitor */
  attach(client);
  attachstack(client);
  setclienttagprop(client);
  focus(NULL);
  arrange(NULL);
}

/* change the current monitor's border width and re-fit its clients */
void setborderpx(const Arg *arg) {
  Client *client;
  int prev_borderpx = sel_mon->borderpx;

  if (arg->i == 0)
    sel_mon->borderpx = borderpx;
  else if (sel_mon->borderpx + arg->i < 0)
    sel_mon->borderpx = 0;
  else
    sel_mon->borderpx += arg->i;

  for (client = sel_mon->clients; client; client = client->next) {
    if (client->border_w + arg->i < 0)
      client->border_w = sel_mon->borderpx = 0;
    else
      client->border_w = sel_mon->borderpx;
    if (client->is_floating || !sel_mon->layout[sel_mon->sel_layout]->arrange) {
      if (arg->i != 0 && prev_borderpx + arg->i >= 0)
        resize(client, client->x, client->y, client->w - (arg->i * 2),
               client->h - (arg->i * 2), 0);
      else if (arg->i != 0)
        resizeclient(client, client->x, client->y, client->w, client->h);
      else if (prev_borderpx > borderpx)
        resize(client, client->x, client->y,
               client->w + 2 * (prev_borderpx - borderpx),
               client->h + 2 * (prev_borderpx - borderpx), 0);
      else if (prev_borderpx < borderpx)
        resize(client, client->x, client->y,
               client->w - 2 * (borderpx - prev_borderpx),
               client->h - 2 * (borderpx - prev_borderpx), 0);
    }
  }
  arrange(sel_mon);
}

/* set a client's WM_STATE property */
void setclientstate(Client *client, long state) {
  long data[] = {state, None};

  XChangeProperty(display, client->win, wm_atom[WMState], wm_atom[WMState], 32,
                  PropModeReplace, (unsigned char *)data, 2);
}

/* set the EWMH _NET_CURRENT_DESKTOP property */
void setcurrentdesktop(void) {
  long data[] = {0};
  XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)data, 1);
}
/* publish the tag names as EWMH _NET_DESKTOP_NAMES */
void setdesktopnames(void) {
  XTextProperty text;
  Xutf8TextListToTextProperty(display, tags, TAGSLENGTH, XUTF8StringStyle,
                              &text);
  XSetTextProperty(display, root, &text, net_atom[NetDesktopNames]);
}

/* send an X ClientMessage to a window (WM protocol or XEmbed message) */
int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2,
              long d3, long d4) {
  int n;
  Atom *protocols, mt;
  int exists = 0;
  XEvent ev;

  if (proto == wm_atom[WMTakeFocus] || proto == wm_atom[WMDelete]) {
    mt = wm_atom[WMProtocols];
    if (XGetWMProtocols(display, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  } else {
    exists = True;
    mt = proto;
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mt;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    XSendEvent(display, w, False, mask, &ev);
  }
  return exists;
}

/* set the EWMH _NET_NUMBER_OF_DESKTOPS property */
void setnumdesktops(void) {
  long data[] = {TAGSLENGTH};
  XChangeProperty(display, root, net_atom[NetNumberOfDesktops], XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)data, 1);
}

/* give X input focus to a client and mark it _NET_ACTIVE_WINDOW */
void setfocus(Client *client) {
  if (!client->never_focus) {
    XSetInputFocus(display, client->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(display, root, net_atom[NetActiveWindow], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&(client->win), 1);
  }
  sendevent(client->win, wm_atom[WMTakeFocus], NoEventMask,
            wm_atom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

/* enter or leave fullscreen for a client */
void setfullscreen(Client *client, int fullscreen) {
  if (fullscreen && !client->is_fullscreen) {
    XChangeProperty(display, client->win, net_atom[NetWMState], XA_ATOM, 32,
                    PropModeReplace,
                    (unsigned char *)&net_atom[NetWMFullscreen], 1);
    client->is_fullscreen = 1;
    client->old_state = client->is_floating;
    client->old_border_w = client->border_w;
    client->border_w = 0;
    client->is_floating = 1;
    resizeclient(client, client->mon->mon_x, client->mon->mon_y,
                 client->mon->mon_w, client->mon->mon_h);
    XRaiseWindow(display, client->win);
  } else if (!fullscreen && client->is_fullscreen) {
    XChangeProperty(display, client->win, net_atom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)0, 0);
    client->is_fullscreen = 0;
    client->is_floating = client->old_state;
    client->border_w = client->old_border_w;
    client->x = client->old_x;
    client->y = client->old_y;
    client->w = client->old_w;
    client->h = client->old_h;
    resizeclient(client, client->x, client->y, client->w, client->h);
    arrange(client->mon);
  }
}

/* set (or toggle) the layout for the current tag */
void setlayout(const Arg *arg) {
  if (!arg || !arg->v || arg->v != sel_mon->layout[sel_mon->sel_layout])
    sel_mon->sel_layout =
        sel_mon->pertag->sel_layouts[sel_mon->pertag->cur_tag] ^= 1;
  if (arg && arg->v)
    sel_mon->layout[sel_mon->sel_layout] =
        sel_mon->pertag
            ->layout_idxs[sel_mon->pertag->cur_tag][sel_mon->sel_layout] =
            (Layout *)arg->v;
  strncpy(sel_mon->layout_symbol, sel_mon->layout[sel_mon->sel_layout]->symbol,
          sizeof sel_mon->layout_symbol);
  if (sel_mon->sel)
    arrange(sel_mon);
  else
    drawbar(sel_mon);
}

/* change the selected client's size factor within the stack */
void setcfact(const Arg *arg) {
  float f;
  Client *client;

  client = sel_mon->sel;

  if (!arg || !client || !sel_mon->layout[sel_mon->sel_layout]->arrange)
    return;
  if (!arg->f)
    f = 1.0;
  else if (arg->f > 4.0) // set fact absolutely
    f = arg->f - 4.0;
  else
    f = arg->f + client->cfact;
  if (f < 0.25)
    f = 0.25;
  else if (f > 4.0)
    f = 4.0;
  client->cfact = f;
  arrange(sel_mon);
}

/* change the master-area width fraction for the current tag */
/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg) {
  float f;

  if (!arg || !sel_mon->layout[sel_mon->sel_layout]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + sel_mon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  sel_mon->mfact = sel_mon->pertag->mfacts[sel_mon->pertag->cur_tag] = f;
  arrange(sel_mon);
}

/* one-time init: screen, atoms, cursors, colours, bars, root event mask */
void setup(void) {
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;

  /* clean up any zombies immediately */
  sigchld(0);

  /* init screen */
  screen = DefaultScreen(display);
  screen_w = DisplayWidth(display, screen);
  screen_h = DisplayHeight(display, screen);
  root = RootWindow(display, screen);
  drw = drw_create(display, screen, root, screen_w, screen_h);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lr_pad = drw->fonts->h;
  bar_h = drw->fonts->h + 2 + vertpadbar + borderpx * 2;
  tab_h = vertpadtab;
  // bh_n = vertpadtab;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(display, "UTF8_STRING", False);
  wm_atom[WMProtocols] = XInternAtom(display, "WM_PROTOCOLS", False);
  wm_atom[WMDelete] = XInternAtom(display, "WM_DELETE_WINDOW", False);
  wm_atom[WMState] = XInternAtom(display, "WM_STATE", False);
  wm_atom[WMTakeFocus] = XInternAtom(display, "WM_TAKE_FOCUS", False);
  net_atom[NetActiveWindow] = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
  net_atom[NetSupported] = XInternAtom(display, "_NET_SUPPORTED", False);
  net_atom[NetSystemTray] = XInternAtom(display, "_NET_SYSTEM_TRAY_S0", False);
  net_atom[NetSystemTrayOP] =
      XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
  net_atom[NetSystemTrayOrientation] =
      XInternAtom(display, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  net_atom[NetSystemTrayOrientationHorz] =
      XInternAtom(display, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  net_atom[NetWMName] = XInternAtom(display, "_NET_WM_NAME", False);
  net_atom[NetWMIcon] = XInternAtom(display, "_NET_WM_ICON", False);
  net_atom[NetWMState] = XInternAtom(display, "_NET_WM_STATE", False);
  net_atom[NetWMCheck] =
      XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
  net_atom[NetWMFullscreen] =
      XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
  net_atom[NetWMWindowType] =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  net_atom[NetWMWindowTypeDialog] =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  net_atom[NetClientList] = XInternAtom(display, "_NET_CLIENT_LIST", False);
  xembed_atom[Manager] = XInternAtom(display, "MANAGER", False);
  xembed_atom[Xembed] = XInternAtom(display, "_XEMBED", False);
  xembed_atom[XembedInfo] = XInternAtom(display, "_XEMBED_INFO", False);
  net_atom[NetDesktopViewport] =
      XInternAtom(display, "_NET_DESKTOP_VIEWPORT", False);
  net_atom[NetNumberOfDesktops] =
      XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False);
  net_atom[NetCurrentDesktop] =
      XInternAtom(display, "_NET_CURRENT_DESKTOP", False);
  net_atom[NetDesktopNames] = XInternAtom(display, "_NET_DESKTOP_NAMES", False);
  net_atom[NetClientInfo] = XInternAtom(display, "_NET_CLIENT_INFO", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  cursor[CurResizeHorzArrow] = drw_cur_create(drw, XC_sb_h_double_arrow);
  cursor[CurResizeVertArrow] = drw_cur_create(drw, XC_sb_v_double_arrow);
  /* init appearance */
  scheme = ecalloc(LENGTH(colors) + 1, sizeof(Clr *));
  scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], 3);
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);
  drw_clr_create(drw, &border_clr, col_borderbar);
  /* init system tray */
  updatesystray();
  /* init bars */
  updatebars();
  updatestatus();
  updatebarpos(sel_mon);
  updatepreview();
  /* supporting window for NetWMCheck */
  wm_check_win = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(display, wm_check_win, net_atom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wm_check_win, 1);
  XChangeProperty(display, wm_check_win, net_atom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"dwm", 3);
  XChangeProperty(display, root, net_atom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wm_check_win, 1);
  /* EWMH support per view */
  XChangeProperty(display, root, net_atom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)net_atom, NetLast);
  setnumdesktops();
  setcurrentdesktop();
  setdesktopnames();
  setviewport();
  XDeleteProperty(display, root, net_atom[NetClientList]);
  XDeleteProperty(display, root, net_atom[NetClientInfo]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(display, root, CWEventMask | CWCursor, &wa);
  XSelectInput(display, root, wa.event_mask);
  signal(SIGHUP, sighup);
  signal(SIGTERM, sigterm);
  grabkeys();
  focus(NULL);
}
/* set the EWMH _NET_DESKTOP_VIEWPORT property */
void setviewport(void) {
  long data[] = {0, 0};
  XChangeProperty(display, root, net_atom[NetDesktopViewport], XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)data, 2);
}

/* set or clear a client's urgency hint */
void seturgent(Client *client, int urg) {
  XWMHints *wmh;

  client->is_urgent = urg;
  if (!(wmh = XGetWMHints(display, client->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(display, client->win, wmh);
  XFree(wmh);
}

/* re-map a previously hidden client */
void show(Client *client) {
  if (!client || !HIDDEN(client))
    return;

  XMapWindow(display, client->win);
  setclientstate(client, NormalState);
  arrange(client->mon);
}

/* recursively map visible clients and slide hidden ones off-screen */
void showhide(Client *client) {
  if (!client)
    return;
  if (ISVISIBLE(client)) {
    /* show clients top down */
    XMoveWindow(display, client->win, client->x, client->y);
    if ((!client->mon->layout[client->mon->sel_layout]->arrange ||
         client->is_floating) &&
        !client->is_fullscreen)
      resize(client, client->x, client->y, client->w, client->h, 0);
    showhide(client->stack_next);
  } else {
    /* hide clients bottom up */
    showhide(client->stack_next);
    XMoveWindow(display, client->win, WIDTH(client) * -2, client->y);
  }
}

/* show or hide the hovered tag's preview thumbnail window */
void showtagpreview(int tag) {
  if (!sel_mon->preview_show || !tag_preview) {
    XUnmapWindow(display, sel_mon->tag_win);
    return;
  }

  if (sel_mon->tagmap[tag]) {
    XSetWindowBackgroundPixmap(display, sel_mon->tag_win, sel_mon->tagmap[tag]);
    XCopyArea(display, sel_mon->tagmap[tag], sel_mon->tag_win, drw->gc, 0, 0,
              sel_mon->mon_w / scalepreview, sel_mon->mon_h / scalepreview, 0,
              0);
    XSync(display, False);
    XMapWindow(display, sel_mon->tag_win);
  } else
    XUnmapWindow(display, sel_mon->tag_win);
}

/* reap zombie child processes */
void sigchld(int unused) {
  if (signal(SIGCHLD, sigchld) == SIG_ERR)
    die("can't install SIGCHLD handler:");
  while (0 < waitpid(-1, NULL, WNOHANG))
    ;
}

/* fork and exec a command */
void spawn(const Arg *arg) { spawncmd(arg); }
/* fork / exec a command and return the child PID */
pid_t spawncmd(const Arg *arg) {
  pid_t pid;
  // if (fork() == 0) {
  if ((pid = fork()) == 0) {
    if (display)
      close(ConnectionNumber(display));
    setsid();
    execvp(((char **)arg->v)[0], (char **)arg->v);
    die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
  }
  return pid;
}

/* store a client's tags and monitor number in _NET_CLIENT_INFO */
void setclienttagprop(Client *client) {
  long data[] = {(long)client->tags, (long)client->mon->num};
  XChangeProperty(display, client->win, net_atom[NetClientInfo], XA_CARDINAL,
                  32, PropModeReplace, (unsigned char *)data, 2);
}

/* snapshot the currently viewed tags' contents for use as tag previews */
void switchtag(void) {
  int i;
  unsigned int occupied_tags = 0;
  Client *client;
  Imlib_Image image;

  for (client = sel_mon->clients; client; client = client->next)
    occupied_tags |= client->tags;
  for (i = 0; i < LENGTH(tags); i++) {
    if (sel_mon->tagset[sel_mon->sel_tags] & 1 << i) {
      if (sel_mon->tagmap[i] != 0) {
        XFreePixmap(display, sel_mon->tagmap[i]);
        sel_mon->tagmap[i] = 0;
      }
      if (occupied_tags & 1 << i && tag_preview) {
        image = imlib_create_image(screen_w, screen_h);
        imlib_context_set_image(image);
        imlib_context_set_display(display);
        imlib_context_set_visual(DefaultVisual(display, screen));
        imlib_context_set_drawable(RootWindow(display, screen));
        imlib_copy_drawable_to_image(0, sel_mon->mon_x, sel_mon->mon_y,
                                     sel_mon->mon_w, sel_mon->mon_h, 0, 0, 1);
        sel_mon->tagmap[i] = XCreatePixmap(
            display, sel_mon->tag_win, sel_mon->mon_w / scalepreview,
            sel_mon->mon_h / scalepreview, DefaultDepth(display, screen));
        imlib_context_set_drawable(sel_mon->tagmap[i]);
        imlib_render_image_part_on_drawable_at_size(
            0, 0, sel_mon->mon_w, sel_mon->mon_h, 0, 0,
            sel_mon->mon_w / scalepreview, sel_mon->mon_h / scalepreview);
        imlib_free_image();
      }
    }
  }
}

/* cycle or set the tab-bar display mode for the current monitor */
void tabmode(const Arg *arg) {
  if (arg && arg->i >= 0)
    sel_mon->showtab = arg->ui % showtab_nmodes;
  else
    sel_mon->showtab = (sel_mon->showtab + 1) % showtab_nmodes;
  arrange(sel_mon);
  updatecurrentdesktop();
}

/* move the selected client to the given tag(s) */
void tag(const Arg *arg) {
  Client *client;
  if (sel_mon->sel && arg->ui & TAGMASK) {
    client = sel_mon->sel;
    sel_mon->sel->tags = arg->ui & TAGMASK;
    setclienttagprop(client);
    focus(NULL);
    arrange(sel_mon);
  }
}

/* send the selected client to the monitor in the given direction */
void tagmon(const Arg *arg) {
  if (!sel_mon->sel || !monitors->next)
    return;
  sendmon(sel_mon->sel, dirtomon(arg->i));
}

/* show or hide the current monitor's bar */
void togglebar(const Arg *arg) {
  sel_mon->showbar = sel_mon->pertag->showbars[sel_mon->pertag->cur_tag] =
      !sel_mon->showbar;
  updatebarpos(sel_mon);
  resizebarwin(sel_mon);
  if (showsystray && systray) {
    XWindowChanges wc;
    if (!sel_mon->showbar)
      wc.y = -bar_h;
    else if (sel_mon->showbar) {
      wc.y = sel_mon->gappoh;
      if (!sel_mon->topbar)
        wc.y = sel_mon->mon_h - bar_h + sel_mon->gappoh;
    }
    XConfigureWindow(display, systray->win, CWY, &wc);
  }
  arrange(sel_mon);
}

/* toggle the selected client between floating and tiled */
void togglefloating(const Arg *arg) {
  if (!sel_mon->sel)
    return;
  if (sel_mon->sel->is_fullscreen) /* no support for fullscreen windows */
    return;
  sel_mon->sel->is_floating =
      !sel_mon->sel->is_floating || sel_mon->sel->is_fixed;
  if (sel_mon->sel->is_floating)
    resize(sel_mon->sel, sel_mon->sel->x, sel_mon->sel->y, sel_mon->sel->w,
           sel_mon->sel->h, 0);
  arrange(sel_mon);
}

/* toggle fullscreen on the selected client */
void togglefullscr(const Arg *arg) {
  if (sel_mon->sel)
    setfullscreen(sel_mon->sel, !sel_mon->sel->is_fullscreen);
}

/* add or remove tag(s) on the selected client */
void toggletag(const Arg *arg) {
  unsigned int newtags;

  if (!sel_mon->sel)
    return;
  newtags = sel_mon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    sel_mon->sel->tags = newtags;
    setclienttagprop(sel_mon->sel);
    focus(NULL);
    arrange(sel_mon);
  }
  updatecurrentdesktop();
}

/* add or remove tag(s) from the current view */
void toggleview(const Arg *arg) {
  unsigned int newtagset =
      sel_mon->tagset[sel_mon->sel_tags] ^ (arg->ui & TAGMASK);
  int i;

  if (newtagset) {
    switchtag();
    sel_mon->tagset[sel_mon->sel_tags] = newtagset;

    if (newtagset == ~0) {
      sel_mon->pertag->prev_tag = sel_mon->pertag->cur_tag;
      sel_mon->pertag->cur_tag = 0;
    }

    /* test if the user did not select the same tag */
    if (!(newtagset & 1 << (sel_mon->pertag->cur_tag - 1))) {
      sel_mon->pertag->prev_tag = sel_mon->pertag->cur_tag;
      for (i = 0; !(newtagset & 1 << i); i++)
        ;
      sel_mon->pertag->cur_tag = i + 1;
    }

    /* apply settings for this view */
    sel_mon->nmaster = sel_mon->pertag->nmasters[sel_mon->pertag->cur_tag];
    sel_mon->mfact = sel_mon->pertag->mfacts[sel_mon->pertag->cur_tag];
    sel_mon->sel_layout =
        sel_mon->pertag->sel_layouts[sel_mon->pertag->cur_tag];
    sel_mon->layout[sel_mon->sel_layout] =
        sel_mon->pertag
            ->layout_idxs[sel_mon->pertag->cur_tag][sel_mon->sel_layout];
    sel_mon->layout[sel_mon->sel_layout ^ 1] =
        sel_mon->pertag
            ->layout_idxs[sel_mon->pertag->cur_tag][sel_mon->sel_layout ^ 1];

    if (sel_mon->showbar != sel_mon->pertag->showbars[sel_mon->pertag->cur_tag])
      togglebar(NULL);

    focus(NULL);
    arrange(sel_mon);
  }
  updatecurrentdesktop();
}

/* hide the selected client and remember it on the hidden-window stack */
void hidewin(const Arg *arg) {
  if (!sel_mon->sel)
    return;
  Client *client = (Client *)sel_mon->sel;
  hide(client);
  hidden_win_stack[++hidden_win_stack_top] = client;
}

/* un-hide the most recently hidden client for the current tag */
void restorewin(const Arg *arg) {
  int i = hidden_win_stack_top;
  while (i > -1) {
    if (HIDDEN(hidden_win_stack[i]) &&
        hidden_win_stack[i]->tags == sel_mon->tagset[sel_mon->sel_tags]) {
      show(hidden_win_stack[i]);
      focus(hidden_win_stack[i]);
      restack(sel_mon);
      for (int j = i; j < hidden_win_stack_top; ++j) {
        hidden_win_stack[j] = hidden_win_stack[j + 1];
      }
      --hidden_win_stack_top;
      return;
    }
    --i;
  }
}

/* drop focus from a client and reset its border colour */
void unfocus(Client *client, int setfocus) {
  if (!client)
    return;
  grabbuttons(client, 0);
  XSetWindowBorder(display, client->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(display, root, net_atom[NetActiveWindow]);
  }
}

/* stop managing a client (window gone or closed) and clean up */
void unmanage(Client *client, int destroyed) {
  Monitor *mon = client->mon;
  XWindowChanges wc;

  if (client->swallowing) {
    unswallow(client);
    return;
  }

  Client *s = swallowingclient(client->win);
  if (s) {
    free(s->swallowing);
    s->swallowing = NULL;
    arrange(mon);
    focus(NULL);
    return;
  }

  detach(client);
  detachstack(client);
  freeicon(client);

  if (!destroyed) {
    wc.border_width = client->old_border_w;
    XGrabServer(display); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(display, client->win, NoEventMask);
    XConfigureWindow(display, client->win, CWBorderWidth,
                     &wc); /* restore border */
    XUngrabButton(display, AnyButton, AnyModifier, client->win);
    setclientstate(client, WithdrawnState);
    XSync(display, False);
    XSetErrorHandler(xerror);
    XUngrabServer(display);
  }
  free(client);
  // focus(NULL);
  // updateclientlist();
  // arrange(mon);

  if (!s) {
    arrange(mon);
    focus(NULL);
    updateclientlist();
  }
}

/* unmanage a client that unmapped itself (or remap a stray systray icon) */
void unmapnotify(XEvent *e) {
  Client *client;
  XUnmapEvent *ev = &e->xunmap;

  if ((client = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(client, WithdrawnState);
    else
      unmanage(client, 0);
  } else if ((client = wintosystrayicon(ev->window))) {
    /* KLUDGE! sometimes icons occasionally unmap their windows, but do
     * _not_ destroy them. We map those windows back */
    XMapRaised(display, client->win);
    updatesystray();
  }
}

/* create the bar, tab, and preview windows for every monitor */
void updatebars(void) {
  unsigned int w;
  Monitor *mon;
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask |
                                           PointerMotionMask};

  XClassHint ch = {"dwm", "dwm"};
  for (mon = monitors; mon; mon = mon->next) {
    if (mon->bar_win)
      continue;
    w = mon->win_w;
    if (showsystray && systray && mon == systraytomon(mon)) {
      unsigned int stw = getsystraywidth();
      w = stw < w ? w - stw : 1;
    }
    mon->bar_win = XCreateWindow(
        display, root, mon->win_x + mon->gappov, mon->bar_y,
        w - 2 * mon->gappov, bar_h, 0, DefaultDepth(display, screen),
        CopyFromParent, DefaultVisual(display, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(display, mon->bar_win, cursor[CurNormal]->cursor);
    if (showsystray && systray && mon == systraytomon(mon))
      XMapRaised(display, systray->win);
    XMapRaised(display, mon->bar_win);
    mon->tab_win = XCreateWindow(
        display, root, mon->win_x + mon->gappov, mon->tab_y,
        mon->win_w - 2 * mon->gappov, tab_h, 0, DefaultDepth(display, screen),
        CopyFromParent, DefaultVisual(display, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(display, mon->tab_win, cursor[CurNormal]->cursor);
    XMapRaised(display, mon->tab_win);
    XSetClassHint(display, mon->bar_win, &ch);
    mon->tag_win = XCreateWindow(
        display, root, mon->win_x, mon->bar_y + bar_h, mon->mon_w / 4,
        mon->mon_h / 4, 0, DefaultDepth(display, screen), CopyFromParent,
        DefaultVisual(display, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(display, mon->tag_win, cursor[CurNormal]->cursor);
    XMapRaised(display, mon->tag_win);
    XUnmapWindow(display, mon->tag_win);
  }
}

/* create the tag-preview window for every monitor */
void updatepreview(void) {
  Monitor *mon;

  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask};
  for (mon = monitors; mon; mon = mon->next) {
    if (mon->tag_win)
      continue;
    mon->tag_win = XCreateWindow(
        display, root, mon->win_x, mon->bar_y + bar_h, mon->mon_w / 4,
        mon->mon_h / 4, 0, DefaultDepth(display, screen), CopyFromParent,
        DefaultVisual(display, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(display, mon->tag_win, cursor[CurNormal]->cursor);
    XMapRaised(display, mon->tag_win);
    XUnmapWindow(display, mon->tag_win);
  }
}

/* recompute a monitor's bar / tab position and shrink its usable area */
void updatebarpos(Monitor *mon) {
  Client *client;
  int nvis = 0; /* nvis: number of visible clients on this monitor */

  mon->win_y = mon->mon_y;
  mon->win_h = mon->mon_h;

  for (client = mon->clients; client; client = client->next) {
    if (ISVISIBLE(client))
      ++nvis;
  }

  if (mon->showtab == showtab_always ||
      ((mon->showtab == showtab_auto) && (nvis > 1) &&
       (mon->layout[mon->sel_layout]->arrange == monocle))) {
    mon->topbar = !toptab;
    mon->win_h -= tab_h +
                  ((mon->topbar == toptab && mon->showbar) ? 0 : mon->gappoh) -
                  mon->gappoh;
    mon->tab_y =
        mon->toptab
            ? mon->win_y + ((mon->topbar && mon->showbar) ? 0 : mon->gappoh)
            : mon->win_y + mon->win_h - mon->gappoh;
    if (mon->toptab)
      mon->win_y += tab_h + ((mon->topbar && mon->showbar) ? 0 : mon->gappoh) -
                    mon->gappoh;
  } else {
    mon->tab_y = -tab_h - mon->gappoh;
    mon->topbar = topbar;
  }
  if (mon->showbar) {
    if (floatbar) {
      mon->win_h = mon->win_h - mon->gappoh - bar_h;
      mon->bar_y =
          mon->topbar ? mon->win_y + mon->gappoh : mon->win_y + mon->win_h;
    } else {
      mon->win_h = mon->win_h - bar_h;
      mon->bar_y = mon->topbar ? mon->win_y : mon->win_y + mon->win_h;
    }
    if (mon->topbar) {
      mon->win_y += floatbar ? bar_h + mon->gappoh : bar_h;
    }
  } else
    mon->bar_y = -bar_h - mon->gappoh;
}

/* rebuild the EWMH _NET_CLIENT_LIST property */
void updateclientlist() {
  Client *client;
  Monitor *mon;

  XDeleteProperty(display, root, net_atom[NetClientList]);
  for (mon = monitors; mon; mon = mon->next)
    for (client = mon->clients; client; client = client->next)
      XChangeProperty(display, root, net_atom[NetClientList], XA_WINDOW, 32,
                      PropModeAppend, (unsigned char *)&(client->win), 1);
}

/* update _NET_CURRENT_DESKTOP from the current tagset */
void updatecurrentdesktop(void) {
  long rawdata[] = {sel_mon->tagset[sel_mon->sel_tags]};
  int i = 0;
  /* i ends as the index of the highest set tag bit = current desktop number */
  while (*rawdata >> (i + 1)) {
    i++;
  }
  long data[] = {i};
  XChangeProperty(display, root, net_atom[NetCurrentDesktop], XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)data, 1);
}

/* detect monitor changes via Xinerama and add / remove / resize monitors */
int updategeom(void) {
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(display)) {
    int i, j, nmons,
        nscreens; /* count of existing monitors / of unique screens */
    Client *client;
    Monitor *mon;
    XineramaScreenInfo *info = XineramaQueryScreens(display, &nscreens);
    XineramaScreenInfo *unique = NULL;

    for (nmons = 0, mon = monitors; mon; mon = mon->next, nmons++)
      ;
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nscreens, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nscreens; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nscreens = j;
    /* add monitors if there are now more screens than monitors */
    for (i = nmons; i < nscreens; i++) {
      for (mon = monitors; mon && mon->next; mon = mon->next)
        ;
      if (mon)
        mon->next = createmon();
      else
        monitors = createmon();
    }
    for (i = 0, mon = monitors; i < nscreens && mon; mon = mon->next, i++)
      if (i >= nmons || unique[i].x_org != mon->mon_x ||
          unique[i].y_org != mon->mon_y || unique[i].width != mon->mon_w ||
          unique[i].height != mon->mon_h) {
        dirty = 1;
        mon->num = i;
        mon->mon_x = mon->win_x = unique[i].x_org;
        mon->mon_y = mon->win_y = unique[i].y_org;
        mon->mon_w = mon->win_w = unique[i].width;
        mon->mon_h = mon->win_h = unique[i].height;
        updatebarpos(mon);
      }
    /* drop monitors if there are now fewer screens than monitors */
    for (i = nscreens; i < nmons; i++) {
      for (mon = monitors; mon && mon->next; mon = mon->next)
        ;
      while ((client = mon->clients)) {
        dirty = 1;
        mon->clients = client->next;
        detachstack(client);
        client->mon = monitors;
        attach(client);
        attachstack(client);
      }
      if (mon == sel_mon)
        sel_mon = monitors;
      // if(showsystray&&systray){
      //  Client *ni;
      //  for (ni = systray->icons; ni; ni=ni->next)
      //    if(ni->mon == mon)
      //      ni->mon = monitors;
      // }
      cleanupmon(mon);
    }
    free(unique);
  } else
#endif /* XINERAMA */
  {    /* default monitor setup */
    if (!monitors)
      monitors = createmon();
    if (monitors->mon_w != screen_w || monitors->mon_h != screen_h) {
      dirty = 1;
      monitors->mon_w = monitors->win_w = screen_w;
      monitors->mon_h = monitors->win_h = screen_h;
      updatebarpos(monitors);
    }
  }
  if (dirty) {
    sel_mon = monitors;
    sel_mon = wintomon(root);
  }
  return dirty;
}

/* work out which modifier bit is Num Lock */
void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlock_mask = 0;
  modmap = XGetModifierMapping(display);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(display, XK_Num_Lock))
        numlock_mask = (1 << i);
  XFreeModifiermap(modmap);
}

/* read a client's WM size hints into its Client struct */
void updatesizehints(Client *client) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(display, client->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    client->base_w = size.base_width;
    client->base_h = size.base_height;
  } else if (size.flags & PMinSize) {
    client->base_w = size.min_width;
    client->base_h = size.min_height;
  } else
    client->base_w = client->base_h = 0;
  if (size.flags & PResizeInc) {
    client->inc_w = size.width_inc;
    client->inc_h = size.height_inc;
  } else
    client->inc_w = client->inc_h = 0;
  if (size.flags & PMaxSize) {
    client->max_w = size.max_width;
    client->max_h = size.max_height;
  } else
    client->max_w = client->max_h = 0;
  if (size.flags & PMinSize) {
    client->min_w = size.min_width;
    client->min_h = size.min_height;
  } else if (size.flags & PBaseSize) {
    client->min_w = size.base_width;
    client->min_h = size.base_height;
  } else
    client->min_w = client->min_h = 0;
  if (size.flags & PAspect) {
    client->min_aspect = (float)size.min_aspect.y / size.min_aspect.x;
    client->max_aspect = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    client->max_aspect = client->min_aspect = 0.0;
  client->is_fixed =
      (client->max_w && client->max_h && client->max_w == client->min_w &&
       client->max_h == client->min_h);
  client->hints_valid = 1;
}

/* refresh the status text from the root window name and redraw the bar */
void updatestatus(void) {
  if (!gettextprop(root, XA_WM_NAME, status_text, sizeof(status_text)))
    strcpy(status_text, "dwm-" VERSION);
  drawbar(sel_mon);
  updatesystray();
}

/* compute a systray icon's size within the tray */
void updatesystrayicongeom(Client *i, int w, int h) {
  int rh = bar_h - vertpadbar;
  if (i) {
    i->h = rh;
    if (w == h)
      i->w = rh;
    else if (h == rh)
      i->w = w;
    else
      i->w = (int)((float)rh * ((float)w / (float)h));
    i->y = i->y + vertpadbar / 2;
    applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
    /* force icons into the systray dimensions if they don't want to */
    if (i->h > rh) {
      if (i->w == i->h)
        i->w = rh;
      else
        i->w = (int)((float)rh * ((float)i->w / (float)i->h));
      i->h = rh;
    }
  }
}

/* map or unmap a systray icon following its XEMBED state */
void updatesystrayiconstate(Client *i, XPropertyEvent *ev) {
  long flags;
  int code = 0;

  if (!showsystray || !i || ev->atom != xembed_atom[XembedInfo] ||
      !(flags = getatomprop(i, xembed_atom[XembedInfo])))
    return;

  if (flags & XEMBED_MAPPED && !i->tags) {
    i->tags = 1;
    code = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(display, i->win);
    setclientstate(i, NormalState);
  } else if (!(flags & XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    code = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(display, i->win);
    setclientstate(i, WithdrawnState);
  } else
    return;
  sendevent(i->win, xembed_atom[Xembed], StructureNotifyMask, CurrentTime, code,
            0, systray->win, XEMBED_EMBEDDED_VERSION);
}

/* lay out and redraw the system tray and its icons */
void updatesystray(void) {
  XSetWindowAttributes wa;
  XWindowChanges wc;
  Client *i;
  Monitor *mon = systraytomon(NULL);
  unsigned int x = floatbar ? mon->mon_x + mon->mon_w - mon->gappov
                            : mon->mon_x + mon->mon_w;
  unsigned int w = 1;

  if (!showsystray)
    return;
  if (!systray) {
    /* init systray */
    if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
      die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
    systray->win = XCreateSimpleWindow(display, root, x, mon->bar_y, w, bar_h,
                                       0, 0, scheme[SchemeSel][ColBg].pixel);
    wa.event_mask = ButtonPressMask | ExposureMask;
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    XSelectInput(display, systray->win, SubstructureNotifyMask);
    XChangeProperty(display, systray->win, net_atom[NetSystemTrayOrientation],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&net_atom[NetSystemTrayOrientationHorz],
                    1);
    XChangeWindowAttributes(display, systray->win,
                            CWEventMask | CWOverrideRedirect | CWBackPixel,
                            &wa);
    XMapRaised(display, systray->win);
    XSetSelectionOwner(display, net_atom[NetSystemTray], systray->win,
                       CurrentTime);
    if (XGetSelectionOwner(display, net_atom[NetSystemTray]) == systray->win) {
      sendevent(root, xembed_atom[Manager], StructureNotifyMask, CurrentTime,
                net_atom[NetSystemTray], systray->win, 0, 0);
      XSync(display, False);
    } else {
      fprintf(stderr, "dwm: unable to obtain system tray.\n");
      free(systray);
      systray = NULL;
      return;
    }
  }
  for (w = 0, i = systray->icons; i; i = i->next) {
    /* make sure the background color stays the same */
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    XChangeWindowAttributes(display, i->win, CWBackPixel, &wa);
    XMapRaised(display, i->win);
    w += systrayspacing;
    i->x = w;
    XMoveResizeWindow(display, i->win, i->x, vertpadbar / 2, i->w, i->h);
    w += i->w;
    if (i->mon != mon)
      i->mon = mon;
  }
  w = w ? w + systrayspacing : 1;
  x -= w;
  XMoveResizeWindow(display, systray->win, x, mon->bar_y, w, bar_h);
  wc.x = x;
  wc.y = mon->bar_y;
  wc.width = w;
  wc.height = bar_h;
  wc.stack_mode = Above;
  wc.sibling = mon->bar_win;
  XConfigureWindow(display, systray->win,
                   CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode,
                   &wc);
  XMapWindow(display, systray->win);
  XMapSubwindows(display, systray->win);
  /* redraw background */
  XSetForeground(display, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(display, systray->win, drw->gc, 0, 0, w, bar_h);
  XSync(display, False);
}

/* refresh a client's cached window title */
void updatetitle(Client *client) {
  if (!gettextprop(client->win, net_atom[NetWMName], client->name,
                   sizeof client->name))
    gettextprop(client->win, XA_WM_NAME, client->name, sizeof client->name);
  if (client->name[0] == '\0') /* hack to mark broken clients */
    strcpy(client->name, broken);
}

/* apply behaviour from a window's type (e.g. dialog, fullscreen) */
void updatewindowtype(Client *client) {
  Atom state = getatomprop(client, net_atom[NetWMState]);
  Atom wtype = getatomprop(client, net_atom[NetWMWindowType]);

  if (state == net_atom[NetWMFullscreen])
    setfullscreen(client, 1);
  if (wtype == net_atom[NetWMWindowTypeDialog]) {
    client->is_centered = 1;
    client->is_floating = 1;
  }
}

/* read a client's WM hints (urgency, input-focus model) */
void updatewmhints(Client *client) {
  XWMHints *wmh;

  if ((wmh = XGetWMHints(display, client->win))) {
    if (client == sel_mon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(display, client->win, wmh);
    } else
      client->is_urgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      client->never_focus = !wmh->input;
    else
      client->never_focus = 0;
    XFree(wmh);
  }
}

/* switch the current monitor to the given tag(s) */
void view(const Arg *arg) {
  int i;
  unsigned int tmptag;

  if ((arg->ui & TAGMASK) == sel_mon->tagset[sel_mon->sel_tags])
    return;
  switchtag();
  sel_mon->sel_tags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK) {
    sel_mon->pertag->prev_tag = sel_mon->pertag->cur_tag;
    sel_mon->tagset[sel_mon->sel_tags] = arg->ui & TAGMASK;

    if (arg->ui == ~0)
      sel_mon->pertag->cur_tag = 0;
    else {
      for (i = 0; !(arg->ui & 1 << i); i++)
        ;
      sel_mon->pertag->cur_tag = i + 1;
    }
  } else {
    tmptag = sel_mon->pertag->prev_tag;
    sel_mon->pertag->prev_tag = sel_mon->pertag->cur_tag;
    sel_mon->pertag->cur_tag = tmptag;
  }

  sel_mon->nmaster = sel_mon->pertag->nmasters[sel_mon->pertag->cur_tag];
  sel_mon->mfact = sel_mon->pertag->mfacts[sel_mon->pertag->cur_tag];
  sel_mon->sel_layout = sel_mon->pertag->sel_layouts[sel_mon->pertag->cur_tag];
  sel_mon->layout[sel_mon->sel_layout] =
      sel_mon->pertag
          ->layout_idxs[sel_mon->pertag->cur_tag][sel_mon->sel_layout];
  sel_mon->layout[sel_mon->sel_layout ^ 1] =
      sel_mon->pertag
          ->layout_idxs[sel_mon->pertag->cur_tag][sel_mon->sel_layout ^ 1];

  if (sel_mon->showbar != sel_mon->pertag->showbars[sel_mon->pertag->cur_tag])
    togglebar(NULL);
  focus(NULL);
  arrange(sel_mon);
  updatecurrentdesktop();
}

/* find the managed client that owns a window */
Client *wintoclient(Window w) {
  Client *client;
  Monitor *mon;

  for (mon = monitors; mon; mon = mon->next)
    for (client = mon->clients; client; client = client->next)
      if (client->win == w)
        return client;
  return NULL;
}

/* find the systray icon that owns a window */
Client *wintosystrayicon(Window w) {
  Client *i = NULL;

  if (!showsystray || !systray || !w)
    return i;
  for (i = systray->icons; i && i->win != w; i = i->next)
    ;
  return i;
}

/* find the monitor a window (or the pointer) is on */
Monitor *wintomon(Window w) {
  int x, y;
  Client *client;
  Monitor *mon;

  if (w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (mon = monitors; mon; mon = mon->next)
    if (w == mon->bar_win || w == mon->tab_win)
      return mon;
  if ((client = wintoclient(w)))
    return client->mon;
  return sel_mon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *display, XErrorEvent *ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle &&
       ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
      (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
          ee->request_code, ee->error_code);
  return orig_xerror_handler(display, ee); /* may call exit */
}

/* X error handler that ignores everything */
int xerrordummy(Display *display, XErrorEvent *ee) { return 0; }

/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *display, XErrorEvent *ee) {
  die("dwm: another window manager is already running");
  return -1;
}

/* return the monitor the system tray should live on */
Monitor *systraytomon(Monitor *mon) {
  Monitor *t;
  int i, n;
  if (!systraypinning) {
    if (!mon)
      return sel_mon;
    return mon == sel_mon ? mon : NULL;
  }
  for (n = 1, t = monitors; t && t->next; n++, t = t->next)
    ;
  for (i = 1, t = monitors; t && t->next && i < systraypinning;
       i++, t = t->next)
    ;
  if (systraypinningfailfirst && n < systraypinning)
    return monitors;
  return t;
}

/* swap the selected client with the master (or promote the next one) */
void zoom(const Arg *arg) {
  Client *client = sel_mon->sel;

  if (!sel_mon->layout[sel_mon->sel_layout]->arrange || !client ||
      client->is_floating)
    return;
  if (client == nexttiled(sel_mon->clients) &&
      !(client = nexttiled(client->next)))
    return;
  pop(client);
}

/* parse args, open the display, set up, scan, run the loop, then clean up */
int main(int argc, char *argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-" VERSION);
  else if (argc != 1 && strcmp("-s", argv[1]))
    die("usage: dwm [-v]");
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(display = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  if (!(xcb_conn = XGetXCBConnection(display)))
    die("dwm: cannot get xcb connection\n");
  if (argc > 1 && !strcmp("-s", argv[1])) {
    XStoreName(display, RootWindow(display, DefaultScreen(display)), argv[2]);
    XCloseDisplay(display);
    return 0;
  }
  checkotherwm();
  setup();
#ifdef __OpenBSD__
  // if (pledge("stdio rpath proc exec", NULL) == -1)
  if (pledge("stdio rpath proc exec ps", NULL) == -1)
    die("pledge");
#endif /* __OpenBSD__ */
  scan();
  run();
  cleanup();
  if (restart)
    execvp(argv[0], argv);
  XCloseDisplay(display);
  return EXIT_SUCCESS;
}

/* focus the first client in the master area */
void focusmaster(const Arg *arg) {
  Client *client;

  if (sel_mon->nmaster < 1)
    return;

  client = nexttiled(sel_mon->clients);

  if (client)
    focus(client);
}
