#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>

static cairo_surface_t *surface = NULL;
static double start_x;
static double start_y;
static double prev_x;
static double prev_y;
static const char *colors[6] = {"#E40303", "#FF8C00", "#FFED00",
                                "#008026", "#24408E", "#732982"};
static int color_index = 0;

static void clear_surface(void) {
  cairo_t *cr = cairo_create(surface);
  cairo_set_source_rgba(cr, 1, 1, 1, 0);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_destroy(cr);
}

static void resize_cb(GtkWidget *widget, int width, int height, gpointer data) {
  if (surface) {
    cairo_surface_destroy(surface);
    surface = NULL;
  }
  if (gtk_native_get_surface(gtk_widget_get_native(widget))) {
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                         gtk_widget_get_width(widget),
                                         gtk_widget_get_height(widget));
    clear_surface();
  }
}

static void draw_cb(GtkDrawingArea *drawing_area, cairo_t *cr, int width,
                    int height, gpointer data) {
  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_paint(cr);
}

static void draw_brush(GtkWidget *widget, double x, double y) {
  GdkRGBA color;
  gdk_rgba_parse(&color, colors[color_index]);
  cairo_t *cr = cairo_create(surface);
  cairo_move_to(cr, prev_x + 2, prev_y + 2);
  cairo_line_to(cr, x + 2, y + 2);
  cairo_set_line_width(cr, 4.0);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
  cairo_stroke(cr);
  cairo_destroy(cr);
  gtk_widget_queue_draw(widget);
  prev_x = x;
  prev_y = y;
}

static void drag_begin(GtkGestureDrag *gesture, double x, double y,
                       GtkWidget *area) {
  start_x = x;
  start_y = y;
  prev_x = x;
  prev_y = y;
  draw_brush(area, x, y);
}

static void drag_update(GtkGestureDrag *gesture, double x, double y,
                        GtkWidget *area) {
  draw_brush(area, start_x + x, start_y + y);
}

static void drag_end(GtkGestureDrag *gesture, double x, double y,
                     GtkWidget *area) {
  draw_brush(area, start_x + x, start_y + y);
}

static void pressed(GtkGestureClick *gesture, int n_press, double x, double y,
                    GtkWidget *area) {
  clear_surface();
  gtk_widget_queue_draw(area);
}

static void scrolled(GtkEventControllerScroll *scroll, double dx, double dy,
                     GtkWidget *area) {
  if (dy < 0)
    color_index++;
  if (dy > 0)
    color_index--;
  color_index = (color_index + 6) % 6;
}

static void close_window(void) {
  if (surface)
    cairo_surface_destroy(surface);
}

static void activate(GtkApplication *app, [[maybe_unused]] gpointer user_data) {
  GtkWindow *window = GTK_WINDOW(gtk_application_window_new(app));
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider, R""""(
window { background: rgba(0, 0, 0, 0); }
* { margin: 0; padding: 0; border: none; })"""");
  GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));
  gtk_style_context_add_provider_for_display(
      display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
  gtk_layer_init_for_window(window);
  gtk_layer_set_namespace(window, "wallace");
  gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_exclusive_zone(window, 0);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
  gtk_window_set_title(GTK_WINDOW(window), "wallace");
  g_signal_connect(window, "destroy", G_CALLBACK(close_window), NULL);
  GtkWidget *frame = gtk_frame_new(NULL);
  gtk_window_set_child(GTK_WINDOW(window), frame);
  GtkWidget *drawing_area = gtk_drawing_area_new();
  gtk_frame_set_child(GTK_FRAME(frame), drawing_area);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_cb, NULL,
                                 NULL);
  g_signal_connect_after(drawing_area, "resize", G_CALLBACK(resize_cb), NULL);

  GtkGesture *drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
  gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(drag));
  g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin), drawing_area);
  g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), drawing_area);
  g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), drawing_area);

  GtkGesture *press = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press),
                                GDK_BUTTON_SECONDARY);
  gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(press));
  g_signal_connect(press, "pressed", G_CALLBACK(pressed), drawing_area);

  GtkEventController *scroll =
      gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(scroll));
  g_signal_connect(scroll, "scroll", G_CALLBACK(scrolled), drawing_area);

  gtk_widget_set_cursor_from_name(drawing_area, "crosshair");
  gtk_window_present(window);
}

int main(int argc, char *argv[]) {
  GtkApplication *app =
      gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
