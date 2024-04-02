/* See LICENSE file for copyright and license details. */

#include <X11/X.h>
#include <X11/XF86keysym.h>

/* brightness control */
/* static const char *upbri[] = {"/home/bear/.dwm/screenlight.sh", "up", NULL}; */
/* static const char *downbri[] = {"/home/bear/.dwm/screenlight.sh", "down", NULL}; */
/*volume control */
/* static const char *upvol[] = {"/home/bear/.dwm/volume.sh", "up", NULL}; */
/* static const char *downvol[] = {"/home/bear/.dwm/volume.sh", "down", NULL}; */
/* static const char *mutevol[] = {"/home/bear/.dwm/volume.sh", "mute", NULL}; */

//swallow patch
static const int swallowfloating    = 0;        /* 1 means swallow floating windows by default */
//riopdraw consts
static const char slopspawnstyle[]  = "-D -l -c 0.3,0.4,0.6,0.4"; /* do NOT define -f (format) here */
static const char slopresizestyle[] = "-D -l -c 0.3,0.4,0.6,0.4"; /* do NOT define -f (format) here */
static const int riodraw_borders    = 0;        /* 0 or 1, indicates whether the area drawn using slop includes the window borders */
static const int riodraw_matchpid   = 1;        /* 0 or 1, indicates whether to match the PID of the client that was spawned with riospawn */
static const int riodraw_spawnasync = 0;        /* 0 means that the application is only spawned after a successful selection while*/
static const char dmenufont[]       = "JetBrainsMono Nerd Font:size=10";
static const unsigned int default_gaps = 0;
/* appearance */
static const unsigned int borderpx  = 3;        /* border pixel of windows */
static const unsigned int default_border = 0;   /* to switch back to default border after dynamic border resizing via keybinds */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int gappih    = default_gaps;       /* horiz inner gap between windows */
static const unsigned int gappiv    = default_gaps;       /* vert inner gap between windows */
static const unsigned int gappoh    = default_gaps;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov    = default_gaps;       /* vert outer gap between windows and screen edge */
static const int smartgaps          = 1;        /* 1 means no outer gap when there is only one window */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails,display systray on the 1st monitor,False: display systray on last monitor*/
static const int showsystray        = 1;        /* 0 means no systray */
static const int showbar            = 1;        /* 0 means no bar */
static const int showtab            = showtab_auto;
static const int toptab             = 0;        /* 0 means bottom tab */
static const int floatbar           = 0;/* 1 means the bar will float(don't have padding),0 means the bar have padding */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int focusonwheel       = 0;
/* static const int horizpadbar        = 5; */
/* static const int vertpadbar         = 11; */
static const int horizpadbar        = 0;
static const int vertpadbar         = 5;
static const int vertpadtab         = 35;
static const int horizpadtabi       = 15;
static const int horizpadtabo       = 15;
static const int scalepreview       = 4;
static const int tag_preview        = 1;        /* 1 means enable, 0 is off */
static const int colorfultag        = 1;        /* 0 means use SchemeSel for selected non vacant tag */
static const int new_window_attach_on_end = 0; /*  1 means the new window will attach on the end; 0 means the new window will attach on the front,default is front */
#define ICONSIZE 19   /* icon size */
#define ICONSPACING 8 /* space between icon and title */

static const char *fonts[]          = {"Noto Sans:style:medium:size=12" ,"JetBrainsMono Nerd Font Mono:style:medium:size=19" };
/* static const char *fonts[]          = {"JetBrainsMono Nerd Font Mono:style:medium:size=19" }; */

// theme
#include "themes/gruvchad.h"

static const char *colors[][3]      = {
    /*                     fg       bg      border */
    [SchemeNorm]       = { gray3,   black,  gray2 },
    [SchemeSel]        = { gray4,   blue,   blue  },
    [SchemeTitle]      = { white,   black,  black  }, // active window title
    [TabSel]           = { blue,    gray2,  black },
    [TabNorm]          = { gray3,   black,  black },
    [SchemeTag]        = { gray3,   black,  black },
    [SchemeTag1]       = { blue,    black,  black },
    [SchemeTag2]       = { red,     black,  black },
    [SchemeTag3]       = { orange,  black,  black },
    [SchemeTag4]       = { green,   black,  black },
    [SchemeTag5]       = { pink,    black,  black },
    [SchemeLayout]     = { green,   black,  black },
    [SchemeBtnPrev]    = { green,   black,  black },
    [SchemeBtnNext]    = { yellow,  black,  black },
    [SchemeBtnClose]   = { red,     black,  black },
};

/* tagging */
static char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "", "" };

static const char* eww[] = { "eww", "open" , "eww", NULL };

static const Launcher launchers[] = {
    /* command     name to display */
    { eww,         "󰣇" },
};

static const int tagschemes[] = {
    SchemeTag1, SchemeTag2, SchemeTag3, SchemeTag4, SchemeTag5, SchemeTag1, SchemeTag2, SchemeTag3, SchemeTag4,
};

static const unsigned int ulinepad      = 5; /* horizontal padding between the underline and tag */
static const unsigned int ulinestroke   = 2; /* thickness / height of the underline */
static const unsigned int ulinevoffset  = -4; /* how far above the bottom of the bar the line should appear */
static const int ulineall               = 0; /* 1 to show underline on all tags, 0 for just the active ones */

static const Rule rules[] = {
    /* xprop(1):
     *	WM_CLASS(STRING) = instance, class
     *	WM_NAME(STRING) = title
     */
    /* class      instance    title       tags mask     iscentered   isfloating   monitor */
    /* { "Gimp",     NULL,       NULL,       0,            0,           1,           -1 }, */
    /* { "Firefox",  NULL,       NULL,       1 << 8,       0,           0,           -1 }, */
    /* { "eww",      NULL,       NULL,       0,            0,           1,           -1 }, */

	/* class                   instance  title           tags mask  is centered isfloating  isterminal  noswallow  monitor */
	{ "Gimp",                  NULL,     NULL,           0,         0,           1,          0,           0,        -1 },
	{ "Firefox",               NULL,     NULL,           1 << 8,    0,           0,          0,          -1,        -1 },
	{ "Alacritty",             NULL,     NULL,           0,         0,           0,          1,           0,        -1 },
	{ "kitty",                 NULL,     NULL,           0,         0,           0,          1,           0,        -1 },
	{ "st-256color",           NULL,     NULL,           0,         0,           0,          1,           0,        -1 },
	{ "Evince",                NULL,     NULL,           0,         0,           0,          0,           1,        -1 },
	{NULL,                     NULL,     "Event Tester", 0,         0,           0,          0,           1,        -1 }, /* xev */
};

/* layout(s) */
static const float mfact     = 0.50; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */

#define FORCE_VSPLIT 1  /* nrowgrid layout: force two clients to always split vertically */
#include "functions.h"

static const Layout layouts[] = {
    /* symbol     arrange function */
    { "[]=",      tile },    /* first entry is default */
    { "[M]",      monocle },
    { "[@]",      spiral },
    { "[\\]",     dwindle },
    { "H[]",      deck },
    { "TTT",      bstack },
    { "===",      bstackhoriz },
    { "HHH",      grid },
    { "###",      nrowgrid },
    { "---",      horizgrid },
    { ":::",      gaplessgrid },
    { "|M|",      centeredmaster },
    { ">M>",      centeredfloatingmaster },
    { "><>",      NULL },    /* no layout function means floating behavior */
	{ "|+|",      tatami },
    { NULL,       NULL },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
    { MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
    { MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
    { MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },


/* commands */

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", gray2, "-nf", gray3, "-sb", green, "-sf", gray4, NULL };
static const char *termcmd[]  = { "alacritty", NULL };

static const Key keys[] = {
    /* modifier                         key         function        argument */
    // screenshot fullscreen and cropped
    /* {MODKEY|ControlMask,                XK_u,       spawn,        SHCMD("maim -u -D -l -c 0.3,0.4,0.6,0.4 | xclip -selection clipboard -t image/png")}, */
    /* {MODKEY,                            XK_u,       spawn,        SHCMD("maim -u --select -D -l -c 0.3,0.4,0.6,0.4 | xclip -selection clipboard -t image/png")}, */
    /* { MODKEY|ShiftMask,                 XK_Return,  spawn,            SHCMD("alacritty")}, */
	/* { MODKEY,                       XK_p,      spawn,          SHCMD("rofi -show run -config ~/.config/chadwm/rofi/config.rasi") }, */
	/* { MODKEY|ShiftMask,             XK_p,      spawn,          SHCMD("rofi -show drun -config ~/.config/chadwm/rofi/config.rasi") }, */
    //https://github.com/adi1090x/rofi
	{ MODKEY,                       XK_p,      spawn,          SHCMD("~/.config/rofi/scripts/launcher_t6") },
	{ MODKEY|ShiftMask,             XK_p,      spawn,          SHCMD("~/.config/rofi/scripts/launcher_t6 r") },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },

	{ MODKEY|ControlMask,           XK_Return, riospawn,       {.v = termcmd } },
	{ MODKEY,                       XK_r,      rioresize,      {0} },

    // toggle stuff
    { MODKEY,                           XK_b,       togglebar,      {0} },
    { MODKEY|ControlMask,               XK_t,       togglegaps,     {0} },
    { MODKEY|ShiftMask,                 XK_space,   togglefloating, {0} },
    { MODKEY,                           XK_f,       togglefullscr,  {0} },
    { MODKEY|ControlMask,               XK_w,       tabmode,        { -1 } },
    { MODKEY,                           XK_j,       focusstack,     {.i = +1 } },
    { MODKEY,                           XK_k,       focusstack,     {.i = -1 } },
    { MODKEY,                           XK_i,       incnmaster,     {.i = +1 } },
    { MODKEY,                           XK_d,       incnmaster,     {.i = -1 } },

    // shift view
    { MODKEY,                           XK_Left,    shiftview,      {.i = -1 } },
    { MODKEY,                           XK_Right,   shiftview,      {.i = +1 } },
    { MODKEY|ControlMask,                           XK_j,    shiftview,      {.i = -1 } },
    { MODKEY|ControlMask,                           XK_k,   shiftview,      {.i = +1 } },

    // change m,cfact sizes 
    { MODKEY,                           XK_h,       setmfact,       {.f = -0.05} },
    { MODKEY,                           XK_l,       setmfact,       {.f = +0.05} },
    { MODKEY,                           XK_o,       setmfact,       {.f =  0.00} },
    { MODKEY|ShiftMask,                 XK_h,       setcfact,       {.f = +0.25} },
    { MODKEY|ShiftMask,                 XK_l,       setcfact,       {.f = -0.25} },
    { MODKEY|ShiftMask,                 XK_o,       setcfact,       {.f =  0.00} },
    { MODKEY|ShiftMask,                 XK_j,       movestack,      {.i = +1 } },
    { MODKEY|ShiftMask,                 XK_k,       movestack,      {.i = -1 } },
    { MODKEY,                           XK_Return,  zoom,           {0} },
    { MODKEY,                           XK_Tab,     view,           {0} },

    // overall gaps
    /* { MODKEY|ControlMask,               XK_i,       incrgaps,       {.i = +1 } }, */
    /* { MODKEY|ControlMask,               XK_d,       incrgaps,       {.i = -1 } }, */
	/* { MODKEY|ShiftMask,             XK_minus,  setgaps,        {.i = GAP_RESET } }, */
	/* { MODKEY|ShiftMask,             XK_equal,  setgaps,        {.i = GAP_TOGGLE} }, */
	{ MODKEY,                       XK_minus,  incrgaps,        {.i = -5 } },
	{ MODKEY,                       XK_equal,  incrgaps,        {.i = +5 } },
	{ MODKEY|ControlMask,           XK_o,      setgaps,         {.i = 0 } },

    // change border size
    { MODKEY|ShiftMask,                 XK_minus,   setborderpx,    {.i = -1 } },
    { MODKEY|ShiftMask,                 XK_equal,   setborderpx,    {.i = +1 } },
    { MODKEY|ControlMask|ShiftMask,     XK_o,       setborderpx,    {.i = default_border } },

    // layout
    { MODKEY,                           XK_t,       setlayout,      {.v = &layouts[0]} },
    { MODKEY|ShiftMask,                 XK_f,       setlayout,      {.v = &layouts[2]} },
    { MODKEY,                           XK_m,       setlayout,      {.v = &layouts[1]} },
    { MODKEY,                           XK_d,       setlayout,      {.v = &layouts[4]} },
    /* { MODKEY|ControlMask,               XK_g,       setlayout,      {.v = &layouts[10]} }, */
    { MODKEY|ControlMask|ShiftMask,     XK_t,       setlayout,      {.v = &layouts[13]} },
    { MODKEY|ControlMask|ShiftMask,     XK_y,       setlayout,      {.v = &layouts[14]} },
    { MODKEY,                           XK_space,   setlayout,      {0} },
    { MODKEY|ControlMask,               XK_comma,   cyclelayout,    {.i = -1 } },
    { MODKEY|ControlMask,               XK_period,  cyclelayout,    {.i = +1 } },
    { MODKEY,                           XK_0,       view,           {.ui = ~0 } },
    /* this keybind causes crashes idk */
    /* { MODKEY|ShiftMask,                 XK_0,       tag,            {.ui = ~0 } }, */
    { MODKEY,                           XK_comma,   focusmon,       {.i = -1 } },
    { MODKEY,                           XK_period,  focusmon,       {.i = +1 } },
    { MODKEY|ShiftMask,                 XK_comma,   tagmon,         {.i = -1 } },
    { MODKEY|ShiftMask,                 XK_period,  tagmon,         {.i = +1 } },

    // kill dwm
    //default kill replaced with kill from exit dwm patch
    /* { MODKEY|ControlMask|ShiftMask,     XK_q,       quit,           {0} }, */
	{ MODKEY|ShiftMask,             XK_q,      spawn,          SHCMD("~/.config/rofi/scripts/powermenu_t3") },

    // kill window
    { MODKEY,                           XK_q,       killclient,     {0} },

    // hide & restore windows
    { MODKEY,                           XK_e,       hidewin,        {0} },
    { MODKEY|ShiftMask,                 XK_e,       restorewin,     {0} },

    TAGKEYS(                            XK_1,                       0)
    TAGKEYS(                            XK_2,                       1)
    TAGKEYS(                            XK_3,                       2)
    TAGKEYS(                            XK_4,                       3)
    TAGKEYS(                            XK_5,                       4)
    TAGKEYS(                            XK_6,                       5)
    TAGKEYS(                            XK_7,                       6)
    TAGKEYS(                            XK_8,                       7)
    TAGKEYS(                            XK_9,                       8)


    /* audio keys */
    /* { 0,                       XF86XK_AudioLowerVolume, spawn, {.v = downvol } }, */
    /* { 0,                       XF86XK_AudioMute, spawn, {.v = mutevol } }, */
    /* { 0,                       XF86XK_AudioRaiseVolume, spawn, {.v = upvol   } }, */
    /*brightness control keys */
    /* { 0, XF86XK_MonBrightnessUp, spawn, {.v = upbri}}, */
    /* { 0, XF86XK_MonBrightnessDown, spawn, {.v = downbri}}, */

    //focusmaster
    { MODKEY|ControlMask,           XK_space,  focusmaster,    {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
    /* click                event mask      button          function        argument */
    { ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
    { ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
    { ClkWinTitle,          0,              Button2,        zoom,           {0} },
    { ClkStatusText,        0,              Button2,        spawn,          SHCMD("st") },

    /* Keep movemouse? */
    /* { ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} }, */

    /* placemouse options, choose which feels more natural:
    *    0 - tiled position is relative to mouse cursor
    *    1 - tiled position is relative to window center
    *    2 - mouse pointer warps to window center
    *
    * The moveorplace uses movemouse or placemouse depending on the floating state
    * of the selected client. Set up individual keybindings for the two if you want
    * to control these separately (i.e. to retain the feature to move a tiled window
    * into a floating position).
    */
    { ClkClientWin,         MODKEY,         Button1,        moveorplace,    {.i = 0} },
    { ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
    { ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
    /* { ClkClientWin,         ControlMask,    Button1,        dragmfact,      {0} }, */
    /* { ClkClientWin,         ControlMask,    Button3,        dragcfact,      {0} }, */
    { ClkTagBar,            0,              Button1,        view,           {0} },
    { ClkTagBar,            0,              Button3,        toggleview,     {0} },
    { ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
    { ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
    { ClkTabBar,            0,              Button1,        focuswin,       {0} },
    { ClkTabBar,            0,              Button1,        focuswin,       {0} },
    { ClkTabPrev,           0,              Button1,        movestack,      { .i = -1 } },
    { ClkTabNext,           0,              Button1,        movestack,      { .i = +1 } },
    { ClkTabClose,          0,              Button1,        killclient,     {0} },
};

    //commented out resize functions to avoid confusion
    /* // inner gaps */
    /* { MODKEY|ShiftMask,                 XK_i,       incrigaps,      {.i = +1 } }, */
    /* { MODKEY|ControlMask|ShiftMask,     XK_i,       incrigaps,      {.i = -1 } }, */
    /**/
    /* // outer gaps */
    /* { MODKEY|ControlMask,               XK_o,       incrogaps,      {.i = +1 } }, */
    /* { MODKEY|ControlMask|ShiftMask,     XK_o,       incrogaps,      {.i = -1 } }, */
    /* // inner+outer hori, vert gaps  */
    /* { MODKEY|ControlMask,               XK_6,       incrihgaps,     {.i = +1 } }, */
    /* { MODKEY|ControlMask|ShiftMask,     XK_6,       incrihgaps,     {.i = -1 } }, */
    /* { MODKEY|ControlMask,               XK_7,       incrivgaps,     {.i = +1 } }, */
    /* { MODKEY|ControlMask|ShiftMask,     XK_7,       incrivgaps,     {.i = -1 } }, */
    /* { MODKEY|ControlMask,               XK_8,       incrohgaps,     {.i = +1 } }, */
    /* { MODKEY|ControlMask|ShiftMask,     XK_8,       incrohgaps,     {.i = -1 } }, */
    /* { MODKEY|ControlMask,               XK_9,       incrovgaps,     {.i = +1 } }, */
    /* { MODKEY|ControlMask|ShiftMask,     XK_9,       incrovgaps,     {.i = -1 } }, */
    /**/
    /* { MODKEY|ControlMask|ShiftMask,     XK_d,       defaultgaps,    {0} }, */


/* dwmc patch */
void
setlayoutex(const Arg *arg)
{
	setlayout(&((Arg) { .v = &layouts[arg->i] }));
}

void
viewex(const Arg *arg)
{
	view(&((Arg) { .ui = 1 << arg->ui }));
}

void
viewall(const Arg *arg)
{
	view(&((Arg){.ui = ~0}));
}

void
toggleviewex(const Arg *arg)
{
	toggleview(&((Arg) { .ui = 1 << arg->ui }));
}

void
tagex(const Arg *arg)
{
	tag(&((Arg) { .ui = 1 << arg->ui }));
}

void
toggletagex(const Arg *arg)
{
	toggletag(&((Arg) { .ui = 1 << arg->ui }));
}

void
tagall(const Arg *arg)
{
	tag(&((Arg){.ui = ~0}));
}

/* signal definitions */
/* signum must be greater than 0 */
/* trigger signals using `xsetroot -name "fsignal:<signame> [<type> <value>]"` */
static Signal signals[] = {
	/* signum           function */
	{ "focusstack",     focusstack },
	{ "setmfact",       setmfact },
	{ "togglebar",      togglebar },
	{ "incnmaster",     incnmaster },
	{ "togglefloating", togglefloating },
	{ "focusmon",       focusmon },
	{ "tagmon",         tagmon },
	{ "zoom",           zoom },
	{ "view",           view },
	{ "viewall",        viewall },
	{ "viewex",         viewex },
	{ "toggleview",     view },
	{ "toggleviewex",   toggleviewex },
	{ "tag",            tag },
	{ "tagall",         tagall },
	{ "tagex",          tagex },
	{ "toggletag",      tag },
	{ "toggletagex",    toggletagex },
	{ "killclient",     killclient },
	{ "quit",           quit },
	{ "setlayout",      setlayout },
	{ "setlayoutex",    setlayoutex },
};
