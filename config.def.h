/* See LICENSE file for copyright and license details. */

/* font */
static const char font[]                = "Terminus 12";

/* border colors */
static const char border_color[]        = "#333333";
static const char sel_border_color[]    = "#FF0000";

/* tag colors */
static const char tag_bg_color[]        = "#131313";
static const char tag_fg_color[]        = "#777777";
static const char sel_tag_bg_color[]    = "#131313";
static const char sel_tag_fg_color[]    = "#80C0FF";

/* layout symbol colors */
static const char ltsym_bg_color[]      = "#131313";
static const char ltsym_fg_color[]      = "#C0FF80";

/* title colors */
static const char title_bg_color[]      = "#131313";
static const char title_fg_color[]      = "#FFFFFF";

/* status colors */
static const char status_bg_color[]     = "#131313";
static const char status_fg_color[]     = "#98AAB2";

/* general settings */
static const int nmaster                = 1;        /* Number of windows in master area */
static const float mfact                = 0.7;      /* Master area size */
static const int border_thickness       = 1;        /* Window border thickness */
static const int snap                   = 5;        /* Window snap threshold */
static const Bool focus_follows_mouse   = False;    /* Always focus window under cursor */
static const Bool showbar               = True;     /* Bar visibility */
static const Bool topbar                = True;     /* Invert bar position */
static const Bool resizehints           = False;    /* Respect window size hints */

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
    /* class      instance    title       tags mask     isfloating   monitor */
    { "Gimp",     NULL,       NULL,       0,            True,        -1 },
};

/* layout(s) */
static const Layout layouts[] = {
    { "Stack", stack },
    { "Tile", tile },
    { "Monacle", monocle },
    { "Float", NULL },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *dmenucmd[] = { "dmenu_run", "-b", "-fn", "-*-terminus-*-*-*-*-17-*-*-*-*-*-*-*", "-nb", tag_bg_color, "-nf", tag_fg_color, "-sb", sel_tag_bg_color, "-sf", sel_tag_fg_color, NULL };
static const char *termcmd[]  = { "roxterm", NULL };

static Key keys[] = {
    /* modifier                     key        function        argument */
    { MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
    { MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },

    { MODKEY,                       XK_b,      togglebar,      {0} },
    { MODKEY,                       XK_w,      toggleborders,  {0} },

    { MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
    { MODKEY,                       XK_k,      focusstack,     {.i = -1 } },

    { MODKEY|ShiftMask,             XK_j,      pushdown,       {0} },
    { MODKEY|ShiftMask,             XK_k,      pushup,         {0} },

    { MODKEY,                       XK_a,      setnmaster,     {.i = +1} },
    { MODKEY,                       XK_z,      setnmaster,     {.i = -1} },
    { MODKEY,                       XK_x,      resetnmaster,   {0} },

    { MODKEY,                       XK_h,      setmfact,       {.f = -0.02} },
    { MODKEY,                       XK_l,      setmfact,       {.f = +0.02} },
    { MODKEY,                       XK_i,      resetmfact,     {0} },

    { MODKEY,                       XK_Return, zoom,           {0} },
    { MODKEY,                       XK_Tab,    view,           {0} },
    { MODKEY|ShiftMask,             XK_c,      killclient,     {0} },

    { MODKEY,                       XK_s,      setlayout,      {.v = &layouts[0]} },
    { MODKEY,                       XK_t,      setlayout,      {.v = &layouts[1]} },
    { MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },

    { MODKEY,                       XK_space,  togglefloating, {0} },

    { MODKEY,                       XK_0,      view,           {.ui = ~0 } },
    { MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },

    { MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
    { MODKEY,                       XK_period, focusmon,       {.i = +1 } },

    { MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
    { MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },

    TAGKEYS(                        XK_1,                      0),
    TAGKEYS(                        XK_2,                      1),
    TAGKEYS(                        XK_3,                      2),
    TAGKEYS(                        XK_4,                      3),
    TAGKEYS(                        XK_5,                      4),
    TAGKEYS(                        XK_6,                      5),
    TAGKEYS(                        XK_7,                      6),
    TAGKEYS(                        XK_8,                      7),
    TAGKEYS(                        XK_9,                      8),

    { MODKEY|ShiftMask,             XK_q,      quit,           {0} },
};

/* button definitions */
static Button buttons[] = {
    /* click            event mask      button          function        argument */
    { ClickLtSym,       0,              Button1,        setlayout,      {0} },
    { ClickLtSym,       0,              Button3,        setlayout,      {.v = &layouts[2]} },
    { ClickTitle,       0,              Button2,        zoom,           {0} },
    { ClickStatus,      0,              Button2,        spawn,          {.v = termcmd } },
    { ClickClient,      MODKEY,         Button1,        movemouse,      {0} },
    { ClickClient,      MODKEY,         Button2,        togglefloating, {0} },
    { ClickClient,      MODKEY,         Button3,        resizemouse,    {0} },
    { ClickTagStrip,    0,              Button1,        view,           {0} },
    { ClickTagStrip,    0,              Button3,        toggleview,     {0} },
    { ClickTagStrip,    MODKEY,         Button1,        tag,            {0} },
    { ClickTagStrip,    MODKEY,         Button3,        toggletag,      {0} },
};
