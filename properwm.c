/* See LICENSE file for copyright and license details. */

#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <X11/Xft/Xft.h>
#include <pango/pango.h>
#include <pango/pangoxft.h>
#include <pango/pango-font.h>

#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask))
#define INRECT(X,Y,RX,RY,RW,RH) ((X) >= (RX) && (X) < (RX) + (RW) && (Y) >= (RY) && (Y) < (RY) + (RH))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])

#ifndef MAX
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#endif

#ifndef MIN
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#endif

#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (textnw(X, strlen(X)) + dc.font.height)

/* cursors */
enum { CursorNormal, CursorResize, CursorMove, CursorLast };

/* colors */
enum {
    ColorBorder,
    ColorTagFG, ColorTagBG,
    ColorLtSymFG, ColorLtSymBG,
    ColorTitleBG, ColorTitleFG,
    ColorStatusFG, ColorStatusBG,
    ColorLast
};

/* color draw types */
enum {
    DrawTag,
    DrawLtSym,
    DrawTitle,
    DrawStatus
};

/* EWMH atoms */
enum { NetSupported, NetWMName, NetWMState,
       NetWMFullscreen, NetLast };

/* default atoms */
enum { WMProtocols, WMDelete, WMState, WMLast };

/* clicks */
enum { ClickTagStrip, ClickLtSym, ClickTitle, ClickStatus,
       ClickClient, ClickRootWin, ClickLast };

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
    float mina, maxa;
    int x, y, w, h;
    int oldx, oldy, oldw, oldh;
    int basew, baseh, incw, inch, maxw, maxh, minw, minh;
    int bw, oldbw;
    unsigned int tags;
    Bool isfixed, isfloating, isurgent, oldstate;
    Client *next;
    Client *snext;
    Monitor *mon;
    Window win;
};

typedef struct {
    int x, y, w, h;
    unsigned long norm[ColorLast];
    unsigned long sel[ColorLast];
    Drawable drawable;
    XftColor xftnorm[ColorLast];
    XftColor xftsel[ColorLast];
    XftDraw *xftdrawable;
    PangoContext *pgc;
    PangoLayout  *plo;
    PangoFontDescription *pfd;
    GC gc;
    struct {
        int ascent;
        int descent;
        int height;
    } font;
} DC; /* draw context */

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const Arg *);
    const Arg arg;
} Key;

typedef struct {
    const char *symbol;
    void (*arrange)(Monitor *);
} Layout;

typedef struct {
    const char *class;
    const char *instance;
    const char *title;
    unsigned int tags;
    Bool isfloating;
    int monitor;
} Rule;

/* function declarations */
static void applyrules(Client *c);
static Bool applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clearurgent(Client *c);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static void die(const char *errstr, ...);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawsquare(Bool filled, Bool empty, Bool invert, int drawtype, unsigned long col[ColorLast]);
static void drawtext(const char *text, int drawtype, unsigned long col[ColorLast], Bool invert);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static unsigned long getcolor(const char *colstr, XftColor *color);
static Bool getrootptr(int *x, int *y);
static long getstate(Window w);
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, Bool focused);
static void grabkeys(void);
static void initfont(const char *fontstr);
static Bool isprotodel(Client *c);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static Client *prevtiled(Client *c);
static void propertynotify(XEvent *e);
static Monitor *ptrtomon(int x, int y);
static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);
static void quit(const Arg *arg);
static void resetnmaster(const Arg *arg);
static void resetmfact(const Arg *arg);
static void resize(Client *c, int x, int y, int w, int h, Bool interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setlayout(const Arg *arg);
static void setnmaster(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void stack(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static int textnw(const char *text, unsigned int len);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unfocus(Client *c, Bool setfocus);
static void unmanage(Client *c, Bool destroyed);
static void unmapnotify(XEvent *e);
static Bool updategeom(void);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

/* variables */
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
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
    [PropertyNotify] = propertynotify,
    [UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static Bool otherwm;
static Bool running = True;
static Cursor cursor[CursorLast];
static Display *dpy;
static DC dc;
static Monitor *mons = NULL, *selmon = NULL;
static Window root;

#include "config.h"

struct Monitor {
    int num;
    int by;               /* bar geometry */
    int mx, my, mw, mh;   /* screen size */
    int wx, wy, ww, wh;   /* window area  */
    unsigned int seltags;
    unsigned int sellt;
    unsigned int tagset[2];

    Client *clients;
    Client *sel;
    Client *stack;

    Monitor *next;
    Window barwin;
    const Layout *lt[2];

    /* global */
    char ltsymbol[16];
    Bool showbar;
    Bool topbar;

    int curtag;
    int prevtag;

    /* per-tag */
    const Layout *lts[LENGTH(tags) + 1];
    float nmasters[LENGTH(tags) + 1];
    double mfacts[LENGTH(tags) + 1];
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* function implementations */
void
applyrules(Client *c) {
    const char *class, *instance;
    unsigned int i;
    const Rule *r;
    Monitor *m;
    XClassHint ch = { 0 };

    /* rule matching */
    c->isfloating = c->tags = 0;
    if (XGetClassHint(dpy, c->win, &ch)) {
        class = ch.res_class ? ch.res_class : broken;
        instance = ch.res_name ? ch.res_name : broken;
        for (i = 0; i < LENGTH(rules); i++) {
            r = &rules[i];
            if ((!r->title || strstr(c->name, r->title))
            && (!r->class || strstr(class, r->class))
            && (!r->instance || strstr(instance, r->instance)))
            {
                c->isfloating = r->isfloating;
                c->tags |= r->tags;
                for (m = mons; m && m->num != r->monitor; m = m->next);
                if (m)
                    c->mon = m;
            }
        }
        if (ch.res_class)
            XFree(ch.res_class);
        if (ch.res_name)
            XFree(ch.res_name);
    }
    c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

Bool
applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact) {
    Bool baseismin;
    Monitor *m = c->mon;

    /* set minimum possible */
    *w = MAX(1, *w);
    *h = MAX(1, *h);
    if (interact) {
        if (*x > sw)
            *x = sw - WIDTH(c);
        if (*y > sh)
            *y = sh - HEIGHT(c);
        if (*x + *w + 2 * c->bw < 0)
            *x = 0;
        if (*y + *h + 2 * c->bw < 0)
            *y = 0;
    }
    else {
        if (*x > m->mx + m->mw)
            *x = m->mx + m->mw - WIDTH(c);
        if (*y > m->my + m->mh)
            *y = m->my + m->mh - HEIGHT(c);
        if (*x + *w + 2 * c->bw < m->mx)
            *x = m->mx;
        if (*y + *h + 2 * c->bw < m->my)
            *y = m->my;
    }
    if (*h < bh)
        *h = bh;
    if (*w < bh)
        *w = bh;
    if (resizehints || c->isfloating) {
        /* see last two sentences in ICCCM 4.1.2.3 */
        baseismin = c->basew == c->minw && c->baseh == c->minh;
        if (!baseismin) { /* temporarily remove base dimensions */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for aspect limits */
        if (c->mina > 0 && c->maxa > 0) {
            if (c->maxa < (float)*w / *h)
                *w = *h * c->maxa + 0.5;
            else if (c->mina < (float)*h / *w)
                *h = *w * c->mina + 0.5;
        }
        if (baseismin) { /* increment calculation requires this */
            *w -= c->basew;
            *h -= c->baseh;
        }
        /* adjust for increment value */
        if (c->incw)
            *w -= *w % c->incw;
        if (c->inch)
            *h -= *h % c->inch;
        /* restore base dimensions */
        *w += c->basew;
        *h += c->baseh;
        *w = MAX(*w, c->minw);
        *h = MAX(*h, c->minh);
        if (c->maxw)
            *w = MIN(*w, c->maxw);
        if (c->maxh)
            *h = MIN(*h, c->maxh);
    }
    return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m) {
    if (m)
        showhide(m->stack);
    else for (m = mons; m; m = m->next)
        showhide(m->stack);
    focus(NULL);
    if (m)
        arrangemon(m);
    else for (m = mons; m; m = m->next)
        arrangemon(m);
}

void
arrangemon(Monitor *m) {
    strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
    if (m->lt[m->sellt]->arrange)
        m->lt[m->sellt]->arrange(m);
    restack(m);
}

void
attach(Client *c) {
    c->next = c->mon->clients;
    c->mon->clients = c;
}

void
attachstack(Client *c) {
    c->snext = c->mon->stack;
    c->mon->stack = c;
}

void
buttonpress(XEvent *e) {
    unsigned int i, x, click;
    Arg arg = {0};
    Client *c;
    Monitor *m;
    XButtonPressedEvent *ev = &e->xbutton;

    click = ClickRootWin;
    /* focus monitor if necessary */
    if ((m = wintomon(ev->window)) && m != selmon) {
        unfocus(selmon->sel, True);
        selmon = m;
        focus(NULL);
    }
    if (ev->window == selmon->barwin) {
        i = x = 0;
        do {
            x += TEXTW(tags[i]);
        } while(ev->x >= x && ++i < LENGTH(tags));
        if (i < LENGTH(tags)) {
            click = ClickTagStrip;
            arg.ui = 1 << i;
        }
        else if (ev->x < x + blw)
            click = ClickLtSym;
        else if (ev->x > selmon->wx + selmon->ww - TEXTW(stext))
            click = ClickStatus;
        else
            click = ClickTitle;
    }
    else if ((c = wintoclient(ev->window))) {
        focus(c);
        click = ClickClient;
    }
    for (i = 0; i < LENGTH(buttons); i++)
        if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
        && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
            buttons[i].func(click == ClickTagStrip && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void) {
    otherwm = False;
    xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
    XSync(dpy, False);
    if (otherwm)
        die("properwm: another window manager is already running\n");
    XSetErrorHandler(xerror);
    XSync(dpy, False);
}

void
cleanup(void) {
    Arg a = {.ui = ~0};
    Layout foo = { "", NULL };
    Monitor *m;

    view(&a);
    selmon->lt[selmon->sellt] = &foo;
    for (m = mons; m; m = m->next)
        while(m->stack)
            unmanage(m->stack, False);
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XFreePixmap(dpy, dc.drawable);
    XFreeGC(dpy, dc.gc);
    XFreeCursor(dpy, cursor[CursorNormal]);
    XFreeCursor(dpy, cursor[CursorResize]);
    XFreeCursor(dpy, cursor[CursorMove]);
    while(mons)
        cleanupmon(mons);
    XSync(dpy, False);
    XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void
cleanupmon(Monitor *mon) {
    Monitor *m;

    if (mon == mons)
        mons = mons->next;
    else {
        for (m = mons; m && m->next != mon; m = m->next);
        m->next = mon->next;
    }
    XUnmapWindow(dpy, mon->barwin);
    XDestroyWindow(dpy, mon->barwin);
    free(mon);
}

void
clearurgent(Client *c) {
    XWMHints *wmh;

    c->isurgent = False;
    if (!(wmh = XGetWMHints(dpy, c->win)))
        return;
    wmh->flags &= ~XUrgencyHint;
    XSetWMHints(dpy, c->win, wmh);
    XFree(wmh);
}

void
configure(Client *c) {
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = dpy;
    ce.event = c->win;
    ce.window = c->win;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->w;
    ce.height = c->h;
    ce.border_width = c->bw;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e) {
    Monitor *m;
    XConfigureEvent *ev = &e->xconfigure;

    if (ev->window == root) {
        sw = ev->width;
        sh = ev->height;
        if (updategeom()) {
            if (dc.drawable != 0)
                XFreePixmap(dpy, dc.drawable);
            dc.drawable = XCreatePixmap(dpy, root, sw, bh, DefaultDepth(dpy, screen));
            XftDrawChange(dc.xftdrawable, dc.drawable);
            updatebars();
            for (m = mons; m; m = m->next)
                XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
            arrange(NULL);
        }
    }
}

void
configurerequest(XEvent *e) {
    Client *c;
    Monitor *m;
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    if ((c = wintoclient(ev->window))) {
        if (ev->value_mask & CWBorderWidth)
            c->bw = ev->border_width;
        else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
            m = c->mon;
            if (ev->value_mask & CWX)
                c->x = m->mx + ev->x;
            if (ev->value_mask & CWY)
                c->y = m->my + ev->y;
            if (ev->value_mask & CWWidth)
                c->w = ev->width;
            if (ev->value_mask & CWHeight)
                c->h = ev->height;
            if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
                c->x = m->mx + (m->mw / 2 - c->w / 2); /* center in x direction */
            if ((c->y + c->h) > m->my + m->mh && c->isfloating)
                c->y = m->my + (m->mh / 2 - c->h / 2); /* center in y direction */
            if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
                configure(c);
            if (ISVISIBLE(c))
                XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
        }
        else
            configure(c);
    }
    else {
        wc.x = ev->x;
        wc.y = ev->y;
        wc.width = ev->width;
        wc.height = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling = ev->above;
        wc.stack_mode = ev->detail;
        XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
    }
    XSync(dpy, False);
}

Monitor *
createmon(void) {
    Monitor *m;
    int i;

    if (!(m = (Monitor *)calloc(1, sizeof(Monitor))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Monitor));
    m->tagset[0] = m->tagset[1] = 1;
    m->showbar = showbar;
    m->topbar = topbar;
    m->lt[0] = &layouts[0];
    m->lt[1] = &layouts[1 % LENGTH(layouts)];
    strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);

    m->curtag = m->prevtag = 1;
    for (i = 0; i < LENGTH(tags)+1; i++) {
        m->nmasters[i] = nmaster;
        m->mfacts[i] = mfact;
        m->lts[i] = &layouts[0];
    }

    return m;
}

void
destroynotify(XEvent *e) {
    Client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    if ((c = wintoclient(ev->window)))
        unmanage(c, True);
}

void
detach(Client *c) {
    Client **tc;

    for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
    *tc = c->next;
}

void
detachstack(Client *c) {
    Client **tc, *t;

    for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
    *tc = c->snext;

    if (c == c->mon->sel) {
        for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
        c->mon->sel = t;
    }
}

void
die(const char *errstr, ...) {
    va_list ap;

    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);

    exit(EXIT_FAILURE);
}

Monitor *
dirtomon(int dir) {
    Monitor *m = NULL;

    if (dir > 0) {
        if (!(m = selmon->next))
            m = mons;
    } else {
        if (selmon == mons)
            for (m = mons; m->next; m = m->next);
        else
            for (m = mons; m->next != selmon; m = m->next);
    }

    return m;
}

void
drawbar(Monitor *m) {
    int x;
    unsigned int i, occ = 0, urg = 0;
    unsigned long *col;
    Client *c;

    for (c = m->clients; c; c = c->next) {
        occ |= c->tags;
        if (c->isurgent)
            urg |= c->tags;
    }

    /* tags */
    dc.x = 0;
    for (i = 0; i < LENGTH(tags); i++) {
        dc.w = TEXTW(tags[i]);
        col = m->tagset[m->seltags] & 1 << i ? dc.sel : dc.norm;
        drawtext(tags[i], DrawTag, col, urg & 1 << i);
        drawsquare(m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
                   occ & 1 << i, urg & 1 << i, DrawTag, col);
        dc.x += dc.w;
    }

    /* layout symbol */
    dc.w = blw = TEXTW(m->ltsymbol);
    drawtext(m->ltsymbol, DrawLtSym, dc.norm, False);
    dc.x += dc.w;
    x = dc.x;

    /* status */
    if (m == selmon) { /* only draw if on selected monitor */
        dc.w = TEXTW(stext);
        dc.x = m->ww - dc.w;
        if (dc.x < x) {
            dc.x = x;
            dc.w = m->ww - x;
        }
        drawtext(stext, DrawStatus, dc.norm, False);
    }
    else
        dc.x = m->ww;

    /* title */
    if ((dc.w = dc.x - x) > bh) {
        dc.x = x;
        if (m->sel) {
            drawtext(m->sel->name, DrawTitle, dc.norm, False);
            drawsquare(m->sel->isfixed, m->sel->isfloating, False, DrawTitle, dc.norm);
        } else
            drawtext(NULL, DrawTitle, dc.norm, False);
    }
    XCopyArea(dpy, dc.drawable, m->barwin, dc.gc, 0, 0, m->ww, bh, 0, 0);
    XSync(dpy, False);
}

void
drawbars(void) {
    Monitor *m;

    for (m = mons; m; m = m->next)
        drawbar(m);
}

void
drawsquare(Bool filled, Bool empty, Bool invert, int drawtype, unsigned long col[ColorLast]) {
    int x;
    XGCValues gcv;
    XRectangle r = { dc.x, dc.y, dc.w, dc.h };

    if (drawtype == DrawTag)
        gcv.foreground = col[invert ? ColorTagBG : ColorTagFG];
    else if (drawtype == DrawLtSym)
        gcv.foreground = col[invert ? ColorLtSymBG : ColorLtSymFG];
    else if (drawtype == DrawTitle)
        gcv.foreground = col[invert ? ColorTitleBG : ColorTitleFG];
    else if (drawtype == DrawStatus)
        gcv.foreground = col[invert ? ColorStatusBG : ColorStatusFG];
    else return;

    XChangeGC(dpy, dc.gc, GCForeground, &gcv);
    x = (dc.font.ascent + dc.font.descent + 2) / 4;
    r.x = dc.x + 1;
    r.y = dc.y + 1;

    if (filled) {
        r.width = r.height = x + 1;
        XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
    } else if (empty) {
        r.width = r.height = x;
        XDrawRectangles(dpy, dc.drawable, dc.gc, &r, 1);
    }
}

void
drawtext(const char *text, int drawtype, unsigned long col[ColorLast], Bool invert) {
    unsigned long bgcolor;
    char buf[256];
    int i, x, y, h, len, olen;
    XRectangle r = { dc.x, dc.y, dc.w, dc.h };
    XftColor *fgcolor;

    if (drawtype == DrawTag)
        bgcolor = col[invert ? ColorTagFG : ColorTagBG];
    else if (drawtype == DrawLtSym)
        bgcolor = col[invert ? ColorLtSymFG : ColorLtSymBG];
    else if (drawtype == DrawTitle)
        bgcolor = col[invert ? ColorTitleFG : ColorTitleBG];
    else if (drawtype == DrawStatus)
        bgcolor = col[invert ? ColorStatusFG : ColorStatusBG];
    else return;

    XSetForeground(dpy, dc.gc, bgcolor);
    XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);

    if (!text)
        return;

    olen = strlen(text);
    h = dc.font.ascent + dc.font.descent;
    y = dc.y;
    x = dc.x + (h / 2);

    /* shorten text if necessary */
    for (len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w - h; len--);

    if (!len)
        return;

    memcpy(buf, text, len);

    if (len < olen)
        for (i = len; i && i > len - 3; buf[--i] = '.');

    if (drawtype == DrawTag)
        fgcolor = (col == dc.norm ? dc.xftnorm : dc.xftsel)+(invert ? ColorTagBG : ColorTagFG);
    else if (drawtype == DrawLtSym)
        fgcolor = dc.xftnorm+(invert ? ColorLtSymBG : ColorLtSymFG);
    else if (drawtype == DrawTitle)
        fgcolor = dc.xftnorm+(invert ? ColorTitleBG : ColorTitleFG);
    else if (drawtype == DrawStatus)
        fgcolor = dc.xftnorm+(invert ? ColorStatusBG : ColorStatusFG);
    else return;

    pango_layout_set_text(dc.plo, text, len);
    pango_xft_render_layout(dc.xftdrawable, fgcolor, dc.plo, x * PANGO_SCALE, y * PANGO_SCALE);
}

void
enternotify(XEvent *e) {
    Client *c;
    Monitor *m;
    XCrossingEvent *ev = &e->xcrossing;

    if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
        return;
    if ((m = wintomon(ev->window)) && m != selmon) {
        unfocus(selmon->sel, True);
        selmon = m;
    }
    if ((c = wintoclient(ev->window)))
        focus(c);
    else
        focus(NULL);
}

void
expose(XEvent *e) {
    Monitor *m;
    XExposeEvent *ev = &e->xexpose;

    if (ev->count == 0 && (m = wintomon(ev->window)))
        drawbar(m);
}

void
focus(Client *c) {
    if (!c || !ISVISIBLE(c))
        for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext);
    /* was if (selmon->sel) */
    if (selmon->sel && selmon->sel != c)
        unfocus(selmon->sel, False);
    if (c) {
        if (c->mon != selmon)
            selmon = c->mon;
        if (c->isurgent)
            clearurgent(c);
        detachstack(c);
        attachstack(c);
        grabbuttons(c, True);
        XSetWindowBorder(dpy, c->win, dc.sel[ColorBorder]);
        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    }
    else
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    selmon->sel = c;
    drawbars();
}

void
focusin(XEvent *e) { /* there are some broken focus acquiring clients */
    XFocusChangeEvent *ev = &e->xfocus;

    if (selmon->sel && ev->window != selmon->sel->win)
        XSetInputFocus(dpy, selmon->sel->win, RevertToPointerRoot, CurrentTime);
}

void
focusmon(const Arg *arg) {
    Monitor *m = NULL;

    if (!mons->next)
        return;
    if ((m = dirtomon(arg->i)) == selmon)
        return;
    unfocus(selmon->sel, True);
    selmon = m;
    focus(NULL);
}

void
focusstack(const Arg *arg) {
    Client *c = NULL, *i;

    if (!selmon->sel)
        return;
    if (arg->i > 0) {
        for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
        if (!c)
            for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
    }
    else {
        for (i = selmon->clients; i != selmon->sel; i = i->next)
            if (ISVISIBLE(i))
                c = i;
        if (!c)
            for (; i; i = i->next)
                if (ISVISIBLE(i))
                    c = i;
    }
    if (c) {
        focus(c);
        restack(selmon);
    }
}

unsigned long
getcolor(const char *colstr, XftColor *color) {
    Colormap cmap = DefaultColormap(dpy, screen);
    Visual *vis = DefaultVisual(dpy, screen);
    if (!XftColorAllocName(dpy,vis,cmap,colstr, color))
        die("error, cannot allocate color '%s'\n", colstr);
    return color->pixel;
}

Bool
getrootptr(int *x, int *y) {
    int di;
    unsigned int dui;
    Window dummy;

    return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w) {
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;

    if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
                          &real, &format, &n, &extra, (unsigned char **)&p) != Success)
        return -1;
    if (n != 0)
        result = *p;
    XFree(p);
    return result;
}

Bool
gettextprop(Window w, Atom atom, char *text, unsigned int size) {
    char **list = NULL;
    int n;
    XTextProperty name;

    if (!text || size == 0)
        return False;
    text[0] = '\0';
    XGetTextProperty(dpy, w, &name, atom);
    if (!name.nitems)
        return False;
    if (name.encoding == XA_STRING)
        strncpy(text, (char *)name.value, size - 1);
    else {
        if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
            strncpy(text, *list, size - 1);
            XFreeStringList(list);
        }
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return True;
}

void
grabbuttons(Client *c, Bool focused) {
    updatenumlockmask();
    {
        unsigned int i, j;
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        if (focused) {
            for (i = 0; i < LENGTH(buttons); i++)
                if (buttons[i].click == ClickClient)
                    for (j = 0; j < LENGTH(modifiers); j++)
                        XGrabButton(dpy, buttons[i].button,
                                    buttons[i].mask | modifiers[j],
                                    c->win, False, BUTTONMASK,
                                    GrabModeAsync, GrabModeSync, None, None);
        }
        else
            XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
                        BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
    }
}

void
grabkeys(void) {
    updatenumlockmask();
    {
        unsigned int i, j;
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        KeyCode code;

        XUngrabKey(dpy, AnyKey, AnyModifier, root);
        for (i = 0; i < LENGTH(keys); i++) {
            if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
                for (j = 0; j < LENGTH(modifiers); j++)
                    XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
                         True, GrabModeAsync, GrabModeAsync);
        }
    }
}

void
initfont(const char *fontstr) {
    PangoFontMetrics *metrics;
    dc.pgc = pango_xft_get_context(dpy, screen);
    dc.pfd = pango_font_description_from_string(fontstr);

    metrics = pango_context_get_metrics(dc.pgc, dc.pfd, pango_language_from_string(setlocale(LC_CTYPE, "")));
    dc.font.ascent = pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
    dc.font.descent = pango_font_metrics_get_descent(metrics) / PANGO_SCALE;

    pango_font_metrics_unref(metrics);

    dc.plo = pango_layout_new(dc.pgc);
    pango_layout_set_font_description(dc.plo, dc.pfd);
    dc.font.height = dc.font.ascent + dc.font.descent;
}

Bool
isprotodel(Client *c) {
    int i, n;
    Atom *protocols;
    Bool ret = False;

    if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
        for (i = 0; !ret && i < n; i++)
            if (protocols[i] == wmatom[WMDelete])
                ret = True;
        XFree(protocols);
    }
    return ret;
}

#ifdef XINERAMA
static Bool
isuniquegeom(XineramaScreenInfo *unique, size_t len, XineramaScreenInfo *info) {
    unsigned int i;

    for (i = 0; i < len; i++)
        if (unique[i].x_org == info->x_org && unique[i].y_org == info->y_org
        && unique[i].width == info->width && unique[i].height == info->height)
            return False;
    return True;
}
#endif /* XINERAMA */

void
keypress(XEvent *e) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev;

    ev = &e->xkey;
    keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
    for (i = 0; i < LENGTH(keys); i++)
        if (keysym == keys[i].keysym
        && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
        && keys[i].func)
            keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg) {
    XEvent ev;

    if (!selmon->sel)
        return;
    if (isprotodel(selmon->sel)) {
        ev.type = ClientMessage;
        ev.xclient.window = selmon->sel->win;
        ev.xclient.message_type = wmatom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = wmatom[WMDelete];
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, selmon->sel->win, False, NoEventMask, &ev);
    }
    else {
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, selmon->sel->win);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
}

void
manage(Window w, XWindowAttributes *wa) {
    static Client cz;
    Client *c, *t = NULL;
    Window trans = None;
    XWindowChanges wc;

    if (!(c = malloc(sizeof(Client))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Client));
    *c = cz;
    c->win = w;
    updatetitle(c);
    if (XGetTransientForHint(dpy, w, &trans))
        t = wintoclient(trans);
    if (t) {
        c->mon = t->mon;
        c->tags = t->tags;
    }
    else {
        c->mon = selmon;
        applyrules(c);
    }
    /* geometry */
    c->x = c->oldx = wa->x + c->mon->wx;
    c->y = c->oldy = wa->y + c->mon->wy;
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;
    if (c->w == c->mon->mw && c->h == c->mon->mh) {
        c->isfloating = 1;
        c->x = c->mon->mx;
        c->y = c->mon->my;
        c->bw = 0;
    }
    else {
        if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
            c->x = c->mon->mx + c->mon->mw - WIDTH(c);
        if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
            c->y = c->mon->my + c->mon->mh - HEIGHT(c);
        c->x = MAX(c->x, c->mon->mx);
        /* only fix client y-offset, if the client center might cover the bar */
        c->y = MAX(c->y, ((c->mon->by == 0) && (c->x + (c->w / 2) >= c->mon->wx)
                   && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
        c->bw = borderpx;
    }
    wc.border_width = c->bw;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, dc.norm[ColorBorder]);
    configure(c); /* propagates border_width, if size doesn't change */
    updatesizehints(c);
    XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
    grabbuttons(c, False);
    if (!c->isfloating)
        c->isfloating = c->oldstate = trans != None || c->isfixed;
    if (c->isfloating)
        XRaiseWindow(dpy, c->win);
    attach(c);
    attachstack(c);
    XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
    XMapWindow(dpy, c->win);
    setclientstate(c, NormalState);
    arrange(c->mon);
}

void
mappingnotify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard)
        grabkeys();
}

void
maprequest(XEvent *e) {
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    if (!XGetWindowAttributes(dpy, ev->window, &wa))
        return;
    if (wa.override_redirect)
        return;
    if (!wintoclient(ev->window))
        manage(ev->window, &wa);
}

void
monocle(Monitor *m) {
    unsigned int n = 0;
    Client *c;

    for (c = m->clients; c; c = c->next)
        if (ISVISIBLE(c))
            n++;
    if (n > 0) /* override layout symbol */
        snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
    for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
        resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, False);
}

void
movemouse(const Arg *arg) {
    int x, y, ocx, ocy, nx, ny;
    Client *c;
    Monitor *m;
    XEvent ev;

    if (!(c = selmon->sel))
        return;
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
    None, cursor[CursorMove], CurrentTime) != GrabSuccess)
        return;
    if (!getrootptr(&x, &y))
        return;
    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            nx = ocx + (ev.xmotion.x - x);
            ny = ocy + (ev.xmotion.y - y);
            if (snap && nx >= selmon->wx && nx <= selmon->wx + selmon->ww
            && ny >= selmon->wy && ny <= selmon->wy + selmon->wh) {
                if (abs(selmon->wx - nx) < snap)
                    nx = selmon->wx;
                else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
                    nx = selmon->wx + selmon->ww - WIDTH(c);
                if (abs(selmon->wy - ny) < snap)
                    ny = selmon->wy;
                else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
                    ny = selmon->wy + selmon->wh - HEIGHT(c);
                if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
                && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
                    togglefloating(NULL);
            }
            if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
                resize(c, nx, ny, c->w, c->h, True);
            break;
        }
    } while(ev.type != ButtonRelease);
    XUngrabPointer(dpy, CurrentTime);
    if ((m = ptrtomon(c->x + c->w / 2, c->y + c->h / 2)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

Client *
nexttiled(Client *c) {
    for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
    return c;
}

Client *
prevtiled(Client *c) {
	Client *p, *r;

	for(p = selmon->clients, r = NULL; p && p != c; p = p->next)
		if(!p->isfloating && ISVISIBLE(p))
			r = p;

	return r;
}

void
propertynotify(XEvent *e) {
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if ((ev->window == root) && (ev->atom == XA_WM_NAME))
        updatestatus();
    else if (ev->state == PropertyDelete)
        return; /* ignore */
    else if ((c = wintoclient(ev->window))) {
        switch (ev->atom) {
        default: break;
        case XA_WM_TRANSIENT_FOR:
            XGetTransientForHint(dpy, c->win, &trans);
            if (!c->isfloating && (c->isfloating = (wintoclient(trans) != NULL)))
                arrange(c->mon);
            break;
        case XA_WM_NORMAL_HINTS:
            updatesizehints(c);
            break;
        case XA_WM_HINTS:
            updatewmhints(c);
            drawbars();
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
            updatetitle(c);
            if (c == c->mon->sel)
                drawbar(c->mon);
        }
    }
}

Monitor *
ptrtomon(int x, int y) {
    Monitor *m;

    for (m = mons; m; m = m->next)
        if (INRECT(x, y, m->wx, m->wy, m->ww, m->wh))
            return m;
    return selmon;
}

void
pushdown(const Arg *arg) {
	Client *sel = selmon->sel;
	Client *c;

	if (!sel || sel->isfloating)
		return;

	if ((c = nexttiled(sel->next))) {
		/* attach after c */
		detach(sel);
		sel->next = c->next;
		c->next = sel;
	} else {
		/* move to the front */
		detach(sel);
		attach(sel);
	}

	focus(sel);
	arrange(selmon);
}

void
pushup(const Arg *arg) {
	Client *sel = selmon->sel;
	Client *c;

	if (!sel || sel->isfloating)
		return;

	if ((c = prevtiled(sel))) {
		/* attach before c */
		detach(sel);
		sel->next = c;
		if(selmon->clients == c)
			selmon->clients = sel;
		else {
			for(c = selmon->clients; c->next != sel->next; c = c->next);
			c->next = sel;
		}
	} else {
		/* move to the end */
		for(c = sel; c->next; c = c->next);
		detach(sel);
		sel->next = NULL;
		c->next = sel;
	}

	focus(sel);
	arrange(selmon);
}

void
clientmessage(XEvent *e) {
    XClientMessageEvent *cme = &e->xclient;
    Client *c;

    if ((c = wintoclient(cme->window))
    && (cme->message_type == netatom[NetWMState] && cme->data.l[1] == netatom[NetWMFullscreen]))
    {
        if (cme->data.l[0]) {
            XChangeProperty(dpy, cme->window, netatom[NetWMState], XA_ATOM, 32,
                            PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
            c->oldstate = c->isfloating;
            c->oldbw = c->bw;
            c->bw = 0;
            c->isfloating = 1;
            resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
            XRaiseWindow(dpy, c->win);
        }
        else {
            XChangeProperty(dpy, cme->window, netatom[NetWMState], XA_ATOM, 32,
                            PropModeReplace, (unsigned char*)0, 0);
            c->isfloating = c->oldstate;
            c->bw = c->oldbw;
            c->x = c->oldx;
            c->y = c->oldy;
            c->w = c->oldw;
            c->h = c->oldh;
            resizeclient(c, c->x, c->y, c->w, c->h);
            arrange(c->mon);
        }
    }
}

void
quit(const Arg *arg) {
    running = False;
}

void
resetnmaster(const Arg *arg) {
    if (selmon->nmasters[selmon->curtag] == nmaster)
        return;
    selmon->nmasters[selmon->curtag] = nmaster;
    arrange(selmon);
}

void
resetmfact(const Arg *arg) {
    if (selmon->mfacts[selmon->curtag] == mfact)
        return;
    selmon->mfacts[selmon->curtag] = mfact;
    arrange(selmon);
}

void
resize(Client *c, int x, int y, int w, int h, Bool interact) {
    if (applysizehints(c, &x, &y, &w, &h, interact))
        resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h) {
    XWindowChanges wc;

    c->oldx = c->x; c->x = wc.x = x;
    c->oldy = c->y; c->y = wc.y = y;
    c->oldw = c->w; c->w = wc.width = w;
    c->oldh = c->h; c->h = wc.height = h;
    wc.border_width = c->bw;
    XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
    configure(c);
    XSync(dpy, False);
}

void
resizemouse(const Arg *arg) {
    int ocx, ocy;
    int nw, nh;
    Client *c;
    Monitor *m;
    XEvent ev;

    if (!(c = selmon->sel))
        return;
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                    None, cursor[CursorResize], CurrentTime) != GrabSuccess)
        return;
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch(ev.type) {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
            nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
            if (snap && nw >= selmon->wx && nw <= selmon->wx + selmon->ww
            && nh >= selmon->wy && nh <= selmon->wy + selmon->wh)
            {
                if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
                && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
                    togglefloating(NULL);
            }
            if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
                resize(c, c->x, c->y, nw, nh, True);
            break;
        }
    } while(ev.type != ButtonRelease);
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    XUngrabPointer(dpy, CurrentTime);
    while(XCheckMaskEvent(dpy, EnterWindowMask, &ev));
    if ((m = ptrtomon(c->x + c->w / 2, c->y + c->h / 2)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

void
restack(Monitor *m) {
    Client *c;
    XEvent ev;
    XWindowChanges wc;

    drawbar(m);
    if (!m->sel)
        return;
    if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
        XRaiseWindow(dpy, m->sel->win);
    if (m->lt[m->sellt]->arrange) {
        wc.stack_mode = Below;
        wc.sibling = m->barwin;
        for (c = m->stack; c; c = c->snext)
            if (!c->isfloating && ISVISIBLE(c)) {
                XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
                wc.sibling = c->win;
            }
    }
    XSync(dpy, False);
    while(XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void) {
    XEvent ev;
    /* main event loop */
    XSync(dpy, False);
    while(running && !XNextEvent(dpy, &ev)) {
        if (handler[ev.type])
            handler[ev.type](&ev); /* call handler */
    }
}

void
scan(void) {
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(dpy, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
                continue;
            if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
                manage(wins[i], &wa);
        }
        for (i = 0; i < num; i++) { /* now the transients */
            if (!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if (XGetTransientForHint(dpy, wins[i], &d1)
            && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
                manage(wins[i], &wa);
        }
        if (wins)
            XFree(wins);
    }
}

void
sendmon(Client *c, Monitor *m) {
    if (c->mon == m)
        return;
    unfocus(c, True);
    detach(c);
    detachstack(c);
    c->mon = m;
    c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
    attach(c);
    attachstack(c);
    focus(NULL);
    arrange(NULL);
}

void
setclientstate(Client *c, long state) {
    long data[] = { state, None };

    XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
            PropModeReplace, (unsigned char *)data, 2);
}

void
setlayout(const Arg *arg) {
    if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
        selmon->sellt ^= 1;
    if (arg && arg->v)
        selmon->lt[selmon->sellt] = selmon->lts[selmon->curtag] = (Layout *)arg->v;
    
    strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);

    if (selmon->sel)
        arrange(selmon);
    else
        drawbar(selmon);
}

void
setnmaster(const Arg *arg) {
    Client *c;
    int new;
    int clen;

    if (arg->i == 0)
        return;

    for (c = selmon->clients, clen = 0; c; c = c->next, clen++);

    new = selmon->nmasters[selmon->curtag] + arg->i;
    if (new < 0 || new > clen)
        return;

    selmon->nmasters[selmon->curtag] = new;
    arrange(selmon);
}

/* arg > 1.0 will set mfact absolutly */
void
setmfact(const Arg *arg) {
    float f;
    if (!arg || !selmon->lt[selmon->sellt]->arrange)
        return;
    f = arg->f < 1.0 ? arg->f + selmon->mfacts[selmon->curtag] : arg->f - 1.0;
    if (f < 0.1 || f > 0.9)
        return;
    selmon->mfacts[selmon->curtag] = f;
    arrange(selmon);
}

void
setup(void) {
    XSetWindowAttributes wa;

    /* clean up any zombies immediately */
    sigchld(0);

    /* init screen */
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    initfont(font);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    bh = dc.h = dc.font.height + 2;
    updategeom();

    /* init atoms */
    wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
    netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
    netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
    netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

    /* init cursors */
    cursor[CursorNormal] = XCreateFontCursor(dpy, XC_left_ptr);
    cursor[CursorResize] = XCreateFontCursor(dpy, XC_sizing);
    cursor[CursorMove] = XCreateFontCursor(dpy, XC_fleur);

    /* init border colors */
    dc.norm[ColorBorder] = getcolor(border_color, dc.xftnorm+ColorBorder);
    dc.sel[ColorBorder] = getcolor(sel_border_color, dc.xftsel+ColorBorder);

    /* init tag colors */
    dc.norm[ColorTagBG] = getcolor(tag_bg_color, dc.xftnorm+ColorTagBG);
    dc.norm[ColorTagFG] = getcolor(tag_fg_color, dc.xftnorm+ColorTagFG);
    dc.sel[ColorTagBG] = getcolor(sel_tag_bg_color, dc.xftsel+ColorTagBG);
    dc.sel[ColorTagFG] = getcolor(sel_tag_fg_color, dc.xftsel+ColorTagFG);

    /* init ltsym colors */
    dc.norm[ColorLtSymBG] = getcolor(ltsym_bg_color, dc.xftnorm+ColorLtSymBG);
    dc.norm[ColorLtSymFG] = getcolor(ltsym_fg_color, dc.xftnorm+ColorLtSymFG);

    /* init title colors */
    dc.norm[ColorTitleBG] = getcolor(title_bg_color, dc.xftnorm+ColorTitleBG);
    dc.norm[ColorTitleFG] = getcolor(title_fg_color, dc.xftnorm+ColorTitleFG);

    /* init status colors */
    dc.norm[ColorStatusBG] = getcolor(status_bg_color, dc.xftnorm+ColorStatusBG);
    dc.norm[ColorStatusFG] = getcolor(status_fg_color, dc.xftnorm+ColorStatusFG);

    dc.drawable = XCreatePixmap(dpy, root, DisplayWidth(dpy, screen), bh, DefaultDepth(dpy, screen));
    dc.gc = XCreateGC(dpy, root, 0, NULL);
    XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);

    dc.xftdrawable = XftDrawCreate(dpy, dc.drawable, DefaultVisual(dpy,screen), DefaultColormap(dpy,screen));
    if(!dc.xftdrawable)
        printf("error: couldn't create drawable\n");

    /* init bars */
    updatebars();
    updatestatus();

    /* EWMH support per view */
    XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
            PropModeReplace, (unsigned char *) netatom, NetLast);

    /* select for events */
    wa.cursor = cursor[CursorNormal];
    wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask
                    |EnterWindowMask|LeaveWindowMask|StructureNotifyMask
                    |PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    grabkeys();
}

void
showhide(Client *c) {
    if (!c)
        return;
    if (ISVISIBLE(c)) { /* show clients top down */
        XMoveWindow(dpy, c->win, c->x, c->y);
        if (!c->mon->lt[c->mon->sellt]->arrange || c->isfloating)
            resize(c, c->x, c->y, c->w, c->h, False);
        showhide(c->snext);
    }
    else { /* hide clients bottom up */
        showhide(c->snext);
        XMoveWindow(dpy, c->win, c->x + 2 * sw, c->y);
    }
}


void
sigchld(int unused) {
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        die("Can't install SIGCHLD handler");
    while(0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg) {
    if (fork() == 0) {
        if (dpy)
            close(ConnectionNumber(dpy));
        setsid();
        execvp(((char **)arg->v)[0], (char **)arg->v);
        fprintf(stderr, "properwm: execvp %s", ((char **)arg->v)[0]);
        perror(" failed");
        exit(0);
    }
}

void
stack(Monitor *m) {
    int x, y, h, w, mh, nm;
    unsigned int i, n;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
    c = nexttiled(m->clients);

    nm = m->num < LENGTH(tags)+1 ? m->nmasters[m->curtag] : nmaster;
    if (nm > n)
        nm = n;

    /* master */
    if (nm > 0) {
        mh = m->mfacts[m->curtag] * m->wh;
        w = m->ww / nm;
        if (w < bh)
            w = m->ww;
        x = m->wx;
        for (i = 0; i < nm; i++, c = nexttiled(c->next)) {
            resize(c, x, m->wy, ((i + 1 == nm) ? m->wx + m->ww - x : w) - 2 * c->bw,
                   (n == nm ? m->wh : mh) - 2 * c->bw, False);
            if (w != m->ww)
                x = c->x + WIDTH(c);
        }
        n -= nm;
    } else
        mh = 0;

    if (n == 0)
        return;

    /* tile stack */
    x = m->wx;
    y = m->wy + mh;
    w = m->ww / n;
    h = m->wh - mh;

    if (w < bh)
        w = m->ww;

    for (i = 0; c; c = nexttiled(c->next), i++) {
        resize(c, x, y, ((i + 1 == n) ? m->wx + m->ww - x : w) - 2 * c->bw,
               h - 2 * c->bw, False);
        if (w != m->ww)
            x = c->x + WIDTH(c);
    }
}

void
tag(const Arg *arg) {
    if (selmon->sel && arg->ui & TAGMASK) {
        selmon->sel->tags = arg->ui & TAGMASK;
        arrange(selmon);
    }
}

void
tagmon(const Arg *arg) {
    if (!selmon->sel || !mons->next)
        return;
    sendmon(selmon->sel, dirtomon(arg->i));
}

int
textnw(const char *text, unsigned int len) {
    PangoRectangle r;
    pango_layout_set_text(dc.plo, text, len);
    pango_layout_get_extents(dc.plo, &r, 0);
    return r.width / PANGO_SCALE;
}

void
tile(Monitor *m) {
    int x, y, h, w, mw, nm;
    unsigned int i, n;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
    c = nexttiled(m->clients);

    nm = m->curtag < LENGTH(tags)+1 ? m->nmasters[m->curtag] : nmaster;

    if (nm > n)
        nm = n;

    /* master */
    if (nm > 0) {
        mw = m->mfacts[m->curtag] * m->ww;
        h = m->wh / nm;
        if (h < bh)
            h = m->wh;
        y = m->wy;
        for (i = 0; i < nm; i++, c = nexttiled(c->next)) {
            resize(c, m->wx, y, (n == nm ? m->ww : mw) - 2 * c->bw,
                   ((i + 1 == nm) ? m->wy + m->wh - y : h) - 2 * c->bw, False);
            if (h != m->wh)
                y = c->y + HEIGHT(c);
        }
        n -= nm;
    } else
        mw = 0;

    if (n == 0)
        return;

    /* tile stack */
    x = m->wx + mw;
    y = m->wy;
    w = m->ww - mw;
    h = m->wh / n;

    if (h < bh)
        h = m->wh;

    for (i = 0; c; c = nexttiled(c->next), i++) {
        resize(c, x, y, w - 2 * c->bw,
               ((i + 1 == n) ? m->wy + m->wh - y : h) - 2 * c->bw, False);
        if (h != m->wh)
            y = c->y + HEIGHT(c);
    }
}

void
togglebar(const Arg *arg) {
    selmon->showbar = !selmon->showbar;
    updatebarpos(selmon);
    XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
    arrange(selmon);
}

void
togglefloating(const Arg *arg) {
    if (!selmon->sel)
        return;
    selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
    if (selmon->sel->isfloating)
        resize(selmon->sel, selmon->sel->x, selmon->sel->y,
               selmon->sel->w, selmon->sel->h, False);
    arrange(selmon);
}

void
toggletag(const Arg *arg) {
    unsigned int newtags;
    unsigned int i;

    if (!selmon->sel)
        return;
    newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
    if (newtags) {
        selmon->sel->tags = newtags;
        if (newtags == ~0) {
            selmon->prevtag = selmon->curtag;
            selmon->curtag = 0;
        }
        if (!(newtags & 1 << (selmon->curtag - 1))) {
            selmon->prevtag = selmon->curtag;
            for (i=0; !(newtags & 1 << i); i++);
            selmon->curtag = i + 1;
        }
        selmon->sel->tags = newtags;
        selmon->lt[selmon->sellt] = selmon->lts[selmon->curtag];
        arrange(selmon);
    }
}

void
toggleview(const Arg *arg) {
    unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

    if (newtagset) {
        selmon->tagset[selmon->seltags] = newtagset;
        arrange(selmon);
    }
}

void
unfocus(Client *c, Bool setfocus) {
    if (!c)
        return;
    grabbuttons(c, False);
    XSetWindowBorder(dpy, c->win, dc.norm[ColorBorder]);
    if (setfocus)
        XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
}

void
unmanage(Client *c, Bool destroyed) {
    Monitor *m = c->mon;
    XWindowChanges wc;

    /* The server grab construct avoids race conditions. */
    detach(c);
    detachstack(c);
    if (!destroyed) {
        wc.border_width = c->oldbw;
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        setclientstate(c, WithdrawnState);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
    free(c);
    focus(NULL);
    arrange(m);
}

void
unmapnotify(XEvent *e) {
    Client *c;
    XUnmapEvent *ev = &e->xunmap;

    if ((c = wintoclient(ev->window)))
        unmanage(c, False);
}

void
updatebars(void) {
    Monitor *m;
    XSetWindowAttributes wa;

    wa.override_redirect = True;
    wa.background_pixmap = ParentRelative;
    wa.event_mask = ButtonPressMask|ExposureMask;
    for (m = mons; m; m = m->next) {
        m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
                                  CopyFromParent, DefaultVisual(dpy, screen),
                                  CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
        XDefineCursor(dpy, m->barwin, cursor[CursorNormal]);
        XMapRaised(dpy, m->barwin);
    }
}

void
updatebarpos(Monitor *m) {
    m->wy = m->my;
    m->wh = m->mh;
    if (m->showbar) {
        m->wh -= bh;
        m->by = m->topbar ? m->wy : m->wy + m->wh;
        m->wy = m->topbar ? m->wy + bh : m->wy;
    }
    else
        m->by = -bh;
}

Bool
updategeom(void) {
    Bool dirty = False;

#ifdef XINERAMA
    if (XineramaIsActive(dpy)) {
        int i, j, n, nn;
        Client *c;
        Monitor *m;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
        XineramaScreenInfo *unique = NULL;

        info = XineramaQueryScreens(dpy, &nn);
        for (n = 0, m = mons; m; m = m->next, n++);
        /* only consider unique geometries as separate screens */
        if (!(unique = (XineramaScreenInfo *)malloc(sizeof(XineramaScreenInfo) * nn)))
            die("fatal: could not malloc() %u bytes\n", sizeof(XineramaScreenInfo) * nn);
        for (i = 0, j = 0; i < nn; i++)
            if (isuniquegeom(unique, j, &info[i]))
                memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
        XFree(info);
        nn = j;
        if (n <= nn) {
            for (i = 0; i < (nn - n); i++) { /* new monitors available */
                for (m = mons; m && m->next; m = m->next);
                if (m)
                    m->next = createmon();
                else
                    mons = createmon();
            }
            for (i = 0, m = mons; i < nn && m; m = m->next, i++)
                if (i >= n
                || (unique[i].x_org != m->mx || unique[i].y_org != m->my
                    || unique[i].width != m->mw || unique[i].height != m->mh))
                {
                    dirty = True;
                    m->num = i;
                    m->mx = m->wx = unique[i].x_org;
                    m->my = m->wy = unique[i].y_org;
                    m->mw = m->ww = unique[i].width;
                    m->mh = m->wh = unique[i].height;
                    updatebarpos(m);
                }
        }
        else { /* less monitors available nn < n */
            for (i = nn; i < n; i++) {
                for (m = mons; m && m->next; m = m->next);
                while(m->clients) {
                    dirty = True;
                    c = m->clients;
                    m->clients = c->next;
                    detachstack(c);
                    c->mon = mons;
                    attach(c);
                    attachstack(c);
                }
                if (m == selmon)
                    selmon = mons;
                cleanupmon(m);
            }
        }
        free(unique);
    }
    else
#endif /* XINERAMA */
    /* default monitor setup */
    {
        if (!mons)
            mons = createmon();
        if (mons->mw != sw || mons->mh != sh) {
            dirty = True;
            mons->mw = mons->ww = sw;
            mons->mh = mons->wh = sh;
            updatebarpos(mons);
        }
    }
    if (dirty) {
        selmon = mons;
        selmon = wintomon(root);
    }
    return dirty;
}

void
updatenumlockmask(void) {
    unsigned int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++)
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
               == XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c) {
    long msize;
    XSizeHints size;

    if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
        /* size is uninitialized, ensure that size.flags aren't used */
        size.flags = PSize;
    if (size.flags & PBaseSize) {
        c->basew = size.base_width;
        c->baseh = size.base_height;
    }
    else if (size.flags & PMinSize) {
        c->basew = size.min_width;
        c->baseh = size.min_height;
    }
    else
        c->basew = c->baseh = 0;
    if (size.flags & PResizeInc) {
        c->incw = size.width_inc;
        c->inch = size.height_inc;
    }
    else
        c->incw = c->inch = 0;
    if (size.flags & PMaxSize) {
        c->maxw = size.max_width;
        c->maxh = size.max_height;
    }
    else
        c->maxw = c->maxh = 0;
    if (size.flags & PMinSize) {
        c->minw = size.min_width;
        c->minh = size.min_height;
    }
    else if (size.flags & PBaseSize) {
        c->minw = size.base_width;
        c->minh = size.base_height;
    }
    else
        c->minw = c->minh = 0;
    if (size.flags & PAspect) {
        c->mina = (float)size.min_aspect.y / size.min_aspect.x;
        c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
    }
    else
        c->maxa = c->mina = 0.0;
    c->isfixed = (c->maxw && c->minw && c->maxh && c->minh
                 && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatetitle(Client *c) {
    if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
        gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
    if (c->name[0] == '\0') /* hack to mark broken clients */
        strcpy(c->name, broken);
}

void
updatestatus(void) {
    if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
        strcpy(stext, "properwm-"VERSION);
    drawbar(selmon);
}

void
updatewmhints(Client *c) {
    XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, c->win))) {
        if (c == selmon->sel && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->win, wmh);
        }
        else
            c->isurgent = (wmh->flags & XUrgencyHint) ? True : False;
        XFree(wmh);
    }
}

void
view(const Arg *arg) {
    unsigned int i;
    if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
        return;
    selmon->seltags ^= 1; /* toggle sel tagset */
    if (arg->ui & TAGMASK) {
         selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
        selmon->prevtag = selmon->curtag;
        if (arg->ui == ~0)
            selmon->curtag = 0;
        else {
            for (i=0; !(arg->ui & 1 << i); i++);
            selmon->curtag = i + 1;
        }
    } else {
        selmon->prevtag = selmon->curtag ^ selmon->prevtag;
        selmon->curtag ^= selmon->prevtag;
        selmon->prevtag = selmon->curtag ^ selmon->prevtag;
    }
    selmon->lt[selmon->sellt] = selmon->lts[selmon->curtag];
    arrange(selmon);
}

Client *
wintoclient(Window w) {
    Client *c;
    Monitor *m;

    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->win == w)
                return c;
    return NULL;
}

Monitor *
wintomon(Window w) {
    int x, y;
    Client *c;
    Monitor *m;

    if (w == root && getrootptr(&x, &y))
        return ptrtomon(x, y);
    for (m = mons; m; m = m->next)
        if (w == m->barwin)
            return m;
    if ((c = wintoclient(w)))
        return c->mon;
    return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int
xerror(Display *dpy, XErrorEvent *ee) {
    if (ee->error_code == BadWindow
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    fprintf(stderr, "properwm: fatal error: request code=%d, error code=%d\n",
            ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee) {
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee) {
    otherwm = True;
    return -1;
}

void
zoom(const Arg *arg) {
    Client *c = selmon->sel;

    if (!selmon->lt[selmon->sellt]->arrange
    || selmon->lt[selmon->sellt]->arrange == monocle
    || (selmon->sel && selmon->sel->isfloating))
        return;

    if (c == nexttiled(selmon->clients) && (!c || !(c = nexttiled(c->next))))
        return;

    detach(c);
    attach(c);
    focus(c);
    arrange(c->mon);
}

int
main(int argc, char *argv[]) {
    if (argc == 2 && !strcmp("-v", argv[1]))
        die("properwm-"VERSION", Forked from DWM 5.8.2 with pertag/push patches and additional mods by SpeedDefrost.\n");
    else if (argc != 1)
        die("usage: properwm [-v]\n");
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
        fputs("warning: no locale support\n", stderr);
    if (!(dpy = XOpenDisplay(NULL)))
        die("properwm: cannot open display\n");
    checkotherwm();
    setup();
    scan();
    run();
    cleanup();
    XCloseDisplay(dpy);
    return 0;
}
