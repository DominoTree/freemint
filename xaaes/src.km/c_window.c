/*
 * $Id$
 *
 * XaAES - XaAES Ain't the AES (c) 1992 - 1998 C.Graham
 *                                 1999 - 2003 H.Robbers
 *                                        2004 F.Naumann & O.Skancke
 *
 * A multitasking AES replacement for FreeMiNT
 *
 * This file is part of XaAES.
 *
 * XaAES is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * XaAES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with XaAES; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "c_window.h"
#include "xa_global.h"

#include "app_man.h"
#include "desktop.h"
#include "k_main.h"
#include "menuwidg.h"
#include "objects.h"
#include "rectlist.h"
#include "scrlobjc.h"
#include "widgets.h"
#include "xa_graf.h"

#include "mint/signal.h"

/*
 * Window Stack Management Functions
 */

/* HR:
 * I hated the kind of runaway behaviour (return wind_handle++)
 * So here is my solution:
 * Window handle bit array (used to generate unique window handles)
 * As this fills up, there may be a problem if a user opens
 * more than MAX_WINDOWS windows concurrently :-)
 */

static unsigned long
btst(unsigned long word, unsigned int bit)
{
	unsigned long mask = 1UL << bit;

	return (word & mask);
}
static void
bclr(unsigned long *word, unsigned int bit)
{
	unsigned long mask = 1UL << bit;

	*word &= ~mask;
}
static void
bset(unsigned long *word, unsigned int bit)
{
	unsigned long mask = 1UL << bit;

	*word |= mask;
}

static unsigned long wind_handle[MAX_WINDOWS / LBITS];
static const unsigned int words = MAX_WINDOWS / LBITS;

static int
new_wind_handle(void)
{
	unsigned int word;
	unsigned int l;

	for (word = 0; word < words; word++)
		if (wind_handle[word] != 0xffffffffUL)
			break;

#if XXX
	assert(word < words);
#endif

	l = 0;
	while (btst(wind_handle[word], l))
		l++;

	bset(wind_handle+word, l);
	return word * LBITS + l;
}

static void
free_wind_handle(int h)
{
	unsigned int word = h / LBITS;

	bclr(wind_handle+word, h % LBITS);
}

void
clear_wind_handles(void)
{
	unsigned int f;
	
	for (f = 0; f < words; f++)
		wind_handle[f] = 0;
}

/*
 * Find first free position for iconification.
 */
RECT
free_icon_pos(enum locks lock)
{
	RECT ic;
	int i = 0;

	for (;;)
	{
		struct xa_window *w = window_list;
		ic = iconify_grid(i++);

		/* find first unused position. */
		while (w)
		{
			if (w->window_status == XAWS_ICONIFIED)
			{
				
				if (w->r.x == ic.x && w->r.y == ic.y)
					/* position occupied; advance with next position in grid. */
					break;
			}
			w = w->next;
		}

		if (!w)
			/* position not in list of iconified windows, so it is free. */
			break;
	}

	return ic;
}

/*
 * Check if a new menu and/or desktop is needed
 */
static void
check_menu_desktop(enum locks lock, struct xa_window *old_top, struct xa_window *new_top)
{
	/* If we're getting a new top window, we may need to swap menu bars... */
	if (old_top && old_top->owner != new_top->owner)
	{
		Sema_Up(desk);

		DIAG((D_appl, NULL, "check_menu_desktop from %s to %s",
			old_top->owner->name, new_top->owner->name));

		swap_menu(lock|desk, new_top->owner, true, 2);

		Sema_Dn(desk);
	}
}

#if 0
/* not used??? */

static RECT
wa_to_curr(struct xa_window *wi, int d, RECT r)
{
	r.x += wi->bd.x - d;
	r.y += wi->bd.y - d;
	r.w += wi->bd.w + d+d;
	r.h += wi->bd.h + d+d;
	return r;
}

static RECT
bound_inside(RECT r, RECT o)
{
	if (r.x       < o.x      ) r.x  = o.x;
	if (r.y       < o.y      ) r.y  = o.y;
	if (r.x + r.w > o.x + o.w) r.w -= (r.x + r.w) - (o.x + o.w);
	if (r.y + r.h > o.y + o.h) r.h -= (r.y + r.h) - (o.y + o.h);
	return r;
}
#endif

XA_WIND_ATTR
hide_move(struct options *o)
{
	XA_WIND_ATTR kind = 0;

	if (!o->xa_nohide)
		kind |= HIDE;
	if (!o->xa_nomove)
		kind |= MOVER;

	return kind;
}

void
inside_root(RECT *r, struct options *o)
{
	if (r->y < root_window->wa.y)
		r->y = root_window->wa.y;

	if (o->noleft)
		if (r->x < root_window->wa.x)
			r->x = root_window->wa.x;
}

static void
wi_put_first(struct win_base *b, struct xa_window *w)
{
	if (b->first)
	{
		w->next = b->first;
		w->prev = NULL;
		b->first->prev = w;
	}
	else
	{
		b->last = w;
		w->next = w->prev = NULL;
	}
	b->first = w;
#if 0
	w = b->first;
	while (w)
	{
		DIAGS(("pf: %d pr:%d", w->handle, w->prev ? w->prev->handle : -1));
		w = w->next;
	}
#endif
}

#if 0
/* not used??? */

static void
wi_put_last(WIN_BASE *b, struct xa_window *w)
{
	if (b->last)
	{
		w->prev = b->last;
		b->last->next = w;
		b->last = w;
		w->next = NULL;
	}
	else
	{
		b->first = b->last = w;
		w->next = w->prev = NULL;
	}
#if 0
	w = b->first;
	while (w)
	{
		DIAGS(("pl: %d pr:%d", w->handle, w->prev ? w->prev->handle : -1));
		w = w->next;
	}
#endif
}
#endif

/* This a special version of put last; it puts w just befor last. */
static void
wi_put_blast(struct win_base *b, struct xa_window *w)
{
	if (b->last)
	{
		struct xa_window *l = b->last;
		w->next = l;
		w->prev = l->prev;
		if (l->prev)
			l->prev->next = w;
		l->prev = w;
	}
	else
	{
		b->first = b->last = w;
		w->next = w->prev = NULL;
	}
#if 0
	w = b->first;
	while (w)
	{
		DIAGS(("pl: %d pr:%d", w->handle, w->prev ? w->prev->handle : -1));
		w = w->next;
	}
#endif
}

static void
wi_remove(struct win_base *b, struct xa_window *w)
{
	if (w->prev)
		w->prev->next = w->next;
	if (w->next)
		w->next->prev = w->prev;
	if (w == b->first)
		b->first = w->next;
	if (w == b->last)
		b->last = w->prev;
	w->next = NULL;
	w->prev = NULL;
#if 0
	w = b->first;
	while (w)
	{
		DIAGS(("rem: %d pr:%d", w->handle, w->prev ? w->prev->handle : -1));
		w = w->next;
	}
#endif
}

#if 0
/* not used??? */

static void
wi_insert(WIN_BASE *b, struct xa_window *w, struct xa_window *after)
{
	if (after)
	{
		if (after != b->last)
		{
			w->next = after->next;
			w->prev = after;
			if (after->next)
			{
				if (after->next == b->last)
					b->last = w;
				after->next->prev = w;
			}
			after->next = w;
		}
	}
	else
		wi_put_first(b, w);
#if 0
	w = b->first;
	while (w)
	{
		DIAGS(("rem: %d pr:%d", w->handle, w->prev ? w->prev->handle : -1)i);
		w = w->next;
	}
#endif
}
#endif

struct xa_window *
get_top(void)
{
	return S.open_windows.first;
}

bool
is_hidden(struct xa_window *wind)
{
	RECT d = root_window->r;
	return !xa_rc_intersect(wind->r, &d);
}

bool
unhide(struct xa_window *wind, short *x, short *y)
{
	RECT r = wind->r;
	bool done = false;

	if (is_hidden(wind))
	{
		RECT d = root_window->r;

		while (r.x + r.w < d.x)
			r.x += d.w;
		while (r.y + r.h < d.y)
			r.y += d.h;
		if (r.y < root_window->wa.y)
			r.y = root_window->wa.y;	/* make shure the mover becomes visible. */
		while (r.x > d.x + d.w)
			r.x -= d.w;
		while (r.y > d.y + d.h)
			r.y -= d.h;
		done = true;
	}
	*x = r.x;
	*y = r.y;
	return done;
}

void
unhide_window(enum locks lock, struct xa_window *wind)
{
	RECT r = wind->r;
	if (unhide(wind, &r.x, &r.y))
	{
		//if (wind->window_status == XAWS_ICONIFIED)
		//	r = free_icon_pos(lock);

		if (wind->send_message)
			wind->send_message(lock, wind, NULL,
					   WM_MOVED, 0, 0, wind->handle,
					   r.x, r.y, r.w, r.h);
		else
			move_window(lock, wind, -1, r.x, r.y, r.w, r.h);
	}
}

/* SendMessage */
void
send_untop(enum locks lock, struct xa_window *wind)
{
	struct xa_client *client = wind->owner;

	if (wind->send_message && !client->fmd.wind)
		wind->send_message(lock, wind, NULL,
				   WM_UNTOPPED, 0, 0, wind->handle,
				   0, 0, 0, 0);
}

void
send_ontop(enum locks lock)
{
	struct xa_window *top = window_list;
	struct xa_client *client = top->owner;

	if (top->send_message && !client->fmd.wind)
		top->send_message(lock, top, NULL,
				  WM_ONTOP, 0, 0, top->handle,
				  0, 0, 0, 0);
}

/*
 * Create a window
 *
 * introduced pid -1 for temporary windows created basicly only to be able to do
 * calc_work_area and center.
 *
 * needed a dynamiccally sized frame
 */
struct xa_window *
create_window(
	enum locks lock,
	SendMessage *message_handler,
	struct xa_client *client,
	bool nolist,
	XA_WIND_ATTR tp,
	WINDOW_TYPE dial,
	int frame,
	int thinframe,
	bool thinwork,
	const RECT r,
	const RECT *max,
	RECT *remember)
{
	struct xa_window *w;

#if GENERATE_DIAGS
	if (max)
	{
		DIAG((D_wind, client, "create_window for %s: r:%d,%d/%d,%d  max:%d,%d/%d,%d",
			c_owner(client), r.x,r.y,r.w,r.h,max->x,max->y,max->w,max->h));
	}
	else
	{
		DIAG((D_wind, client, "create_window for %s: r:%d,%d/%d,%d  no max",
			c_owner(client), r.x,r.y,r.w,r.h));
	}
#endif

	w = kmalloc(sizeof(*w));
	if (!w)
		/* Unable to allocate memory for window? */
		return NULL;

	/* avoid confusion: if only 1 specified, give both (fail safe!) */
	if ((tp & UPARROW) || (tp & DNARROW))
		tp |= UPARROW|DNARROW;
	if ((tp & LFARROW) || (tp & RTARROW))
		tp |= LFARROW|RTARROW;
	/* cant hide a window that cannot be moved. */
	if ((tp & MOVER) == 0)
		tp &= ~HIDE;
	/* temporary until solved. */
	if (tp & MENUBAR)
		tp |= XaMENU;

	/* implement maximum rectangle (needed for at least TosWin2) */
	w->max = max ? *max : root_window->wa;
		
	w->r  = r;
	w->pr = w->r;

	if (!MONO && frame > 0)
	{
		if (thinframe < 0)
			frame = 1;
	}

# if 0
	/* implement border sizing. */
	if ((tp & (SIZE|MOVE)) == (SIZE|MOVE))
#endif
		if (frame > 0 && thinframe > 0)
			/* see how well it performs. */
			frame += thinframe;

	w->frame = frame;
	w->thinwork = thinwork;
	w->owner = client;
	w->rect_user = w->rect_list = w->rect_start = NULL;
	w->handle = -1;
	w->window_status = XAWS_CLOSED;
	w->remember = remember;
	w->nolist = nolist;
	w->dial = dial;
	w->send_message = message_handler;
	get_widget(w, XAW_TITLE)->stuff = client->name;

	if (nolist)
	{
		/* Dont put in the windowlist */

		/* Attach the appropriate widgets to the window */
		standard_widgets(w, tp, false);
	}
	else
	{
		w->handle = new_wind_handle();
		DIAG((D_wind, client, " allocated handle = %d", w->handle));

		wi_put_first(&S.closed_windows, w);
		DIAG((D_wind,client," inserted in closed_windows list"));

		/* Attach the appropriate widgets to the window */
		standard_widgets(w, tp, false);

		/* If STORE_BACK extended attribute is used, window preserves its own background */
		if (tp & STORE_BACK)
		{
			DIAG((D_wind,client," allocating background storage buffer"));
			w->background = kmalloc(calc_back(&r, screen.planes));
		}
		else
			w->background = NULL;

	}

	calc_work_area(w);

	if (remember)
		*remember = w->r;

	return w;
}

int
open_window(enum locks lock, struct xa_window *wind, RECT r)
{
	struct xa_window *wl = window_list;
	RECT clip, our_win;

	DIAG((D_wind, wind->owner, "open_window %d for %s to %d/%d,%d/%d status %x",
		wind->handle, c_owner(wind->owner), r.x,r.y,r.w,r.h, wind->window_status));

#if 0
	if (r.w <= 0) r.w = 6*cfg.widg_w;
	if (r.h <= 0) r.h = 6*cfg.widg_h;
#endif

	if (wind->nolist)
		/* dont open unlisted windows */
		return 0;

	if (wind->is_open == true)
	{
		/* The window is already open, no need to do anything */

		DIAGS(("WARNING: Attempt to open window when it was already open"));
		return 0;
	}

	wi_remove(&S.closed_windows, wind);
	wi_put_first(&S.open_windows, wind);

	C.focus = wind;

	/* New top window - change the cursor to this client's choice */
	graf_mouse(wind->owner->mouse, wind->owner->mouse_form);

	if (wind->window_status == XAWS_ICONIFIED)
	{
		if (r.w != -1 && r.h != -1)
			wind->r = r;

		if (wind != root_window)
			inside_root(&wind->r, &wind->owner->options);
	}
	else
	{
		/* Change the window coords */
		wind->r = r;
		if (wind != root_window)
			inside_root(&wind->r, &wind->owner->options);

		wind->window_status = XAWS_OPEN;
	}

	/* Flag window as open */
	wind->is_open = true;

	calc_work_area(wind);

	check_menu_desktop(lock|winlist, wl, wind);

	/* Is this a 'preserve own background' window? */
	if (wind->active_widgets & STORE_BACK)
	{
		form_save(0, wind->r, &(wind->background));
		/* This is enough, it is only for TOOLBAR windows. */
		draw_window(lock|winlist, wind);
		redraw_menu(lock);
	}
	else
	{
		our_win = wind->r;
	
		wl = wind;
		while (wl)
		{
			clip = wl->r;

			if (xa_rc_intersect(our_win, &clip))
				generate_rect_list(lock|winlist, wl, 1);

			wl = wl->next;
		}

		/* streamlining the topping */
		after_top(lock|winlist, true);

		/* Display the window using clipping rectangles from the rectangle list */
		display_window(lock|winlist, 10, wind, NULL);
		redraw_menu(lock);

		if (wind->send_message)
			wind->send_message(lock|winlist, wind, NULL,
					   WM_REDRAW, 0, 0, wind->handle,
					   wind->r.x, wind->r.y, wind->r.w, wind->r.h);
	}

	DIAG((D_wind, wind->owner, "open_window %d for %s exit with 1",
		wind->handle, c_owner(wind->owner)));
	return 1;
}

static void
if_bar(short pnt[4])
{
	if ((pnt[2] - pnt[0]) !=0 && (pnt[3] - pnt[1]) != 0)
		v_bar(C.vh, pnt);
}


static void
//draw_window(enum locks lock, struct xa_window *wind)
Ddraw_window(enum locks lock, struct xa_window *wind)
{
	//struct xa_window *wind = (struct xa_window *)ce->ptr1;

	DIAG((D_wind, wind->owner, "draw_window %d for %s to %d/%d,%d/%d",
		wind->handle, w_owner(wind),
		wind->r.x, wind->r.y, wind->r.w, wind->r.h));

	if (!wind->is_open)
	{
		DIAG((D_wind, wind->owner, "window %d for %s not open",
			wind->handle, w_owner(wind)));
		return;
	}

	l_color(G_BLACK);
	hidem();

	/* Dont waste precious CRT glass */
	if (wind != root_window)
	{
		RECT cl = wind->r;
		RECT wa = wind->wa;

		/* Display the window backdrop (borders only, GEM style) */

		cl.w-=SHADOW_OFFSET;
		cl.h-=SHADOW_OFFSET;
#if 0
		if (wind->active_widgets & TOOLBAR)
		{
			/* whole plane */

			if (wind->frame > 0)
				if (MONO)
					p_gbar(0, &cl);
				else
					gbar  (0, &cl);		
		}
		else
#endif
		if ((wind->active_widgets & V_WIDG) == 0)
		{
			if (wind->frame > 0)
				gbox(0, &cl);
		}
		else
		{
			short pnt[8];

			f_color(screen.dial_colours.bg_col);
			f_interior(FIS_SOLID);

			pnt[0] = cl.x;
			pnt[1] = cl.y;
			pnt[2] = cl.x + cl.w - 1;
			pnt[3] = wa.y - 1;
			if_bar(pnt); 			/* top */
			pnt[1] = wa.y + wa.h;
			pnt[3] = cl.y + cl.h - 1;
			if_bar(pnt);			/* bottom */
			pnt[0] = cl.x;
			pnt[1] = cl.y;
			pnt[2] = wa.x - 1;
			if_bar(pnt);			/* left */
			pnt[0] = wa.x + wa.w;
			pnt[2] = cl.x + cl.w - 1;
			if_bar(pnt);			/* right */

			/* Display the work area */

			if (MONO)
			{
				if (wind->frame > 0)
					gbox(0, &cl);
				gbox(1, &wa);
			}
			else
			{
				if (wind->thinwork)
				{
					gbox(1, &wa);
				}
				else
				{
					RECT nw = wa;
					nw.w++;
					nw.h++;
					br_hook(2, &nw, screen.dial_colours.shadow_col);
					tl_hook(2, &nw, screen.dial_colours.lit_col);
					br_hook(1, &nw, screen.dial_colours.lit_col);
					tl_hook(1, &nw, screen.dial_colours.shadow_col);
				}
			}
		}

#if 0
		/* for outlined windowed objects */
		if (wind->outline_adjust)
		{
			l_color(screen.dial_colours.bg_col);
			gbox( 0, &wa);
			gbox(-1, &wa);
			gbox(-2, &wa);
		}
#endif
		if (wind->frame > 0)
		{
			shadow_object(0, OS_SHADOWED, &cl, G_BLACK, SHADOW_OFFSET/2);

#if 0
			/* only usefull if we can distinguish between
			 * top left margin and bottom right margin, which currently is
			 * not the case
			 */
			if (!MONO)
				tl_hook(0, &cl, screen.dial_colours.lit_col);
#endif
		}
	}

	/* If the window has an auto-redraw function, call it
	 * do this before the widgets. (some programs supply x=0, y=0)
	 */
	if (wind->redraw) /* && !wind->owner->killed */
		wind->redraw(lock, wind);

	/* Go through and display the window widgets using their display behaviour */
	if (wind->window_status == XAWS_ICONIFIED)
	{
		XA_WIDGET *widg;

		widg = get_widget(wind, XAW_TITLE);
		widg->display(lock, wind, widg);

		widg = get_widget(wind, XAW_ICONIFY);
		widg->display(lock, wind, widg);
	}
	else
	{
		int f;
		XA_WIDGET *mwidg = get_widget(root_window, XAW_MENU);

		/*
		 * Ozk: Check for the root-window menuline widget (applications menuline)
		 * and skip redrawing it here. Call redraw_menu() where appropriate instead. 
		*/
		for (f = 0; f < XA_MAX_WIDGETS; f++)
		{
			XA_WIDGET *widg;

			widg = get_widget(wind, f);
			if (widg != mwidg)
			{
				if (widg->display)
				{
					DIAG((D_wind, wind->owner, "draw_window %d: display widget %d (func: %lx)",
						wind->handle, f, widg->display));
					widg->display(lock, wind, widg);
				}
			}
			else
			{
				DIAG((D_wind, wind->owner, "draw_window %d: (%d) draw menu", wind->handle, f));
				//redraw_menu(lock);
			}
		}
	}

	showm();

	DIAG((D_wind, wind->owner, "draw_window %d for %s exit ok",
		wind->handle, w_owner(wind)));
}

static void
Pdraw_window(void *_parm)
{
	long *parm = _parm;
//	struct xa_client *c = (struct xa_client *)parm[1];

	Ddraw_window(0, (struct xa_window *)parm[0]);
	wake(IO_Q, (long)parm);
	kfree(parm);
	kthread_exit(0);
}

void
draw_window(enum locks lock, struct xa_window *wind)
{
	struct xa_client *rc = lookup_extension(NULL, XAAES_MAGIC);
	struct xa_client *wo;

	wo = wind == root_window ? get_desktop()->owner : wind->owner;
		
	if (rc == wo || wo == C.Aes)
	{
		DIAG((D_wind, rc, "draw_window %d, for %s", wind->handle, rc->name));
		Ddraw_window(lock, wind);
	}
	else
	{
		struct proc *np;
		long *p;

		p = kmalloc(16);
		assert(p);

		p[0] = (long)wind;
		p[1] = (long)wo;

		DIAG((D_wind, rc, "kthreaded draw_window %d for %s by %s", wind->handle, wo->name, rc->name));

		kthread_create(wo->p, Pdraw_window, p, &np, "k%s", wo->name);
		sleep(IO_Q, (long)p);
		if (wo->usr_evnt && wo->sleeplock)
			Unblock(wo, 1, 9000);
	}
	DIAGS(("DrawWind - exit OK"));
}

struct xa_window *
find_window(enum locks lock, int x, int y)
{
	struct xa_window *w;

	w = window_list;
	while(w)
	{
		if (m_inside(x, y, &w->r))
			break;

		w = w->next;
	}

	return w;
}

struct xa_window *
get_wind_by_handle(enum locks lock, int h)
{
	struct xa_window *w;

	w = window_list;
	while (w)
	{
		if (w->handle == h)
			break;

		w = w->next;
	}

	if (!w)
	{
		w = S.closed_windows.first;
		while (w)
		{
			if (w->handle == h)
				break;

			w = w->next;
		}
	}

	return w;
}

/*
 * Handle windows after topping
 */
void
after_top(enum locks lock, bool untop)
{
	struct xa_window *below;

	below = window_list->next;

	/* Refresh the previous top window as being 'non-topped' */
	if (below && below != root_window)
	{
		display_window(lock, 11, below, NULL);

		if (untop)
			send_untop(lock, below);
	}
}

/*
 * Pull this window to the head of the window list
 */
struct xa_window *
pull_wind_to_top(enum locks lock, struct xa_window *w)
{
	enum locks wlock = lock|winlist;
	struct xa_window *wl;
	RECT clip, r;

	DIAG((D_wind, w->owner, "pull_wind_to_top %d for %s", w->handle, w_owner(w)));

	check_menu_desktop(wlock, window_list, w);

	if (w == root_window) /* just a safeguard */
	{
		generate_rect_list(wlock, w, 3);
	}
	else
	{
		wl = w->prev;
		r = w->r;

		/* Very small change in logic for was_visible() optimization.
	         * It eliminates a spurious generate_rect_list() call, which
	         * spoiled the setting of rect_prev. */

		if (w != window_list)	
		{
			wi_remove   (&S.open_windows, w);
			wi_put_first(&S.open_windows, w);

			while(wl)
			{
				clip = wl->r;
				DIAG((D_r, wl->owner, "wllist %d", wl->handle));
				if (xa_rc_intersect(r, &clip))
					generate_rect_list(wlock, wl, 2);
				wl = wl->prev;
			}
		}
		else			/* already on top */
			generate_rect_list(wlock, w, 3);
	}

	return w;
}

void
send_wind_to_bottom(enum locks lock, struct xa_window *w)
{
	struct xa_window *wl;
	struct xa_window *old_top = window_list;

	RECT r, clip;

	if (   w->next == root_window	/* Can't send to the bottom a window that's already there */
	    || w       == root_window	/* just a safeguard */
	    || w->is_open == false)
		return;

	wl = w->next;
	r = w->r;

	wi_remove   (&S.open_windows, w);
	wi_put_blast(&S.open_windows, w);	/* put before last; last = root_window. */

	while (wl)
	{
		clip = wl->r;
		if (xa_rc_intersect(r, &clip))
			generate_rect_list(lock|winlist, wl, 4);
		wl = wl->next;
	}

	generate_rect_list(lock|winlist, w, 5);

	check_menu_desktop(lock|winlist, old_top, window_list);
}

/*
 * Change an open window's coordinates, updating rectangle lists as appropriate
 */
void
move_window(enum locks lock, struct xa_window *wind, int newstate, int x, int y, int w, int h)
{
	enum locks wlock = lock|winlist;
	IFDIAG(struct xa_client *client = wind->owner;)
	struct xa_window *wl;
	RECT old, new, clip, oldw, pr;
	bool blit_mode;

	DIAG((D_wind,client,"move_window(%s) %d for %s from %d/%d,%d/%d to %d/%d,%d,%d",
	      wind->is_open ? "open" : "closed",
	      wind->handle, c_owner(client), wind->r.x,wind->r.y,wind->r.w,wind->r.h, x,y,w,h));

#if 0
	temporary commented out, because I dont know exactly the consequences.
	/* avoid spurious moves or sizings */
	if (x != wind->r.x || y != wind->r.y || w != wind->r.w || h != wind->r.h)
#endif
	{
		pr = wind->r;

		if (newstate >= 0)
		{
			wind->window_status = newstate;
			if (newstate == XAWS_ICONIFIED)
				wind->ro = wind->r;
		}
		else
			/* Save windows previous coords */
			wind->pr = wind->r;

		old = wind->r;
		oldw = wind->wa;

		/* Change the window coords */
		wind->r.x = x;
		wind->r.y = y;
		wind->r.w = w;
		wind->r.h = h;

		inside_root(&wind->r, &wind->owner->options);

		new = wind->r;

		blit_mode = (    wind == window_list
			     && (wind->active_widgets & TOOLBAR) == 0	/* temporary slist workarea hack */
			     && (pr.x != new.x || pr.y != new.y)
			     && pr.w == new.w
			     && pr.h == new.h
			     && pr.x >= 0
			     && pr.y >= 0
			     && pr.x + pr.w < screen.r.w
			     && pr.y + pr.h < screen.r.h
			     && new.x + new.w < screen.r.w
			     && new.y + new.h < screen.r.h);

		/* Recalculate the work area (as well as moving,
		 * it might have changed size).
		 */
		calc_work_area(wind);
	
		/* Is it allowed to move a closed window? */
		if (wind->is_open)
		{
			XA_WIDGET *widg = get_widget(wind, XAW_TOOLBAR);
			XA_TREE *wt = widg->stuff;
			
			/* Temporary hack 070702 */
			if (wt && wt->tree)
			{
				wt->tree->ob_x = wind->wa.x;
				wt->tree->ob_y = wind->wa.y;
			}

			/* Update the window's rectangle list, it will be out of date now */
			generate_rect_list(wlock, wind, 6);

			/* If window is being blit mode transferred, do the blit instead of redrawing */
			if (blit_mode)
			{
				DIAG((D_wind, client, "blit"));
				form_copy(&pr, &new);
			}
			else
			{
				display_window(wlock, 12, wind, NULL);
				/* Does this window's application want messages? If so send it a redraw */
				if (wind->send_message)
				{
					/* moved? */
					if (new.x != pr.x || new.y != pr.y)
					{
						DIAG((D_wind, client, "moved"));
						wind->send_message(wlock, wind, NULL,
								   WM_REDRAW, 0, 0, wind->handle,
								   wind->wa.x, wind->wa.y, wind->wa.w, wind->wa.h);
					}
					else	/* sized */
					{
						DIAG((D_wind, client, "sized"));
						if (wind->wa.w > oldw.w)
						{
							DIAG((D_wind,client, "wider"));
							wind->send_message(wlock, wind, NULL,
									   WM_REDRAW, 0, 0, wind->handle,
									   wind->wa.x + oldw.w, wind->wa.y,
									   wind->wa.w - oldw.w, MIN(oldw.h, wind->wa.h));
						}
						if (wind->wa.h > oldw.h)
						{
							DIAG((D_wind, client, "higher"));
							wind->send_message(wlock, wind, NULL,
									   WM_REDRAW, 0, 0, wind->handle,
									   wind->wa.x, wind->wa.y + oldw.h,
									   wind->wa.w, wind->wa.h - oldw.h);
						}
					}
				}
			}
		
			wl = wind->next;

			/* For some reason the open window had got behind
			 * root!!! This was caused by the problems with the
			 * SIGCHILD stuff (the root window was pulled to top!)
			 * I've put a safeguard there, so that all the below
			 * kind of loops can be trusted.
			 */
			while (wl)
			{
				DIAG((D_wind, wl->owner, "[1]redisplay %d", wl->handle));

				clip = wl->r;

				/* Check for newly exposed windows */
				if (xa_rc_intersect(old, &clip))
				{			
					generate_rect_list(wlock, wl, 7);
					display_window(wlock, 13, wl, &clip);
					if (wl->send_message)
						wl->send_message(wlock, wl, NULL,
							WM_REDRAW, 0, 0, wl->handle,
							clip.x, clip.y, clip.w, clip.h);

				}
				else
				{
					clip = wl->r;
					/* Check for newly covered windows */
					if (xa_rc_intersect(new, &clip))
					{
						/* We don't need to send a redraw to
						 * these windows, we just have to update
						 * their rect lists */
						generate_rect_list(wlock, wl, 8);
					}
				}

				wl = wl->next;
			}

			/* removed rootwindow stuff, that perfectly worked within the loop. */
		}
	}

	if (wind->remember)
		*wind->remember = wind->r;
}

/*
 * Close an open window and re-display any windows underneath it.
 * Also places window in the closed_windows list but does NOT delete it
 * - the window will still exist after this call.
 */
bool
close_window(enum locks lock, struct xa_window *wind)
{
	struct xa_window *wl;
	struct xa_client *client = wind->owner;
	RECT r, clip;
	bool is_top;

	if (wind == NULL)
	{
		DIAGS(("close_window: null window pointer"));
		/* Invalid window handle, return error */
		return false;
	}

	DIAG((D_wind, wind->owner, "close_window: %d (%s)",
		wind->handle, wind->is_open ? "open" : "closed"));

	if (wind->is_open == false || wind->nolist)
		return false;

	is_top = (wind == window_list);

	wi_remove(&S.open_windows, wind);
	wi_put_first(&S.closed_windows, wind);

	if (C.focus == wind)
	{
		C.focus = window_list;
		if (!C.focus)
			DIAGS(("no focus owner, no windows open???"));
	}

	r = wind->r;

	if (wind->rect_start)
		kfree(wind->rect_start);

	wind->rect_user = wind->rect_list = wind->rect_start = NULL;

	/* Tag window as closed */
	wind->is_open = false;
	wind->window_status = XAWS_CLOSED;

	wl = window_list;

	if (is_top)
	{
		struct xa_window *w, *napp = NULL;

		if (wind->active_widgets & STORE_BACK)
		{
			form_restore(0, wind->r, &(wind->background));
			return true;
		}

 		w = window_list;

		/* v_hide/show_c now correctly done in draw_window()  */
		/* if you want point_to_type, handle that in do_keyboard */

		/* First: find a window of the owner */
		while (w)
		{
			if (w != root_window)
			{ 
				if (w->owner == client) /* gotcha */
				{
					top_window(lock|winlist, w, client);
					/* send WM_ONTOP to just topped window. */
					send_ontop(lock);
					wl = w->next;
					break;
				}
				else if (!napp)
					/* remember the first window of another app. */
					napp = w;
			}

			w = w->next;

			if (w == NULL && napp)
			{
				/* Second: If none: top any other open window  */
				/* this is the one I was really missing */
				top_window(lock|winlist, napp, menu_owner());
				/* send WM_ONTOP to just topped window. */
				send_ontop(lock);
				wl = window_list->next;
				break;
			}
		}
	}

	/* Redisplay any windows below the one we just have closed
	 * or just have topped
	 */
	while (wl)
	{
		clip = wl->r;
		DIAG((D_wind, client, "[2]redisplay %d", wl->handle));
		if (xa_rc_intersect(r, &clip))
		{
			DIAG((D_wind, client, "   --   clip %d/%d,%d/%d", clip));
			/* If a new focus was pulled up, some of these are not needed */
			generate_rect_list(lock|winlist, wl, 9);
			display_window(lock|winlist, 14, wl, &clip);
			if (wl->send_message)
				wl->send_message(lock|winlist, wl, NULL,
					WM_REDRAW, 0, 0, wl->handle,
					clip.x, clip.y, clip.w, clip.h);
		}
		wl = wl->next;
	}

	if (window_list)
	{
		if (   window_list->owner != client
		    && client->std_menu.tree == NULL)
		{
			/* get the menu bar right (only if the pid has no menu bar
			 * and no more open windows for pid.
			 */
			DIAG((D_menu, NULL, "close_window: swap_menu to %s", w_owner(window_list)));
			swap_menu(lock|winlist, window_list->owner, true, 3);
		}
	}

	return true;
}

static void
free_widg(struct xa_window *wind, int n)
{
	XA_WIDGET *widg;

	widg = get_widget(wind, n);
	if (widg->stuff)
	{
		kfree(widg->stuff);
		widg->stuff = NULL;
	}
}

static void
free_standard_widgets(struct xa_window *wind)
{
	free_widg(wind, XAW_HSLIDE);
	free_widg(wind, XAW_VSLIDE);
}

static void
delete_window1(enum locks lock, struct xa_window *wind)
{
	if (!wind->nolist)
	{
		DIAG((D_wind, wind->owner, "delete_window1 %d for %s: open? %d",
			wind->handle, w_owner(wind), wind->is_open));

		assert(!wind->is_open);

		free_standard_widgets(wind);

		/* Call the window destructor if any */
		if (wind->destructor)
			wind->destructor(lock|winlist, wind);

		free_wind_handle(wind->handle);

		if (wind->background)
			kfree(wind->background);

		if (wind->rect_start)
			kfree(wind->rect_start);
	}
	else
	{
		free_standard_widgets(wind);

		if (wind->destructor)
			wind->destructor(lock, wind);
	}

	kfree(wind);
}

void
delete_window(enum locks lock, struct xa_window *wind)
{
	if (!wind->nolist)
	{
		/* We must be sure it is in the correct list. */
		if (wind->is_open)
		{
			DIAG((D_wind, wind->owner, "delete_window %d: not closed", wind->handle));
			/* open window, return error */
			return;
		}

		wi_remove(&S.closed_windows, wind);
	}

	delete_window1(lock, wind);
}

void
delayed_delete_window(enum locks lock, struct xa_window *wind)
{
	if (!wind->nolist)
	{
		DIAG((D_wind, wind->owner, "delayed_delete_window %d for %s: open? %d",
			wind->handle, w_owner(wind), wind->is_open));

		/* We must be sure it is in the correct list. */
		if (wind->is_open)
		{
			DIAG((D_wind, wind->owner, "delayed_delete_window %d: not closed", wind->handle));
			/* open window, return error */
			return;
		}

		wi_remove(&S.closed_windows, wind);
	}

	wi_put_first(&S.deleted_windows, wind);
}

void
do_delayed_delete_window(enum locks lock)
{
	struct xa_window *wind = S.deleted_windows.first;

	DIAGS(("do_delayed_delete_window"));

	while (wind)
	{
		/* remove from list */
		wi_remove(&S.deleted_windows, wind);

		/* final delete */
		delete_window1(lock, wind);

		/* anything left? */
		wind = S.deleted_windows.first;
	}
}

/*
 * Display a window that isn't on top, respecting clipping
 * - Pass clip == NULL to redraw whole window, otherwise clip is a pointer to a GRECT that
 * defines the clip rectangle.
 */
void
display_window(enum locks lock, int which, struct xa_window *wind, RECT *clip)
{
	if (wind->is_open)
	{
		DIAG((D_wind, wind->owner, "[%d]display_window%s %d for %s to %d/%d,%d/%d",
			which, wind->nolist ? "(no_list)" : "", wind->handle, w_owner(wind),
			wind->r.x, wind->r.y, wind->r.w, wind->r.h));

		if (wind->nolist)
		{
			draw_window(lock, wind);
		}
		else
		{
			struct xa_rect_list *rl;

			rl = rect_get_system_first(wind);
			while (rl)
			{			
				if (clip)
				{
					RECT target = rl->r;				

					if (xa_rc_intersect(*clip, &target))
					{
						set_clip(&target);
						DIAG((D_rect, wind->owner, "clip: %d/%d,%d/%d",
							target.x,target.y,target.w,target.h));

						draw_window(lock, wind);
					}
				}
				else
				{
					set_clip(&rl->r);
					draw_window(lock, wind);
				}

				rl = rect_get_system_next(wind);
			}

			clear_clip();
		}

		DIAG((D_wind, wind->owner, "display_window %d for %s exit ok",
			wind->handle, w_owner(wind)));
	}
}

/*
 * Display windows below a given rectangle, starting with window w.
 * HR: at the time only called for form_dial(FMD_FINISH)
 */
void
display_windows_below(enum locks lock, const RECT *r, struct xa_window *wl)
{
	RECT win_r;

	while (wl)
	{
		struct xa_rect_list *rl = rect_get_system_first(wl);
		while (rl)
		{
			win_r = rl->r;		
			if (xa_rc_intersect(*r, &win_r))
			{
				set_clip(&win_r);
				draw_window(lock, wl);		/* Display the window */
				if (wl->send_message)
					wl->send_message(lock, wl, NULL,
						WM_REDRAW, 0, 0, wl->handle,
						win_r.x, win_r.y, win_r.w, win_r.h);
			}
			rl = rect_get_system_next(wl);
		}
		wl = wl->next;
	}
	clear_clip();
}

/*
 * Calculate window sizes
 *
 *   HR: first intruduction of the use of a fake window (pid=-1) for calculations.
 *         (heavily used by multistrip, as an example.)
 */

static void
Xpolate(RECT *r, RECT *o, RECT *i)
{
	/* If you want to prove the maths here, draw two boxes one inside
	 * the other, then sit and think about it for a while...
	 * HR: very clever :-)
	 */
	r->x = 2 * o->x - i->x;
	r->y = 2 * o->y - i->y;
	r->w = 2 * o->w - i->w;
	r->h = 2 * o->h - i->h;	
}

RECT
calc_window(enum locks lock, struct xa_client *client, int request, ulong tp, int mg, int thinframe, bool thinwork, RECT r)
{
	struct xa_window *w_temp;
	RECT o;

	DIAG((D_wind,client,"calc %s from %d/%d,%d/%d", request ? "work" : "border", r));

	/* Create a temporary window with the required widgets */
	w_temp = create_window(lock, NULL, client, true, tp, 0, mg, thinframe, thinwork, r, 0, 0);

	switch(request)
	{
		case WC_BORDER:	
			/* We have to work out the border size ourselves here */
			Xpolate(&o, &w_temp->r, &w_temp->wa);
			break;
		case WC_WORK:
			/* Work area was calculated when the window was created */
			o = w_temp->wa;
			break;
		default:
			DIAG((D_wind, client, "wind_calc request %d", request));
			o = w_temp->wa;	/* HR: return something usefull*/
	}
	DIAG((D_wind,client,"calc returned: %d/%d,%d/%d", o));

	/* Dispose of the temporary window we created */
	delete_window(lock, w_temp);

	return o;
}
