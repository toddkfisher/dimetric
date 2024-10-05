#include <X11/X.h>
#include <X11/extensions/XKB.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#define XK_LATIN1
#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

const unsigned int KBD_TIMEOUT_MS = 1;  // Initial repeat delay (in game).
const unsigned int KBD_INTERVAL_MS = 1; // Delay between repeats (in game).

cairo_surface_t *g_image;
cairo_surface_t *g_background_image;
uint32_t g_x_pos = 100;
uint32_t g_y_pos = 100;
const int32_t DX = 75;
const int32_t DY = 75;

typedef char ASCII;

typedef enum
{
  EV_NONE, // Must be at top of list.
  EV_PAINT,
  EV_ENTER,
  EV_LEAVE,
  EV_BUTTON_PRESS,
  EV_BUTTON_RELEASE,
  EV_CLOSE,
  // NOTE: each keycode is a separate event type.
  EV_KEY_UP,
  EV_KEY_DOWN,
  EV_KEY_LEFT,
  EV_KEY_RIGHT,
  EV_KEY_END,
  EV_KEY_PAUSE
} event_type_t;

ASCII *g_event_names[] =
{
  "EV_NONE", 
  "EV_PAINT",
  "EV_ENTER",
  "EV_LEAVE",
  "EV_BUTTON_PRESS",
  "EV_BUTTON_RELEASE",
  "EV_CLOSE",
  "EV_KEY_UP",
  "EV_KEY_DOWN",
  "EV_KEY_LEFT",
  "EV_KEY_RIGHT",
  "EV_KEY_END",
  "EV_KEY_PAUSE"
};

typedef struct ui_state_t
{
  float u_line_width;
  float u_line_red;
  float u_line_green;
  float u_line_blue;
  float u_line_alpha;
  float u_fill_red;
  float u_fill_green;
  float u_fill_blue;
  float u_fill_alpha;
  float u_background_fill_red;
  float u_background_fill_green;
  float u_background_fill_blue;
  cairo_t *u_cr;
  cairo_surface_t *u_surface;
  int u_window_width;
  int u_window_height;
  Display *u_display;
  Drawable u_window;
  int u_screen;
  unsigned int u_kbd_timeout_default_ms;  // Initial repeat delay (default X value).
  unsigned int u_kbd_interval_default_ms; // Delay between repeats (default X value).
  XEvent u_event;
  event_type_t u_event_type;
  Time u_last_pause_key_time_millisec;
} ui_state_t;

// Keep cairo/XWindows state in a global.
ui_state_t g_ui_state;

// Set cairo/XWindow defaults.
static void ui_init_state(void)
{
  g_ui_state.u_line_width = 1.0;
  g_ui_state.u_line_red = 0.0;
  g_ui_state.u_line_green = 0.0;
  g_ui_state.u_line_blue = 0.0;
  g_ui_state.u_line_alpha = 1.0;
  g_ui_state.u_fill_red = 1.0;
  g_ui_state.u_fill_green = 1.0;
  g_ui_state.u_fill_blue = 1.0;
  g_ui_state.u_fill_alpha = 1.0;
  g_ui_state.u_background_fill_red = 1.0;
  g_ui_state.u_background_fill_green = 1.0;
  g_ui_state.u_background_fill_blue = 1.0;
  g_ui_state.u_window_width = 0;
  g_ui_state.u_window_height = 0;
  g_ui_state.u_display = NULL;
  g_ui_state.u_window = None;
  g_ui_state.u_screen = -1;
  g_ui_state.u_event_type = EV_NONE;
  // TODO get the correct default values.
  g_ui_state.u_kbd_timeout_default_ms = 660;
  g_ui_state.u_kbd_interval_default_ms = 250;
  g_ui_state.u_last_pause_key_time_millisec = 0;
}

static event_type_t ui_keypress_event(const Time ev_time_millisec)
{
  KeySym ks = XLookupKeysym((XKeyEvent *) &g_ui_state.u_event, 0);
  switch (ks)
  {
    case XK_Up:
      return g_ui_state.u_event_type = EV_KEY_UP;
      break;
    case XK_Down:
      return g_ui_state.u_event_type =  EV_KEY_DOWN;
      break;
    case XK_Left:
      return g_ui_state.u_event_type = EV_KEY_LEFT;
      break;
    case XK_Right:
      return g_ui_state.u_event_type = EV_KEY_RIGHT;
      break;
    case XK_End:
      return g_ui_state.u_event_type = EV_KEY_END;
      break;
    case XK_Pause:
      if (EV_NONE != g_ui_state.u_event_type &&
          EV_KEY_PAUSE != g_ui_state.u_event_type &&
          (ev_time_millisec - g_ui_state.u_last_pause_key_time_millisec) > 3000)
      {
        g_ui_state.u_last_pause_key_time_millisec = ev_time_millisec;
        return g_ui_state.u_event_type = EV_KEY_PAUSE;
      }
      // fallthru
    default:
      return g_ui_state.u_event_type = EV_NONE;
      break;
  }
}

static event_type_t ui_expose_event(void)
{
  XWindowAttributes win_attr;
  XGetWindowAttributes(g_ui_state.u_display, g_ui_state.u_window, &win_attr);
  g_ui_state.u_window_width = win_attr.width;
  g_ui_state.u_window_height = win_attr.height;
  if (!g_ui_state.u_surface)
  {
    g_ui_state.u_surface = cairo_xlib_surface_create(g_ui_state.u_display, g_ui_state.u_window,
                                                     DefaultVisual(g_ui_state.u_display,
                                                                   g_ui_state.u_screen),
                                                     g_ui_state.u_window_width,
                                                     g_ui_state.u_window_height);
    if (g_ui_state.u_cr)
      cairo_destroy(g_ui_state.u_cr);
    g_ui_state.u_cr = cairo_create(g_ui_state.u_surface);
  }
  else
    cairo_xlib_surface_set_size(g_ui_state.u_surface,
                                g_ui_state.u_window_width,
                                g_ui_state.u_window_height);
  return EV_PAINT;
}

void ui_set_kbd_repeat(uint32_t timeout_ms, uint32_t delay_ms)  // EXPORT
{
  XkbSetAutoRepeatRate(g_ui_state.u_display, XkbUseCoreKbd, timeout_ms, delay_ms);
  XFlush(g_ui_state.u_display);
}

void ui_set_default_kbd_repeat(void)  // EXPORT
{
  XkbSetAutoRepeatRate(g_ui_state.u_display, XkbUseCoreKbd,
                       g_ui_state.u_kbd_timeout_default_ms,
                       g_ui_state.u_kbd_interval_default_ms);
  XFlush(g_ui_state.u_display);
}

// Poll for next event or return EV_NONE.
event_type_t ui_next_event(void)  // EXPORT
{
  if (XPending(g_ui_state.u_display) <= 0)
    return EV_NONE;
  XNextEvent(g_ui_state.u_display, &g_ui_state.u_event);
  switch(g_ui_state.u_event.type)
  {
    case ButtonPress:
      return g_ui_state.u_event_type = EV_BUTTON_PRESS;
      break;
    case ButtonRelease:
      return g_ui_state.u_event_type = EV_BUTTON_RELEASE;
      break;
    case EnterNotify:
      return g_ui_state.u_event_type = EV_ENTER;
      break;
    case LeaveNotify:
      return g_ui_state.u_event_type = EV_LEAVE;
      break;
    case KeyPress:
      return ui_keypress_event(((XKeyEvent *) &g_ui_state.u_event)->time);
      break;
    case Expose:
      return ui_expose_event();
      break;
    case ClientMessage:
      return g_ui_state.u_event_type = EV_CLOSE;
      break;
    default:
      return g_ui_state.u_event_type = EV_NONE;
      break;
  }
}

event_type_t ui_get_last_event_type(void)  // EXPORT
{
  return g_ui_state.u_event_type;
}

// Current width of window.
uint32_t ui_get_width(void)  // EXPORT
{
  XWindowAttributes win_attr;
  XGetWindowAttributes(g_ui_state.u_display, g_ui_state.u_window, &win_attr);
  return win_attr.width;
}

// Current height of window.
uint32_t ui_get_height(void)  // EXPORT
{
  XWindowAttributes win_attr;
  XGetWindowAttributes(g_ui_state.u_display, g_ui_state.u_window, &win_attr);
  return win_attr.height;
}

// Line/path width.
void ui_set_line_width(float w)  // EXPORT
{
  cairo_set_line_width(g_ui_state.u_cr, w);
  g_ui_state.u_line_width = w;
}

// Line/path color/alpha.
void ui_set_line_rgba(float r, float g, float b, float a)  // EXPORT
{
  g_ui_state.u_line_red = r;
  g_ui_state.u_line_green = g;
  g_ui_state.u_line_blue = b;
  g_ui_state.u_line_alpha = a;
}

// Background color.
void ui_set_background_fill_rgb(float r, float g, float b)  // EXPORT
{
  g_ui_state.u_background_fill_red = r;
  g_ui_state.u_background_fill_green = g;
  g_ui_state.u_background_fill_blue = b;
}

// Fill color/alpha.
void ui_set_fill_rgba(float r, float g, float b, float a)  // EXPORT
{
  g_ui_state.u_fill_red = r;
  g_ui_state.u_fill_green = g;
  g_ui_state.u_fill_blue = b;
  g_ui_state.u_fill_alpha = a;
}

// Enable drawing; req'd with cairo+xlib.
void ui_begin_draw(void)  // EXPORT
{
  cairo_push_group(g_ui_state.u_cr);
}

// Make any drawing that occured after ui_begin_draw() visible in window.
void ui_end_draw(void)  // EXPORT
{
  cairo_pop_group_to_source(g_ui_state.u_cr);
  cairo_paint(g_ui_state.u_cr);
  cairo_surface_flush(g_ui_state.u_surface);
  XFlush(g_ui_state.u_display);
}

// Erase background and fill with default color.
void ui_fill_background(void)  // EXPORT
{
  XWindowAttributes win_attr;
  XGetWindowAttributes(g_ui_state.u_display, g_ui_state.u_window, &win_attr);
  cairo_set_source_rgba(g_ui_state.u_cr,
                        g_ui_state.u_background_fill_red,
                        g_ui_state.u_background_fill_green,
                        g_ui_state.u_background_fill_blue,
                        1.0);
  cairo_set_line_width(g_ui_state.u_cr, 0.0);
  cairo_rectangle(g_ui_state.u_cr, 0, 0, win_attr.width, win_attr.height);
  cairo_fill(g_ui_state.u_cr);
}

// Move Cairo "turtle" to a point.
void ui_move_to(float x, float y)  // EXPORT
{
  cairo_move_to(g_ui_state.u_cr, x, y);
}

// Draw line.
void ui_line(float x0, float y0, float x1, float y1)  // EXPORT
{
  cairo_set_source_rgba(g_ui_state.u_cr,
                        g_ui_state.u_line_red,
                        g_ui_state.u_line_green,
                        g_ui_state.u_line_blue,
                        g_ui_state.u_line_alpha);
  cairo_set_line_width(g_ui_state.u_cr, g_ui_state.u_line_width);
  cairo_move_to(g_ui_state.u_cr, x0, y0);
  cairo_line_to(g_ui_state.u_cr, x1, y1);
  cairo_stroke(g_ui_state.u_cr);
}

// Draw a (maybe filled) circle.
void ui_circle(float x, float y, float r, uint32_t fill)  // EXPORT
{
  cairo_set_source_rgba(g_ui_state.u_cr,
                        g_ui_state.u_line_red,
                        g_ui_state.u_line_green,
                        g_ui_state.u_line_blue,
                        g_ui_state.u_line_alpha);
  cairo_set_line_width(g_ui_state.u_cr, g_ui_state.u_line_width);
  cairo_move_to(g_ui_state.u_cr, x, y);
  cairo_new_sub_path(g_ui_state.u_cr);
  cairo_arc(g_ui_state.u_cr, x, y, r, 0.0, 2.0*M_PI);
  if (fill)
  {
    cairo_set_source_rgba(g_ui_state.u_cr,
                          g_ui_state.u_fill_red,
                          g_ui_state.u_fill_green,
                          g_ui_state.u_fill_blue,
                          g_ui_state.u_fill_alpha);
    cairo_fill(g_ui_state.u_cr);
  } else
    cairo_stroke(g_ui_state.u_cr);
}

// Draw a (maybe filled) rectangle.
void ui_rectangle(float x, float y, float w, float h, uint32_t fill)  // EXPORT
{
  cairo_set_source_rgba(g_ui_state.u_cr,
                        g_ui_state.u_line_red,
                        g_ui_state.u_line_green,
                        g_ui_state.u_line_blue,
                        g_ui_state.u_line_alpha);
  if (fill)
    cairo_set_line_width(g_ui_state.u_cr, 1.0);
  else
    cairo_set_line_width(g_ui_state.u_cr, g_ui_state.u_line_width);
  cairo_move_to(g_ui_state.u_cr, x, y);
  //cairo_new_sub_path(g_ui_state.u_cr);
  cairo_rectangle(g_ui_state.u_cr, x, y, w, h);
  if (fill)
  {
    cairo_set_source_rgba(g_ui_state.u_cr,
                          g_ui_state.u_fill_red,
                          g_ui_state.u_fill_green,
                          g_ui_state.u_fill_blue,
                          g_ui_state.u_fill_alpha);
    cairo_fill(g_ui_state.u_cr);
  }
  else
    cairo_stroke(g_ui_state.u_cr);
}

// Front window and prepare for event reception; return 0/1 on fail/success.
uint32_t ui_open_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h)  // EXPORT
{
  uint32_t retval = 1;
  Atom del_window;
  ui_init_state();
  if (!(g_ui_state.u_display = XOpenDisplay(NULL)))
    goto ERROR_EXIT_0;
  g_ui_state.u_screen = DefaultScreen(g_ui_state.u_display);
  if (!XkbGetAutoRepeatRate(g_ui_state.u_display, XkbUseCoreKbd,
                            &g_ui_state.u_kbd_timeout_default_ms,
                            &g_ui_state.u_kbd_interval_default_ms))
    goto ERROR_EXIT_0;
  g_ui_state.u_window = XCreateSimpleWindow(g_ui_state.u_display,
                                            DefaultRootWindow(g_ui_state.u_display),
                                            x, y,
                                            w, h,
                                            1,
                                            BlackPixel(g_ui_state.u_display,
                                                       g_ui_state.u_screen),
                                            WhitePixel(g_ui_state.u_display,
                                                       g_ui_state.u_screen));
  if (!g_ui_state.u_window)
    goto ERROR_EXIT_1;
  del_window = XInternAtom(g_ui_state.u_display, "WM_DELETE_WINDOW", 0);
  if (None == del_window)
    goto ERROR_EXIT_2;
  if (!XSetWMProtocols(g_ui_state.u_display, g_ui_state.u_window, &del_window, 1))
    goto ERROR_EXIT_2;
  // Even though the game doesn't use Button... events, they are still caught.
  // Without them it appears that ButtonPress generates an EnterNotify event and
  // ButtonRelease generates a LeaveNotify event.
  XSelectInput(g_ui_state.u_display, g_ui_state.u_window,
               ExposureMask | KeyPressMask | EnterWindowMask | LeaveWindowMask |
               ButtonPressMask | ButtonReleaseMask);
  XMapWindow(g_ui_state.u_display, g_ui_state.u_window);
  g_ui_state.u_surface = cairo_xlib_surface_create(g_ui_state.u_display, g_ui_state.u_window,
                                                   DefaultVisual(g_ui_state.u_display,
                                                                 g_ui_state.u_screen),
                                                   (int) w, (int) h);
  cairo_xlib_surface_set_size(g_ui_state.u_surface, w, h);
  g_ui_state.u_cr = cairo_create(g_ui_state.u_surface);
  goto OK_EXIT;
ERROR_EXIT_2:
  XDestroyWindow(g_ui_state.u_display, g_ui_state.u_window);
  g_ui_state.u_window = None;
ERROR_EXIT_1:
  g_ui_state.u_screen = -1;
  XCloseDisplay(g_ui_state.u_display);
  g_ui_state.u_display = NULL;
ERROR_EXIT_0:
  retval = 0;
OK_EXIT:
  return retval;
}

// Destroy window and free cairo resources.
void ui_quit(void)  // EXPORT
{
  if (g_ui_state.u_surface)
  {
    cairo_surface_destroy(g_ui_state.u_surface);
    g_ui_state.u_surface = NULL;
  }
  if (g_ui_state.u_cr)
  {
    cairo_destroy(g_ui_state.u_cr);
    g_ui_state.u_cr = NULL;
  }
  if (g_ui_state.u_display && None != g_ui_state.u_window)
  {
    ui_set_default_kbd_repeat();
    XDestroyWindow(g_ui_state.u_display, g_ui_state.u_window);
    XCloseDisplay(g_ui_state.u_display);
    g_ui_state.u_display = NULL;  // :(  assume that resources are freed in XCloseDisplay().
    g_ui_state.u_screen = -1;
    g_ui_state.u_window = None;
  }
}

static void paint(void)
{
  double alpha = atan(0.5);
  double C = cos(alpha);
  double S = sin(alpha);
  cairo_matrix_t M =
    {
      .xx = C,   .yx = -S,
      .xy = C,   .yy =  S,
      .x0 = g_x_pos, .y0 = g_y_pos
    };
  ui_begin_draw();
  cairo_set_source_surface(g_ui_state.u_cr, g_background_image, 0, 0);
  cairo_paint(g_ui_state.u_cr);
  cairo_set_matrix(g_ui_state.u_cr, &M);
  cairo_set_source_surface(g_ui_state.u_cr, g_image, 0, 0);
  cairo_paint(g_ui_state.u_cr);
  ui_end_draw();
}

int main(int argc, char **argv)
{
  event_type_t e;
  uint32_t width;
  uint32_t height;
  if (argc < 3)
  {
    fprintf(stderr, "usage: xdim <bgimage>.png <image>.png\n");
    return 1;
  }
  g_background_image = cairo_image_surface_create_from_png(argv[1]);
  g_image = cairo_image_surface_create_from_png(argv[2]);
  width = cairo_image_surface_get_width(g_background_image);
  height = cairo_image_surface_get_height(g_background_image);
  ui_open_window(10, 10, width, height);
  while (EV_CLOSE != (e = ui_next_event()))
  {
    switch (e)
    {
      case EV_PAINT:
        paint();
        break;
      case EV_CLOSE:
        ui_quit();
        return 0;
       break;
      case EV_KEY_UP:
        g_y_pos -= DY;
        paint();
        break;
      case EV_KEY_DOWN:
        g_y_pos += DY;
        paint();
        break;
      case EV_KEY_LEFT:
        g_x_pos -= DY;
        paint();
        break;
      case EV_KEY_RIGHT:
        g_x_pos += DY;
        paint();
        break;
      case EV_KEY_END:
        ui_quit();
        return 0;
        break;
      default:
        break;
    }
  }
  ui_quit();
  return 0;
}

//* EOF
