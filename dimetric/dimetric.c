#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <cairo/cairo.h>
#include <gtk-3.0/gtk/gtk.h>
#include <math.h>

// TODO: "port" to xlib

const double eps = 0.00001;

double pos_x = 300;
double pos_y = 300;
double goal_x = 300;
double goal_y = 300;

const double dx = 200;
const double dy = 200;

double vx_frame = 0;
double vy_frame = 0;

const double fps = 190.0;
const double paint_interval_millisec = 1000.0/fps;

const double speed = 900;  // px/sec

double pxpersec_to_pxperframe(double px)
{
  return px*paint_interval_millisec/1000.0;
}

static void do_drawing(cairo_t *, GtkWidget *);

struct {
  cairo_surface_t *image;
  cairo_surface_t *background_image;
} glob;

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr,
    gpointer user_data)
{
  do_drawing(cr, widget);
  return FALSE;
}

static void do_drawing(cairo_t *cr, GtkWidget *widget)
{
  GtkWidget *win = gtk_widget_get_toplevel(widget);
  int width, height;
  double alpha = atan(0.5);
  double C = cos(alpha);
  double S = sin(alpha);
  cairo_matrix_t M =
  {
    .xx = C,   .yx = -S,
    .xy = C,   .yy =  S,
    .x0 = pos_x, .y0 = pos_y
  };
  gtk_window_get_size(GTK_WINDOW(win), &width, &height);
  cairo_set_source_surface(cr, glob.background_image, 0, 0);
  cairo_paint(cr);

  cairo_set_matrix(cr, &M);
  cairo_set_source_surface(cr, glob.image, 0, 0);
  cairo_arc(cr, pos_x, pos_y, radius, 0, 2*M_PI);
  int w = cairo_image_surface_get_width(glob.image);
  int h = cairo_image_surface_get_height(glob.image);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_clip(cr);
  cairo_paint(cr);
}

static gboolean time_handler(GtkWidget *widget)
{
  if (fabs(pos_x - goal_x) < eps)
    vx_frame = 0;
  else
    pos_x += vx_frame;
  if (fabs(pos_y - goal_y) < eps)
    vy_frame = 0;
  else
    pos_y += vy_frame;
  gtk_widget_queue_draw(widget);
  return TRUE;
}

gboolean on_key_press(GtkWidget* self,
                      GdkEventKey *event,
                      gpointer notused)
{
  gboolean result = FALSE;
  //printf("key press 0x%x\n", event->keyval);
  switch (event->keyval)
  {
    case GDK_KEY_Up:
      goal_y = pos_y - dy;
      vy_frame = -pxpersec_to_pxperframe(speed);
      result = true;
      break;
    case GDK_KEY_Down:
      goal_y = pos_y + dy;
      vy_frame = pxpersec_to_pxperframe(speed);
      result = true;
      break;
    case GDK_KEY_Left:
      goal_x = pos_x - dx;
      vx_frame = -pxpersec_to_pxperframe(speed);
      result = true;
      break;
    case GDK_KEY_Right:
      goal_x = pos_x + dx;
      vx_frame = pxpersec_to_pxperframe(speed);
      result = true;
      break;
  }
  return result;
}

int main(int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *darea;
  gint width, height;

  glob.image = cairo_image_surface_create_from_png("blue.png");
  glob.background_image = cairo_image_surface_create_from_png("polka-dots.png");
  width = cairo_image_surface_get_width(glob.background_image);
  height = cairo_image_surface_get_height(glob.background_image);
  printf("%d, %d\n", width, height);
  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  darea = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER (window), darea);
  gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
  g_signal_connect(G_OBJECT(darea), "draw",
                   G_CALLBACK(on_draw_event), NULL);
  g_signal_connect(G_OBJECT(window), "destroy",
                   G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(window), "key_press_event",
                   G_CALLBACK(on_key_press), NULL);
  
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), width, height);
  gtk_window_set_title(GTK_WINDOW(window), "Clip image");

  gtk_widget_show_all(window);
  g_timeout_add(paint_interval_millisec, (GSourceFunc) time_handler, (gpointer) window);

  gtk_main();

  cairo_surface_destroy(glob.image);
  return 0;
}
