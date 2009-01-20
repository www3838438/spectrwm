/* $scrotwm$ */
/*
 * Copyright (c) 2009 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2009 Ryan McBride <mcbride@countersiege.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Much code and ideas taken from dwm under the following license:
 * MIT/X Consortium License
 * 
 * 2006-2008 Anselm R Garbe <garbeam at gmail dot com>
 * 2006-2007 Sander van Dijk <a dot h dot vandijk at gmail dot com>
 * 2006-2007 Jukka Salmi <jukka at salmi dot ch>
 * 2007 Premysl Hruby <dfenze at gmail dot com>
 * 2007 Szabolcs Nagy <nszabolcs at gmail dot com>
 * 2007 Christof Musik <christof at sendfax dot de>
 * 2007-2008 Enno Gottox Boland <gottox at s01 dot de>
 * 2007-2008 Peter Hartlich <sgkkr at hartlich dot com>
 * 2008 Martin Hurton <martin dot hurton at gmail dot com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define	SWM_VERSION	"0.5"

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <util.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <sys/select.h>

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>

#if RANDR_MAJOR < 1
#  error XRandR versions less than 1.0 are not supported
#endif

#if RANDR_MAJOR >= 1
#if RANDR_MINOR >= 2
#define SWM_XRR_HAS_CRTC
#endif
#endif

/* #define SWM_DEBUG */
#ifdef SWM_DEBUG
#define DPRINTF(x...)		do { if (swm_debug) fprintf(stderr, x); } while(0)
#define DNPRINTF(n,x...)	do { if (swm_debug & n) fprintf(stderr, x); } while(0)
#define	SWM_D_MISC		0x0001
#define	SWM_D_EVENT		0x0002
#define	SWM_D_WS		0x0004
#define	SWM_D_FOCUS		0x0008
#define	SWM_D_MOVE		0x0010
#define	SWM_D_STACK		0x0020

u_int32_t		swm_debug = 0
			    | SWM_D_MISC
			    | SWM_D_EVENT
			    | SWM_D_WS
			    | SWM_D_FOCUS
			    | SWM_D_MOVE
			    | SWM_D_STACK
			    ;
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

#define LENGTH(x)               (sizeof x / sizeof x[0])
#define MODKEY			Mod1Mask
#define CLEANMASK(mask)         (mask & ~(numlockmask | LockMask))

#define X(r)		(r)->g.x	
#define Y(r)		(r)->g.y
#define WIDTH(r)	(r)->g.w	
#define HEIGHT(r)	(r)->g.h

char			**start_argv;
Atom			astate;
int			(*xerrorxlib)(Display *, XErrorEvent *);
int			other_wm;
int			running = 1;
int			xrandr_eventbase;
int			ignore_enter = 0;
unsigned int		numlockmask = 0;
Display			*display;

/* dialog windows */
double			dialog_ratio = .6;
/* status bar */
#define SWM_BAR_MAX	(128)
sig_atomic_t		bar_alarm = 0;
int			bar_enabled = 1;
int			bar_verbose = 1;
int			bar_height = 0;
GC			bar_gc;
XGCValues		bar_gcv;
XFontStruct		*bar_fs;
char			*bar_fonts[] = {
			    "-*-terminus-*-*-*-*-*-*-*-*-*-*-*-*",
			    "-*-times-medium-r-*-*-*-*-*-*-*-*-*-*",
			    NULL
};

/* terminal + args */
char				*spawn_term[] = { "xterm", NULL };
char				*spawn_menu[] = { "dmenu_run", NULL };

/* layout manager data */
struct swm_geometry {
	int			x;
	int			y;
	int			w;
	int			h;
};

struct swm_screen;
struct workspace;

/* virtual "screens" */
struct swm_region {
	TAILQ_ENTRY(swm_region)	entry;
	struct swm_geometry	g;
	Window			bar_window;
	struct workspace 	*ws;	/* current workspace on this region */
	struct swm_screen	*s;	/* screen idx */
}; 
TAILQ_HEAD(swm_region_list, swm_region);


struct ws_win {
	TAILQ_ENTRY(ws_win)	entry;
	Window			id;
	struct swm_geometry	g;
	int			focus;
	int			floating;
	int			transient;
	struct workspace	*ws;	/* always valid */
	struct swm_screen	*s;	/* always valid, never changes */
	XWindowAttributes	wa;
};
TAILQ_HEAD(ws_win_list, ws_win);

/* layout handlers */
void	stack(void);
void	vertical_config(struct workspace *, int);
void	vertical_stack(struct workspace *, struct swm_geometry *);
void	horizontal_config(struct workspace *, int);
void	horizontal_stack(struct workspace *, struct swm_geometry *);
void	max_stack(struct workspace *, struct swm_geometry *);

struct layout {
	void		(*l_stack)(struct workspace *, struct swm_geometry *);
	void		(*l_config)(struct workspace *, int);
} layouts[] =  {
	/* stack,		configure */
	{ vertical_stack,	vertical_config},
	{ horizontal_stack,	horizontal_config},
	{ max_stack,		NULL},
	{ NULL,			NULL},
};

#define SWM_H_SLICE		(32)
#define SWM_V_SLICE		(32)

/* define work spaces */
struct workspace {
	int			idx;		/* workspace index */
	int			restack;	/* restack on switch */
	struct layout		*cur_layout;	/* current layout handlers */
	struct ws_win		*focus;		/* may be NULL */
	struct swm_region	*r;		/* may be NULL */
	struct ws_win_list	winlist;	/* list of windows in ws */

	/* stacker state */
	struct {
				int horizontal_msize;
				int horizontal_mwin;
				int vertical_msize;
				int vertical_mwin;
	} l_state;
};

/* physical screen mapping */
#define SWM_WS_MAX		(10)		/* XXX Too small? */
struct swm_screen {
	int			idx;		/* screen index */
	struct swm_region_list	rl;	/* list of regions on this screen */
	Window			root;
	int			xrandr_support;
	struct workspace	ws[SWM_WS_MAX];

	/* colors */
	unsigned long		bar_border;
	unsigned long		bar_color;
	unsigned long		bar_font_color;
	unsigned long		color_focus;		/* XXX should this be per ws? */
	unsigned long		color_unfocus;
	char			bar_text[SWM_BAR_MAX];
};
struct swm_screen	*screens;
int			num_screens;
Window rootclick = 0;

struct ws_win		*cur_focus = NULL;

/* args to functions */
union arg {
	int			id;
#define SWM_ARG_ID_FOCUSNEXT	(0)
#define SWM_ARG_ID_FOCUSPREV	(1)
#define SWM_ARG_ID_FOCUSMAIN	(2)
#define SWM_ARG_ID_SWAPNEXT	(3)
#define SWM_ARG_ID_SWAPPREV	(4)
#define SWM_ARG_ID_SWAPMAIN	(5)
#define SWM_ARG_ID_MASTERSHRINK (6)
#define SWM_ARG_ID_MASTERGROW	(7)
#define SWM_ARG_ID_MASTERADD	(8)
#define SWM_ARG_ID_MASTERDEL	(9)
#define SWM_ARG_ID_STACKRESET	(10)
#define SWM_ARG_ID_STACKINIT	(11)
	char			**argv;
};

unsigned long
name_to_color(char *colorname)
{
	Colormap		cmap;
	Status			status;
	XColor			screen_def, exact_def;
	unsigned long		result = 0;
	char			cname[32] = "#";

	cmap = DefaultColormap(display, screens[0].idx);
	status = XAllocNamedColor(display, cmap, colorname,
	    &screen_def, &exact_def);
	if (!status) {
		strlcat(cname, colorname + 2, sizeof cname - 1);
		status = XAllocNamedColor(display, cmap, cname, &screen_def,
		    &exact_def);
	}
	if (status)
		result = screen_def.pixel;
	else
		fprintf(stderr, "color '%s' not found.\n", colorname);

	return (result);
}

/* conf file stuff */
#define	SWM_CONF_WS	"\n= \t"
#define SWM_CONF_FILE	"scrotwm.conf"
int
conf_load(char *filename)
{
	FILE			*config;
	char			*line, *cp, *var, *val;
	size_t			len, lineno = 0;
	int			i;

	DNPRINTF(SWM_D_MISC, "conf_load: filename %s\n", filename);

	if (filename == NULL)
		return (1);

	if ((config = fopen(filename, "r")) == NULL)
		return (1);

	for (;;) {
		if ((line = fparseln(config, &len, &lineno, NULL, 0)) == NULL)
			if (feof(config))
				break;
		cp = line;
		cp += (long)strspn(cp, SWM_CONF_WS);
		if (cp[0] == '\0') {
			/* empty line */
			free(line);
			continue;
		}
		if ((var = strsep(&cp, SWM_CONF_WS)) == NULL || cp == NULL)
			break;
		cp += (long)strspn(cp, SWM_CONF_WS);
		if ((val = strsep(&cp, SWM_CONF_WS)) == NULL)
			break;

		DNPRINTF(SWM_D_MISC, "conf_load: %s=%s\n",var ,val);
		switch (var[0]) {
		case 'b':
			if (!strncmp(var, "bar_enabled", strlen("bar_enabled")))
				bar_enabled = atoi(val);
			else if (!strncmp(var, "bar_border",
			    strlen("bar_border")))
				for (i = 0; i < ScreenCount(display); i++)
					screens[i].bar_border = name_to_color(val);
			else if (!strncmp(var, "bar_color",
			    strlen("bar_color")))
				for (i = 0; i < ScreenCount(display); i++)
					screens[i].bar_color = name_to_color(val);
			else if (!strncmp(var, "bar_font_color",
			    strlen("bar_font_color")))
				for (i = 0; i < ScreenCount(display); i++)
					screens[i].bar_font_color = name_to_color(val);
			else if (!strncmp(var, "bar_font", strlen("bar_font")))
				asprintf(&bar_fonts[0], "%s", val);
			else
				goto bad;
			break;

		case 'c':
			if (!strncmp(var, "color_focus", strlen("color_focus")))
				for (i = 0; i < ScreenCount(display); i++)
					screens[i].color_focus = name_to_color(val);
			else if (!strncmp(var, "color_unfocus",
			    strlen("color_unfocus")))
				for (i = 0; i < ScreenCount(display); i++)
					screens[i].color_unfocus = name_to_color(val);
			else
				goto bad;
			break;

		case 'd':
			if (!strncmp(var, "dialog_ratio",
			    strlen("dialog_ratio"))) {
				dialog_ratio = atof(val);
				if (dialog_ratio > 1.0 || dialog_ratio <= .3)
					dialog_ratio = .6;
			} else
				goto bad;
			break;

		case 's':
			if (!strncmp(var, "spawn_term", strlen("spawn_term")))
				asprintf(&spawn_term[0], "%s", val); /* XXX args? */
			break;
		default:
			goto bad;
		}
		free(line);
	}

	fclose(config);
	return (0);
bad:
	errx(1, "invalid conf file entry: %s=%s", var, val);
}

void
bar_print(struct swm_region *r, char *s, int erase)
{
	if (erase) {
		XSetForeground(display, bar_gc, r->s->bar_color);
		XDrawString(display, r->bar_window, bar_gc, 4, bar_fs->ascent,
		    r->s->bar_text, strlen(r->s->bar_text));
	}

	strlcpy(r->s->bar_text, s, sizeof r->s->bar_text);
	XSetForeground(display, bar_gc, r->s->bar_font_color);
	XDrawString(display, r->bar_window, bar_gc, 4, bar_fs->ascent,
	    r->s->bar_text, strlen(r->s->bar_text));
}

void
bar_update(void)
{
	time_t			tmt;
	struct tm		tm;
	struct swm_region	*r;
	int			i, x;
	char			s[SWM_BAR_MAX];
	char			e[SWM_BAR_MAX];
 
	if (bar_enabled == 0)
		return;

	time(&tmt);
	localtime_r(&tmt, &tm);
	strftime(s, sizeof s, "%a %b %d %R %Z %Y", &tm);
	for (i = 0; i < ScreenCount(display); i++) {
		x = 1;
		TAILQ_FOREACH(r, &screens[i].rl, entry) {
			snprintf(e, sizeof e, "%s     %d:%d",
			    s, x++, r->ws->idx + 1);
			bar_print(r, e, 1);
		}
	}
	XSync(display, False);
	alarm(60);
}

void
bar_signal(int sig)
{
	bar_alarm = 1;
}

void
bar_toggle(struct swm_region *r, union arg *args)
{
	struct swm_region	*tmpr;
	int			i, j;	

	DNPRINTF(SWM_D_MISC, "bar_toggle\n");

	if (bar_enabled) {
		for (i = 0; i < ScreenCount(display); i++)
			TAILQ_FOREACH(tmpr, &screens[i].rl, entry)
				XUnmapWindow(display, tmpr->bar_window);
	} else {
		for (i = 0; i < ScreenCount(display); i++)
			TAILQ_FOREACH(tmpr, &screens[i].rl, entry)
				XMapRaised(display, tmpr->bar_window);
	}
	bar_enabled = !bar_enabled;
	XSync(display, False);
	for (i = 0; i < ScreenCount(display); i++)
		for (j = 0; j < SWM_WS_MAX; j++)
			screens[i].ws[j].restack = 1;

	stack();
	/* must be after stack */
	for (i = 0; i < ScreenCount(display); i++)
		TAILQ_FOREACH(tmpr, &screens[i].rl, entry)
			bar_update();
}

void
bar_setup(struct swm_region *r)
{
	int			i;

	for (i = 0; bar_fonts[i] != NULL; i++) {
		bar_fs = XLoadQueryFont(display, bar_fonts[i]);
		if (bar_fs)
			break;
	}
	if (bar_fonts[i] == NULL)
			errx(1, "couldn't load font");
	bar_height = bar_fs->ascent + bar_fs->descent + 3;

	r->bar_window = XCreateSimpleWindow(display, 
	    r->s->root, X(r), Y(r), WIDTH(r) - 2, bar_height - 2,
	    1, r->s->bar_border, r->s->bar_color);
	bar_gc = XCreateGC(display, r->bar_window, 0, &bar_gcv);
	XSetFont(display, bar_gc, bar_fs->fid);
	XSelectInput(display, r->bar_window, VisibilityChangeMask);
	if (bar_enabled)
		XMapRaised(display, r->bar_window);
	DNPRINTF(SWM_D_MISC, "bar_setup: bar_window %lu\n", r->bar_window);

	if (signal(SIGALRM, bar_signal) == SIG_ERR)
		err(1, "could not install bar_signal");
	bar_update();
}

void
config_win(struct ws_win *win)
{
	XConfigureEvent		ce;

	DNPRINTF(SWM_D_MISC, "config_win: win %lu x %d y %d w %d h %d\n",
	    win->id, win->g.x, win->g.y, win->g.w, win->g.h);
	ce.type = ConfigureNotify;
	ce.display = display;
	ce.event = win->id;
	ce.window = win->id;
	ce.x = win->g.x;
	ce.y = win->g.y;
	ce.width = win->g.w;
	ce.height = win->g.h;
	ce.border_width = 1; /* XXX store this! */
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(display, win->id, False, StructureNotifyMask, (XEvent *)&ce);
}

int
count_win(struct workspace *ws, int count_transient)
{
	struct ws_win		*win;
	int			count = 0;

	TAILQ_FOREACH(win, &ws->winlist, entry) {
		if (count_transient == 0 && win->floating)
			continue;
		if (count_transient == 0 && win->transient)
			continue;
		count++;
	}
	DNPRINTF(SWM_D_MISC, "count_win: %d\n", count);

	return (count);
}

void
quit(struct swm_region *r, union arg *args)
{
	DNPRINTF(SWM_D_MISC, "quit\n");
	running = 0;
}

void
restart(struct swm_region *r, union arg *args)
{
	DNPRINTF(SWM_D_MISC, "restart: %s\n", start_argv[0]);

	/* disable alarm because the following code may not be interrupted */
	alarm(0);
	if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
		errx(1, "can't disable alarm");

	XCloseDisplay(display);
	execvp(start_argv[0], start_argv);
	fprintf(stderr, "execvp failed\n");
	perror(" failed");
	quit(NULL, NULL);
}

struct swm_region *
root_to_region(Window root)
{
	struct swm_region	*r;
	Window			rr, cr;
	int			i, x, y, wx, wy;
	unsigned int		mask;

	for (i = 0; i < ScreenCount(display); i++)
		if (screens[i].root == root)
			break;

	if (rootclick != root && /* if root was just clicked in, use cursor */
	    cur_focus && cur_focus->ws->r && cur_focus->s == &screens[i])
		r = cur_focus->ws->r;
	else {
		if (XQueryPointer(display, screens[i].root, 
		    &rr, &cr, &x, &y, &wx, &wy, &mask) == False) {
			r = TAILQ_FIRST(&screens[i].rl);
		} else {
			TAILQ_FOREACH(r, &screens[i].rl, entry) {
				if (x > X(r) && x < X(r) + WIDTH(r) &&
				    y > Y(r) && y < Y(r) + HEIGHT(r))
					break;
			}

			if (r == NULL)
				r = TAILQ_FIRST(&screens[i].rl);
		}
	}
	return (r);
}

struct ws_win *
find_window(Window id)
{
	struct ws_win		*win;
	int			i, j;

	for (i = 0; i < ScreenCount(display); i++)
		for (j = 0; j < SWM_WS_MAX; j++)
			TAILQ_FOREACH(win, &screens[i].ws[j].winlist, entry)
				if (id == win->id)
					return (win);
	return (NULL);
}

void
spawn(struct swm_region *r, union arg *args)
{
	DNPRINTF(SWM_D_MISC, "spawn: %s\n", args->argv[0]);
	/*
	 * The double-fork construct avoids zombie processes and keeps the code
	 * clean from stupid signal handlers.
	 */
	if (fork() == 0) {
		if (fork() == 0) {
			if (display)
				close(ConnectionNumber(display));
			setsid();
			execvp(args->argv[0], args->argv);
			fprintf(stderr, "execvp failed\n");
			perror(" failed");
		}
		exit(0);
	}
	wait(0);
}

void
unfocus_win(struct ws_win *win)
{
	DNPRINTF(SWM_D_FOCUS, "unfocus_win: id: %lu\n", win->id);
	if (win->ws->r && win->focus)
		XSetWindowBorder(display, win->id,
		    win->ws->r->s->color_unfocus);
	win->focus = 0;
	if (win->ws->focus == win)
		win->ws->focus = NULL;
	if (cur_focus == win)
		cur_focus = NULL;
}


void
focus_win(struct ws_win *win)
{
	DNPRINTF(SWM_D_FOCUS, "focus_win: id: %lu\n", win->id);

	rootclick = 0;

	win->ws->focus = win;
	if (win->ws->r != NULL) {
		if (cur_focus && cur_focus != win)
			unfocus_win(cur_focus);
		cur_focus = win;
		if (!win->focus)
			XSetWindowBorder(display, win->id,
			    win->ws->r->s->color_focus);
		win->focus = 1;
		XSetInputFocus(display, win->id,
		    RevertToPointerRoot, CurrentTime);
	}
}

void
switchws(struct swm_region *r, union arg *args)
{
	int			wsid = args->id;
	struct swm_region	*this_r, *other_r;
	struct ws_win		*win;
	struct workspace	*new_ws, *old_ws;

	this_r = r;
	old_ws = this_r->ws;
	new_ws = &this_r->s->ws[wsid];

	DNPRINTF(SWM_D_WS, "switchws screen %d region %dx%d+%d+%d: "
	    "%d -> %d\n", r->s->idx, WIDTH(r), HEIGHT(r), X(r), Y(r),
	    old_ws->idx, wsid);

	if (new_ws == old_ws)
		return;

	other_r = new_ws->r;
	if (!other_r) {
		/* if the other workspace is hidden, switch windows */
		/* map new window first to prevent ugly blinking */
		TAILQ_FOREACH(win, &new_ws->winlist, entry)
			XMapRaised(display, win->id);

		TAILQ_FOREACH(win, &old_ws->winlist, entry)
			XUnmapWindow(display, win->id);

		old_ws->r = NULL;
		old_ws->restack = 1;
	} else {
		other_r->ws = old_ws;
		old_ws->r = other_r;
	}
	this_r->ws = new_ws;
	new_ws->r = this_r;

	ignore_enter = 1;
	/* set focus */
	if (new_ws->focus)
		focus_win(new_ws->focus);
	stack();
	bar_update();
}

void
swapwin(struct swm_region *r, union arg *args)
{
	struct ws_win		*target;
	struct ws_win_list	*wl;


	DNPRINTF(SWM_D_WS, "swapwin id %d "
	    "in screen %d region %dx%d+%d+%d ws %d\n", args->id,
	    r->s->idx, WIDTH(r), HEIGHT(r), X(r), Y(r), r->ws->idx);
	if (cur_focus == NULL)
		return;

	wl = &cur_focus->ws->winlist;

	switch (args->id) {
	case SWM_ARG_ID_SWAPPREV:
		target = TAILQ_PREV(cur_focus, ws_win_list, entry);
		TAILQ_REMOVE(wl, cur_focus, entry);
		if (target == NULL)
			TAILQ_INSERT_TAIL(wl, cur_focus, entry);
		else
			TAILQ_INSERT_BEFORE(target, cur_focus, entry);
		break;
	case SWM_ARG_ID_SWAPNEXT: 
		target = TAILQ_NEXT(cur_focus, entry);
		TAILQ_REMOVE(wl, cur_focus, entry);
		if (target == NULL)
			TAILQ_INSERT_HEAD(wl, cur_focus, entry);
		else
			TAILQ_INSERT_AFTER(wl, target, cur_focus, entry);
		break;
	case SWM_ARG_ID_SWAPMAIN:
		target = TAILQ_FIRST(wl);
		if (target == cur_focus)
			return;
		TAILQ_REMOVE(wl, target, entry);
		TAILQ_INSERT_BEFORE(cur_focus, target, entry);
		TAILQ_REMOVE(wl, cur_focus, entry);
		TAILQ_INSERT_HEAD(wl, cur_focus, entry);
		break;
	default:
		DNPRINTF(SWM_D_MOVE, "invalid id: %d\n", args->id);
		return;
	}

	ignore_enter = 2;
	stack();
}

void
focus(struct swm_region *r, union arg *args)
{
	struct ws_win		*winfocus, *winlostfocus;
	struct ws_win_list	*wl;

	DNPRINTF(SWM_D_FOCUS, "focus: id %d\n", args->id);
	if (cur_focus == NULL)
		return;

	wl = &cur_focus->ws->winlist;

	winlostfocus = cur_focus;

	switch (args->id) {
	case SWM_ARG_ID_FOCUSPREV:
		winfocus = TAILQ_PREV(cur_focus, ws_win_list, entry);
		if (winfocus == NULL)
			winfocus = TAILQ_LAST(wl, ws_win_list);
		break;

	case SWM_ARG_ID_FOCUSNEXT:
		winfocus = TAILQ_NEXT(cur_focus, entry);
		if (winfocus == NULL)
			winfocus = TAILQ_FIRST(wl);
		break;

	case SWM_ARG_ID_FOCUSMAIN:
		winfocus = TAILQ_FIRST(wl);
		break;

	default:
		return;
	}

	if (winfocus == winlostfocus)
		return;

	focus_win(winfocus);
	XSync(display, False);
}

void
cycle_layout(struct swm_region *r, union arg *args)
{
	struct workspace	*ws = r->ws;

	DNPRINTF(SWM_D_EVENT, "cycle_layout: workspace: %d\n", ws->idx);

	ws->cur_layout++;
	if (ws->cur_layout->l_stack == NULL)
		ws->cur_layout = &layouts[0];
	ignore_enter = 1;

	stack();
}

void
stack_config(struct swm_region *r, union arg *args)
{
	struct workspace	*ws = r->ws;

	DNPRINTF(SWM_D_STACK, "stack_config for workspace %d (id %d\n",
	    args->id, ws->idx);

	if (ws->cur_layout->l_config != NULL)
		ws->cur_layout->l_config(ws, args->id);

	if (args->id != SWM_ARG_ID_STACKINIT);
		stack();
}

void
stack(void) {
	struct swm_geometry	g;
	struct swm_region	*r;
	int			i, j;

	DNPRINTF(SWM_D_STACK, "stack\n");

	for (i = 0; i < ScreenCount(display); i++) {
		j = 0;
		TAILQ_FOREACH(r, &screens[i].rl, entry) {
			DNPRINTF(SWM_D_STACK, "stacking workspace %d "
			    "(screen %d, region %d)\n", r->ws->idx, i, j++);

			/* start with screen geometry, adjust for bar */
			g = r->g;
			g.w -= 2;
			g.h -= 2;
			if (bar_enabled) {
				g.y += bar_height;
				g.h -= bar_height;
			} 

			r->ws->restack = 0;
			r->ws->cur_layout->l_stack(r->ws, &g);
		}
	}
	XSync(display, False);
}

void
stack_floater(struct ws_win *win, struct swm_region *r)
{
	unsigned int		mask;
	XWindowChanges		wc;

	bzero(&wc, sizeof wc);
	mask = CWX | CWY | CWBorderWidth | CWWidth | CWHeight;
	wc.border_width = 1;
	if (win->transient) {
		win->g.w = (double)WIDTH(r) * dialog_ratio;
		win->g.h = (double)HEIGHT(r) * dialog_ratio;
	}
	wc.width = win->g.w;
	wc.height = win->g.h;
	wc.x = (WIDTH(r) - win->g.w) / 2;
	wc.y = (HEIGHT(r) - win->g.h) / 2;

	DNPRINTF(SWM_D_STACK, "stack_floater: win %lu x %d y %d w %d h %d\n",
	    win->id, wc.x, wc.y, wc.width, wc.height);

	XConfigureWindow(display, win->id, mask, &wc);
}


void
vertical_config(struct workspace *ws, int id)
{
	DNPRINTF(SWM_D_STACK, "vertical_resize: workspace: %d\n", ws->idx);

	switch (id) {
	case SWM_ARG_ID_STACKRESET:
	case SWM_ARG_ID_STACKINIT:
		ws->l_state.vertical_msize = SWM_V_SLICE / 2;
		ws->l_state.vertical_mwin = 1;
		break;
	case SWM_ARG_ID_MASTERSHRINK:
		if (ws->l_state.vertical_msize > 1)
			ws->l_state.vertical_msize--;
		break;
	case SWM_ARG_ID_MASTERGROW:
		if (ws->l_state.vertical_msize < SWM_V_SLICE - 1)
			ws->l_state.vertical_msize++;
		break;
	case SWM_ARG_ID_MASTERADD:
		ws->l_state.vertical_mwin++;
		break;
	case SWM_ARG_ID_MASTERDEL:
		if (ws->l_state.vertical_mwin > 0)
			ws->l_state.vertical_mwin--;
		break;
	default:
		return;
	}
}

void
vertical_stack(struct workspace *ws, struct swm_geometry *g) {
	XWindowChanges		wc;
	struct swm_geometry	gg = *g;
	struct ws_win		*win, *winfocus;
	int			i, j, split, colno, hrh, winno, main_width;
	unsigned int		mask;

	DNPRINTF(SWM_D_STACK, "vertical_stack: workspace: %d\n", ws->idx);

	if ((winno = count_win(ws, 0)) == 0)
		return;

	if (ws->focus == NULL)
		ws->focus = TAILQ_FIRST(&ws->winlist);
	winfocus = cur_focus ? cur_focus : ws->focus;

	if (ws->l_state.vertical_mwin &&
	    winno > ws->l_state.vertical_mwin) {
		split = ws->l_state.vertical_mwin;
		colno = split;
		main_width = (g->w / SWM_V_SLICE) *
		   ws->l_state.vertical_msize;
		gg.w = main_width;
	} else {
		colno = winno;
		split = 0;
	}
	hrh = g->h / colno;
	gg.h = hrh - 2;

	i = j = 0;
	TAILQ_FOREACH(win, &ws->winlist, entry) {
		if (split && i == split) {
			colno = winno - split;
			hrh = (g->h / colno);
			gg.x += main_width + 2;
			gg.w = g->w - (main_width + 2);
			gg.h = hrh - 2;
			j = 0;
		} else if (j == colno - 1)
			gg.h = (hrh + (g->h - (colno * hrh)));
		 
		if (j == 0)
			gg.y = g->y;
		else
			gg.y += hrh;


		if (win->transient != 0 || win->floating != 0)
			stack_floater(win, ws->r);
		else {
			bzero(&wc, sizeof wc);
			wc.border_width = 1;
			win->g.x = wc.x = gg.x;
			win->g.y = wc.y = gg.y;
			win->g.w = wc.width = gg.w;
			win->g.h = wc.height = gg.h;
			mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
			XConfigureWindow(display, win->id, mask, &wc);
		}

		XMapRaised(display, win->id);
		i++;
		j++;
	}

	if (winfocus)
		focus_win(winfocus); /* has to be done outside of the loop */
}


void
horizontal_config(struct workspace *ws, int id)
{
	DNPRINTF(SWM_D_STACK, "horizontal_config: workspace: %d\n", ws->idx);

	switch (id) {
	case SWM_ARG_ID_STACKRESET:
	case SWM_ARG_ID_STACKINIT:
		ws->l_state.horizontal_mwin = 1;
		ws->l_state.horizontal_msize = SWM_H_SLICE / 2;
		break;
	case SWM_ARG_ID_MASTERSHRINK:
		if (ws->l_state.horizontal_msize > 1)
			ws->l_state.horizontal_msize--;
		break;
	case SWM_ARG_ID_MASTERGROW:
		if (ws->l_state.horizontal_msize < SWM_H_SLICE - 1)
			ws->l_state.horizontal_msize++;
		break;
	case SWM_ARG_ID_MASTERADD:
		ws->l_state.horizontal_mwin++;
		break;
	case SWM_ARG_ID_MASTERDEL:
		if (ws->l_state.horizontal_mwin > 0)
			ws->l_state.horizontal_mwin--;
		break;
	default:
		return;
	}
}

void
horizontal_stack(struct workspace *ws, struct swm_geometry *g) {
	XWindowChanges		wc;
	struct swm_geometry	gg = *g;
	struct ws_win		*win, *winfocus;
	int			i, j, split, rowno, hrw, winno, main_height;
	unsigned int		mask;

	DNPRINTF(SWM_D_STACK, "horizontal_stack: workspace: %d\n", ws->idx);

	if ((winno = count_win(ws, 0)) == 0)
		return;

	if (ws->focus == NULL)
		ws->focus = TAILQ_FIRST(&ws->winlist);
	winfocus = cur_focus ? cur_focus : ws->focus;

	if (ws->l_state.horizontal_mwin &&
	    winno > ws->l_state.horizontal_mwin) {
		split = ws->l_state.horizontal_mwin;
		rowno = split;
		main_height = (g->h / SWM_V_SLICE) *
		   ws->l_state.horizontal_msize;
		gg.h = main_height;
	} else {
		rowno = winno;
		split = 0;
	}
	hrw = g->w / rowno;
	gg.w = hrw - 2;

	i = j = 0;
	TAILQ_FOREACH(win, &ws->winlist, entry) {
		if (split && i == split) {
			rowno = winno - split;
			hrw = (g->w / rowno);
			gg.y += main_height + 2;
			gg.h = g->h - (main_height + 2);
			gg.w = hrw - 2;
			j = 0;
		} else if (j == rowno - 1)
			gg.w = (hrw + (g->w - (rowno * hrw)));

		if (j == 0)
			gg.x = g->x;
		else
			gg.x += hrw;

		if (win->transient != 0 || win->floating != 0)
			stack_floater(win, ws->r);
		else {
			bzero(&wc, sizeof wc);
			wc.border_width = 1;
			win->g.x = wc.x = gg.x;
			win->g.y = wc.y = gg.y;
			win->g.w = wc.width = gg.w;
			win->g.h = wc.height = gg.h;
			mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
			XConfigureWindow(display, win->id, mask, &wc);
		}

		XMapRaised(display, win->id);
		j++;
		i++;
	}

	if (winfocus)
		focus_win(winfocus); /* this has to be done outside of the loop */
}

/* fullscreen view */
void
max_stack(struct workspace *ws, struct swm_geometry *g) {
	XWindowChanges		wc;
	struct swm_geometry	gg = *g;
	struct ws_win		*win, *winfocus;
	unsigned int		mask;

	DNPRINTF(SWM_D_STACK, "max_stack: workspace: %d\n", ws->idx);

	if (count_win(ws, 0) == 0)
		return;

	if (ws->focus == NULL)
		ws->focus = TAILQ_FIRST(&ws->winlist);
	winfocus = cur_focus ? cur_focus : ws->focus;

	TAILQ_FOREACH(win, &ws->winlist, entry) {
		if (win->transient != 0 || win->floating != 0) {
			if (win == ws->focus) {
				/* XXX maximize? */
				stack_floater(win, ws->r);
				XMapRaised(display, win->id);
			} else
				XUnmapWindow(display, win->id);
		} else {
			bzero(&wc, sizeof wc);
			wc.border_width = 1;
			win->g.x = wc.x = gg.x;
			win->g.y = wc.y = gg.y;
			win->g.w = wc.width = gg.w;
			win->g.h = wc.height = gg.h;
			mask = CWX | CWY | CWWidth | CWHeight | CWBorderWidth;
			XConfigureWindow(display, win->id, mask, &wc);

			if (win == ws->focus) {
				XMapRaised(display, win->id);
			} else
				XUnmapWindow(display, win->id);
		}
	}

	if (winfocus)
		focus_win(winfocus); /* has to be done outside of the loop */
}

void
send_to_ws(struct swm_region *r, union arg *args)
{
	int			wsid = args->id;
	struct ws_win		*win = cur_focus;
	struct workspace	*ws, *nws;

	DNPRINTF(SWM_D_MOVE, "send_to_ws: win: %lu\n", win->id);

	ws = win->ws;
	nws = &win->s->ws[wsid];

	XUnmapWindow(display, win->id);

	/* find a window to focus */
	ws->focus = TAILQ_PREV(win, ws_win_list, entry);
	if (ws->focus == NULL)
		ws->focus = TAILQ_FIRST(&ws->winlist);
	if (ws->focus == win)
		ws->focus = NULL;

	TAILQ_REMOVE(&ws->winlist, win, entry);

	TAILQ_INSERT_TAIL(&nws->winlist, win, entry);
	win->ws = nws;

	if (count_win(nws, 1) == 1)
		nws->focus = win;
	ws->restack = 1;
	nws->restack = 1;

	stack();
}

/* key definitions */
struct key {
	unsigned int		mod;
	KeySym			keysym;
	void			(*func)(struct swm_region *r, union arg *);
	union arg		args;
} keys[] = {
	/* modifier		key	function		argument */
	{ MODKEY,		XK_space,	cycle_layout,	{0} }, 
	{ MODKEY | ShiftMask,	XK_space,	stack_config,	{.id = SWM_ARG_ID_STACKRESET} }, 
	{ MODKEY,		XK_h,		stack_config,	{.id = SWM_ARG_ID_MASTERSHRINK} },
	{ MODKEY,		XK_l,		stack_config,	{.id = SWM_ARG_ID_MASTERGROW} },
	{ MODKEY,		XK_comma,	stack_config,	{.id = SWM_ARG_ID_MASTERADD} },
	{ MODKEY,		XK_period,	stack_config,	{.id = SWM_ARG_ID_MASTERDEL} },
	{ MODKEY,		XK_Return,	swapwin,	{.id = SWM_ARG_ID_SWAPMAIN} },
	{ MODKEY,		XK_j,		focus,		{.id = SWM_ARG_ID_FOCUSNEXT} },
	{ MODKEY,		XK_k,		focus,		{.id = SWM_ARG_ID_FOCUSPREV} },
	{ MODKEY | ShiftMask,	XK_j,		swapwin,	{.id = SWM_ARG_ID_SWAPNEXT} },
	{ MODKEY | ShiftMask,	XK_k,		swapwin,	{.id = SWM_ARG_ID_SWAPPREV} },
	{ MODKEY | ShiftMask,	XK_Return,	spawn,		{.argv = spawn_term} },
	{ MODKEY,		XK_p,		spawn,		{.argv = spawn_menu} },
	{ MODKEY | ShiftMask,	XK_q,		quit,		{0} },
	{ MODKEY,		XK_q,		restart,	{0} },
	{ MODKEY,		XK_m,		focus,		{.id = SWM_ARG_ID_FOCUSMAIN} },
	{ MODKEY,		XK_1,		switchws,	{.id = 0} },
	{ MODKEY,		XK_2,		switchws,	{.id = 1} },
	{ MODKEY,		XK_3,		switchws,	{.id = 2} },
	{ MODKEY,		XK_4,		switchws,	{.id = 3} },
	{ MODKEY,		XK_5,		switchws,	{.id = 4} },
	{ MODKEY,		XK_6,		switchws,	{.id = 5} },
	{ MODKEY,		XK_7,		switchws,	{.id = 6} },
	{ MODKEY,		XK_8,		switchws,	{.id = 7} },
	{ MODKEY,		XK_9,		switchws,	{.id = 8} },
	{ MODKEY,		XK_0,		switchws,	{.id = 9} },
	{ MODKEY | ShiftMask,	XK_1,		send_to_ws,	{.id = 0} },
	{ MODKEY | ShiftMask,	XK_2,		send_to_ws,	{.id = 1} },
	{ MODKEY | ShiftMask,	XK_3,		send_to_ws,	{.id = 2} },
	{ MODKEY | ShiftMask,	XK_4,		send_to_ws,	{.id = 3} },
	{ MODKEY | ShiftMask,	XK_5,		send_to_ws,	{.id = 4} },
	{ MODKEY | ShiftMask,	XK_6,		send_to_ws,	{.id = 5} },
	{ MODKEY | ShiftMask,	XK_7,		send_to_ws,	{.id = 6} },
	{ MODKEY | ShiftMask,	XK_8,		send_to_ws,	{.id = 7} },
	{ MODKEY | ShiftMask,	XK_9,		send_to_ws,	{.id = 8} },
	{ MODKEY | ShiftMask,	XK_0,		send_to_ws,	{.id = 9} },
	{ MODKEY,		XK_b,		bar_toggle,	{0} },
	{ MODKEY,		XK_Tab,		focus,		{.id = SWM_ARG_ID_FOCUSNEXT} },
	{ MODKEY | ShiftMask,	XK_Tab,		focus,		{.id = SWM_ARG_ID_FOCUSPREV} },
};

void
updatenumlockmask(void)
{
	unsigned int		i, j;
	XModifierKeymap		*modmap;

	DNPRINTF(SWM_D_MISC, "updatenumlockmask\n");
	numlockmask = 0;
	modmap = XGetModifierMapping(display);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
			  == XKeysymToKeycode(display, XK_Num_Lock))
				numlockmask = (1 << i);

	XFreeModifiermap(modmap);
}

void
grabkeys(void)
{
	unsigned int		i, j, k;
	KeyCode			code;
	unsigned int		modifiers[] =
	    { 0, LockMask, numlockmask, numlockmask | LockMask };

	DNPRINTF(SWM_D_MISC, "grabkeys\n");
	updatenumlockmask();

	for (k = 0; k < ScreenCount(display); k++) {
		if (TAILQ_EMPTY(&screens[k].rl))
			continue;
		XUngrabKey(display, AnyKey, AnyModifier, screens[k].root);
		for (i = 0; i < LENGTH(keys); i++) {
			if ((code = XKeysymToKeycode(display, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(display, code,
					    keys[i].mod | modifiers[j],
					    screens[k].root, True,
					    GrabModeAsync, GrabModeAsync);
		}
	}
}
void
expose(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "expose: window: %lu\n", e->xexpose.window);
}

void
keypress(XEvent *e)
{
	unsigned int		i;
	KeySym			keysym;
	XKeyEvent		*ev = &e->xkey;

	DNPRINTF(SWM_D_EVENT, "keypress: window: %lu\n", ev->window);

	keysym = XKeycodeToKeysym(display, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		   && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		   && keys[i].func)
			keys[i].func(root_to_region(ev->root),
			    &(keys[i].args));
}

void
buttonpress(XEvent *e)
{
	XButtonPressedEvent	*ev = &e->xbutton;
#ifdef SWM_CLICKTOFOCUS
	struct ws_win		*win;
	struct workspace	*ws;
	struct swm_region	*r;
#endif

	DNPRINTF(SWM_D_EVENT, "buttonpress: window: %lu\n", ev->window);

	if (ev->window == ev->root) {
		rootclick = ev->root;
		return;
	}
	if (ev->window == cur_focus->id)
		return;
#ifdef SWM_CLICKTOFOCUS
	r = root_to_region(ev->root);
	ws = r->ws;
	TAILQ_FOREACH(win, &ws->winlist, entry)
		if (win->id == ev->window) {
			/* focus in the clicked window */
			XSetWindowBorder(display, ev->window, 0xff0000);
			XSetWindowBorder(display, ws->focus->id, 0x888888);
			XSetInputFocus(display, ev->window, RevertToPointerRoot,
			    CurrentTime);
			ws->focus = win;
			XSync(display, False);
			break;
	}
#endif
}

void
set_win_state(struct ws_win *win, long state)
{
	long			data[] = {state, None};

	DNPRINTF(SWM_D_EVENT, "set_win_state: window: %lu\n", win->id);

	XChangeProperty(display, win->id, astate, astate, 32, PropModeReplace,
	    (unsigned char *)data, 2);
}

struct ws_win *
manage_window(Window id, struct workspace *ws)
{
	Window			trans;
	struct ws_win		*win;
	XClassHint		ch;

	TAILQ_FOREACH(win, &ws->winlist, entry) {
		if (win->id == id)
			return (win);	/* already being managed */
	}

	if ((win = calloc(1, sizeof(struct ws_win))) == NULL)
		errx(1, "calloc: failed to allocate memory for new window");

	win->id = id;
	win->ws = ws;
	win->s = ws->r->s;	/* this never changes */
	TAILQ_INSERT_TAIL(&ws->winlist, win, entry);

	/* make new win focused */
	focus_win(win);

	XGetTransientForHint(display, win->id, &trans);
	if (trans) {
		win->transient = trans;
		DNPRINTF(SWM_D_MISC, "manage_window: win %u transient %u\n",
		    (unsigned)win->id, win->transient);
	}
	XGetWindowAttributes(display, id, &win->wa);
	win->g.w = win->wa.width;
	win->g.h = win->wa.height;
	win->g.x = win->wa.x;
	win->g.y = win->wa.y;

	/* XXX make this a table */
	bzero(&ch, sizeof ch);
	if (XGetClassHint(display, win->id, &ch)) {
		/*fprintf(stderr, "class: %s name: %s\n", ch.res_class, ch.res_name); */
		if (!strcmp(ch.res_class, "MPlayer") && !strcmp(ch.res_name, "xv")) {
			win->floating = 1;
		}
		if (ch.res_class)
			XFree(ch.res_class);
		if (ch.res_name)
			XFree(ch.res_name);
	}

	XSelectInput(display, id, EnterWindowMask | FocusChangeMask |
	    PropertyChangeMask | StructureNotifyMask);

	set_win_state(win, NormalState);

	return (win);
}

void
configurerequest(XEvent *e)
{
	XConfigureRequestEvent	*ev = &e->xconfigurerequest;
	struct ws_win		*win;
	int			new = 1;
	XWindowChanges		wc;

	if ((win = find_window(ev->window)) == NULL)
		new = 1;

	if (new) {
		DNPRINTF(SWM_D_EVENT, "configurerequest: new window: %lu\n",
		    ev->window);
		bzero(&wc, sizeof wc);
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(display, ev->window, ev->value_mask, &wc);
	} else {
		DNPRINTF(SWM_D_EVENT, "configurerequest: change window: %lu\n",
		    ev->window);
		if (win->floating) {
			if (ev->value_mask & CWX)
				win->g.x = ev->x;
			if (ev->value_mask & CWY)
				win->g.y = ev->y;
			if (ev->value_mask & CWWidth)
				win->g.w = ev->width;
			if (ev->value_mask & CWHeight)
				win->g.h = ev->height;
			if (win->ws->r != NULL) {
				/* this seems to be full screen */
				if (win->g.w > WIDTH(win->ws->r)) {
					/* kill border */
					win->g.x -= 1;
					win->g.w += 1;
				}
				if (win->g.h > HEIGHT(win->ws->r)) {
					/* kill border */
					win->g.y -= 1;
					win->g.h += 1;
				}
			}
			if ((ev->value_mask & (CWX|CWY)) &&
			    !(ev->value_mask & (CWWidth|CWHeight)))
				config_win(win);
			XMoveResizeWindow(display, win->id,
			    win->g.x, win->g.y, win->g.w, win->g.h);
		} else
			config_win(win);
	}
}

void
configurenotify(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "configurenotify: window: %lu\n",
	    e->xconfigure.window);
}

void
destroynotify(XEvent *e)
{
	struct ws_win		*win;
	XDestroyWindowEvent	*ev = &e->xdestroywindow;

	DNPRINTF(SWM_D_EVENT, "destroynotify: window %lu\n", ev->window);

	if ((win = find_window(ev->window)) != NULL) {
		struct workspace *ws = win->ws;
		/* find a window to focus */
		if (ws->focus == win)
			ws->focus = TAILQ_PREV(win, ws_win_list, entry);
		if (ws->focus == NULL)
			ws->focus = TAILQ_FIRST(&ws->winlist);
		if (ws->focus == win)
			ws->focus = NULL;
		if (cur_focus == win) {
			if (ws->focus == NULL) 
				unfocus_win(win); /* XXX focus another ws? */
			else
				focus_win(ws->focus);
		}

		TAILQ_REMOVE(&ws->winlist, win, entry);
		set_win_state(win, WithdrawnState);
		free(win);
	}
	stack();
}

void
enternotify(XEvent *e)
{
	XCrossingEvent		*ev = &e->xcrossing;
	struct ws_win		*win;
	int			i, j;

	DNPRINTF(SWM_D_EVENT, "enternotify: window: %lu\n", ev->window);

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
	    ev->window != ev->root)
		return;
	if (ignore_enter) {
		/* eat event(r) to prevent autofocus */
		ignore_enter--;
		return;
	}
	/* brute force for now */
	for (i = 0; i < ScreenCount(display); i++) {
		for (j = 0; j < SWM_WS_MAX; j++) {
			TAILQ_FOREACH(win, &screens[i].ws[j].winlist , entry) {
				if (win->id == ev->window)
					focus_win(win);
			}
		}
	}
}

void
focusin(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "focusin: window: %lu\n", e->xfocus.window);

	/* XXX this probably needs better handling now.
	if (ev->window == ev->root)
		return;
	*/
	/*
	 * kill grab for now so that we can cut and paste , this screws up
	 * click to focus
	 */
	/*
	DNPRINTF(SWM_D_EVENT, "focusin: window: %lu grabbing\n", ev->window);
	XGrabButton(display, Button1, AnyModifier, ev->window, False,
	    ButtonPress, GrabModeAsync, GrabModeSync, None, None);
	*/
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent		*ev = &e->xmapping;

	DNPRINTF(SWM_D_EVENT, "mappingnotify: window: %lu\n", ev->window);

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	XMapRequestEvent	*ev = &e->xmaprequest;
	XWindowAttributes	wa;
	struct swm_region	*r;

	DNPRINTF(SWM_D_EVENT, "maprequest: window: %lu\n",
	    e->xmaprequest.window);

	if (!XGetWindowAttributes(display, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	r = root_to_region(wa.root);
	manage_window(e->xmaprequest.window, r->ws);
	stack();
}

void
propertynotify(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "propertynotify: window: %lu\n",
	    e->xproperty.window);
}

void
unmapnotify(XEvent *e)
{
	DNPRINTF(SWM_D_EVENT, "unmapnotify: window: %lu\n", e->xunmap.window);
}

void
visibilitynotify(XEvent *e)
{
	int			i;
	struct swm_region	*r;

	DNPRINTF(SWM_D_EVENT, "visibilitynotify: window: %lu\n",
	    e->xvisibility.window);
	if (e->xvisibility.state == VisibilityUnobscured)
		for (i = 0; i < ScreenCount(display); i++) 
			TAILQ_FOREACH(r, &screens[i].rl, entry)
				if (e->xvisibility.window == r->bar_window)
					bar_update();
}

void			(*handler[LASTEvent])(XEvent *) = {
				[Expose] = expose,
				[KeyPress] = keypress,
				[ButtonPress] = buttonpress,
				[ConfigureRequest] = configurerequest,
				[ConfigureNotify] = configurenotify,
				[DestroyNotify] = destroynotify,
				[EnterNotify] = enternotify,
				[FocusIn] = focusin,
				[MappingNotify] = mappingnotify,
				[MapRequest] = maprequest,
				[PropertyNotify] = propertynotify,
				[UnmapNotify] = unmapnotify,
				[VisibilityNotify] = visibilitynotify,
};

int
xerror_start(Display *d, XErrorEvent *ee)
{
	other_wm = 1;
	return (-1);
}

int
xerror(Display *d, XErrorEvent *ee)
{
	/* fprintf(stderr, "error: %p %p\n", display, ee); */
	return (-1);
}

int
active_wm(void)
{
	other_wm = 0;
	xerrorxlib = XSetErrorHandler(xerror_start);

	/* this causes an error if some other window manager is running */
	XSelectInput(display, DefaultRootWindow(display),
	    SubstructureRedirectMask);
	XSync(display, False);
	if (other_wm)
		return (1);

	XSetErrorHandler(xerror);
	XSync(display, False);
	return (0);
}

long
getstate(Window w)
{
	int			format, status;
	long			result = -1;
	unsigned char		*p = NULL;
	unsigned long		n, extra;
	Atom			real;

	astate = XInternAtom(display, "WM_STATE", False);
	status = XGetWindowProperty(display, w, astate, 0L, 2L, False, astate,
	    &real, &format, &n, &extra, (unsigned char **)&p);
	if (status != Success)
		return (-1);
	if (n != 0)
		result = *p;
	XFree(p);
	return (result);
}

void
new_region(struct swm_screen *s, struct workspace *ws,
    int x, int y, int w, int h)
{
	struct swm_region	*r;

	DNPRINTF(SWM_D_MISC, "new region on screen %d: %dx%d (%d, %d)\n",
	     s->idx, x, y, w, h);

	if ((r = calloc(1, sizeof(struct swm_region))) == NULL)
		errx(1, "calloc: failed to allocate memory for screen");

	X(r) = x;
	Y(r) = y;
	WIDTH(r) = w;
	HEIGHT(r) = h;
	r->s = s;
	r->ws = ws;
	ws->r = r;
	TAILQ_INSERT_TAIL(&s->rl, r, entry);
	bar_setup(r);
}

void
setup_screens(void)
{
#ifdef SWM_XRR_HAS_CRTC
	XRRCrtcInfo		*ci;
	XRRScreenResources	*sr;
	int			c;
#endif /* SWM_XRR_HAS_CRTC */
	Window			d1, d2, *wins = NULL;
	XWindowAttributes	wa;
	struct swm_region	*r;
	unsigned int		no;
	int			errorbase, major, minor;
	int			ncrtc = 0, w = 0;
        int			i, j, k;
	struct workspace	*ws;

	if ((screens = calloc(ScreenCount(display),
	     sizeof(struct swm_screen))) == NULL)
		errx(1, "calloc: screens");

	/* map physical screens */
	for (i = 0; i < ScreenCount(display); i++) {
		DNPRINTF(SWM_D_WS, "setup_screens: init screen %d\n", i);
		screens[i].idx = i;
		TAILQ_INIT(&screens[i].rl);
		screens[i].root = RootWindow(display, i);
		XGetWindowAttributes(display, screens[i].root, &wa);
		XSelectInput(display, screens[i].root,
		    ButtonPressMask | wa.your_event_mask);

		/* set default colors */
		screens[i].color_focus = name_to_color("red");
		screens[i].color_unfocus = name_to_color("rgb:88/88/88");
		screens[i].bar_border = name_to_color("rgb:00/80/80");
		screens[i].bar_color = name_to_color("black");
		screens[i].bar_font_color = name_to_color("rgb:a0/a0/a0");

		/* init all workspaces */
		for (j = 0; j < SWM_WS_MAX; j++) {
			ws = &screens[i].ws[j];
			ws->idx = j;
			ws->restack = 1;
			ws->focus = NULL;
			ws->r = NULL;
			TAILQ_INIT(&ws->winlist);

			for (k = 0; layouts[k].l_stack != NULL; k++)
				if (layouts[k].l_config != NULL)
					layouts[k].l_config(ws,
					    SWM_ARG_ID_STACKINIT);
			ws->cur_layout = &layouts[0];
		}

		/* map virtual screens onto physical screens */
		screens[i].xrandr_support = XRRQueryExtension(display,
		    &xrandr_eventbase, &errorbase);
		if (screens[i].xrandr_support)
			if (XRRQueryVersion(display, &major, &minor) &&
			    major < 1)
                        	screens[i].xrandr_support = 0;

#if 0	/* not ready for dynamic screen changes */
		if (screens[i].xrandr_support)
			XRRSelectInput(display,
			    screens[r->s].root,
			    RRScreenChangeNotifyMask);
#endif

		/* grab existing windows (before we build the bars)*/
		if (!XQueryTree(display, screens[i].root, &d1, &d2, &wins, &no))
			continue;

#ifdef SWM_XRR_HAS_CRTC
		sr = XRRGetScreenResources(display, screens[i].root);
		if (sr == NULL)
			new_region(&screens[i], &screens[i].ws[w],
			    0, 0, DisplayWidth(display, i),
			    DisplayHeight(display, i)); 
		else 
			ncrtc = sr->ncrtc;

		for (c = 0; c < ncrtc; c++) {
			ci = XRRGetCrtcInfo(display, sr, sr->crtcs[c]);
			if (ci->noutput == 0)
				continue;

			if (ci != NULL && ci->mode == None)
				new_region(&screens[i], &screens[i].ws[w], 0, 0,
				    DisplayWidth(display, i),
				    DisplayHeight(display, i)); 
			else
				new_region(&screens[i], &screens[i].ws[w],
				    ci->x, ci->y, ci->width, ci->height);
			w++;
		}
		XRRFreeCrtcInfo(ci);
		XRRFreeScreenResources(sr);
#else
		new_region(&screens[i], &screens[i].ws[w], 0, 0,
		    DisplayWidth(display, i),
		    DisplayHeight(display, i)); 
#endif /* SWM_XRR_HAS_CRTC */

		/* attach windows to a region */
		/* normal windows */
		if ((r = TAILQ_FIRST(&screens[i].rl)) == NULL)
			errx(1, "no regions on screen %d", i);

		for (i = 0; i < no; i++) {
                        XGetWindowAttributes(display, wins[i], &wa);
			if (!XGetWindowAttributes(display, wins[i], &wa) ||
			    wa.override_redirect ||
			    XGetTransientForHint(display, wins[i], &d1))
				continue;

			if (wa.map_state == IsViewable ||
			    getstate(wins[i]) == NormalState)
				manage_window(wins[i], r->ws);
		}
		/* transient windows */
		for (i = 0; i < no; i++) {
			if (!XGetWindowAttributes(display, wins[i], &wa))
				continue;

			if (XGetTransientForHint(display, wins[i], &d1) &&
			    (wa.map_state == IsViewable || getstate(wins[i]) ==
			    NormalState))
				manage_window(wins[i], r->ws);
                }
                if (wins) {
                        XFree(wins);
			wins = NULL;
		}
	}
}

int
main(int argc, char *argv[])
{
	struct passwd		*pwd;
	char			conf[PATH_MAX], *cfile = NULL;
	struct stat		sb;
	XEvent			e;
	int			xfd;
	fd_set			rd;

	start_argv = argv;
	fprintf(stderr, "Welcome to scrotwm V%s\n", SWM_VERSION);
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		warnx("no locale support");

	if (!(display = XOpenDisplay(0)))
		errx(1, "can not open display");

	if (active_wm())
		errx(1, "other wm running");

	astate = XInternAtom(display, "WM_STATE", False);

	/* look for local and global conf file */
	pwd = getpwuid(getuid());
	if (pwd == NULL)
		errx(1, "invalid user %d", getuid());

	snprintf(conf, sizeof conf, "%s/.%s", pwd->pw_dir, SWM_CONF_FILE);
	if (stat(conf, &sb) != -1) {
		if (S_ISREG(sb.st_mode))
			cfile = conf;
	} else {
		/* try global conf file */
		snprintf(conf, sizeof conf, "/etc/%s", SWM_CONF_FILE);
		if (!stat(conf, &sb))
			if (S_ISREG(sb.st_mode))
				cfile = conf;
	}

	setup_screens();

	if (cfile)
		conf_load(cfile);


	/* ws[0].focus = TAILQ_FIRST(&ws[0].winlist); */

	grabkeys();
	stack();

	xfd = ConnectionNumber(display);
	while (running) {
		FD_SET(xfd, &rd);
		if (select(xfd + 1, &rd, NULL, NULL, NULL) == -1)
			if (errno != EINTR)
				errx(1, "select failed");
		if (bar_alarm) {
			bar_alarm = 0;
			bar_update();
		}
		while(XPending(display)) {
			XNextEvent(display, &e);
			if (handler[e.type])
				handler[e.type](&e);
		}
	}

	XCloseDisplay(display);

	return (0);
}
