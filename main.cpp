#include <cstdio>
#include <map>
#include <vector>

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <signal.h>

static double start_x;
static double start_y;
static double prev_x;
static double prev_y;
static const char *colors[6] = {"#E40303", "#FF8C00", "#FFED00",
                                "#008026", "#24408E", "#732982"};
static int color_index = 0;

GtkApplication *app;
static std::vector<GtkWindow *> windows;
static std::map<GtkWidget *, cairo_surface_t *> surface;

static void clear_surface(GtkWidget *widget) {
  cairo_t *cr = cairo_create(surface[widget]);
  cairo_set_source_rgba(cr, 1, 1, 1, 0);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);
  cairo_destroy(cr);
}

static void resize_cb(GtkWidget *widget, int width, int height, gpointer data) {
  if (gtk_native_get_surface(gtk_widget_get_native(widget))) {
    auto new_surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, gtk_widget_get_width(widget),
        gtk_widget_get_height(widget));
    if (surface[widget]) {
      cairo_t *cr = cairo_create(new_surface);
      cairo_set_source_surface(cr, surface[widget], 0, 0);
      cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
      cairo_rectangle(cr, 0, 0, width, height);
      cairo_paint(cr);
      cairo_destroy(cr);
      cairo_surface_destroy(surface[widget]);
    } else {
      clear_surface(widget);
    }
    surface[widget] = new_surface;
  }
}

static void draw_cb(GtkDrawingArea *drawing_area, cairo_t *cr, int width,
                    int height, gpointer data) {
  cairo_set_source_surface(cr, surface[GTK_WIDGET(drawing_area)], 0, 0);
  cairo_paint(cr);
}

static void draw_brush(GtkWidget *widget, double x, double y) {
  GdkRGBA color;
  gdk_rgba_parse(&color, colors[color_index]);
  cairo_t *cr = cairo_create(surface[widget]);
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

static void erase_brush(GtkWidget *widget, double x, double y) {
  GdkRGBA color;
  gdk_rgba_parse(&color, colors[color_index]);
  cairo_t *cr = cairo_create(surface[widget]);
  cairo_move_to(cr, prev_x + 2, prev_y + 2);
  cairo_line_to(cr, x + 2, y + 2);
  cairo_set_line_width(cr, 40.0);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 1, 1, 1, 0);
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

static void erase_begin(GtkGestureDrag *gesture, double x, double y,
                        GtkWidget *area) {
  start_x = x;
  start_y = y;
  prev_x = x;
  prev_y = y;
  gtk_widget_set_cursor_from_name(area, "not-allowed");
  erase_brush(area, x, y);
}

static void erase_update(GtkGestureDrag *gesture, double x, double y,
                         GtkWidget *area) {
  erase_brush(area, start_x + x, start_y + y);
}

static void erase_end(GtkGestureDrag *gesture, double x, double y,
                      GtkWidget *area) {
  gtk_widget_set_cursor_from_name(area, "crosshair");
  erase_brush(area, start_x + x, start_y + y);
}

static void pressed(GtkGestureClick *gesture, int n_press, double x, double y,
                    GtkWidget *area) {
  clear_surface(area);
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

static void signal_handler(int sig) {
  if (sig == SIGUSR1 && gtk_layer_is_supported()) {
    for (auto window : windows) {
      if (gtk_layer_get_layer(window) == GTK_LAYER_SHELL_LAYER_BOTTOM) {
        gtk_layer_set_keyboard_mode(window,
                                    GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
      } else {
        gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BOTTOM);
      }
    }
  } else if (sig == SIGTERM) {
    g_application_quit(G_APPLICATION(app));
  }
}

static void key_press(GtkEventControllerKey *self, guint keyval, guint keycode,
                      GdkModifierType state, gpointer data) {
  auto window = reinterpret_cast<GtkWindow *>(data);
  if (keyval == GDK_KEY_Escape) {
    // gtk_window_close(window);
    g_application_quit(G_APPLICATION(app));
  }
}

static void realize(GtkWindow *window, gpointer data) {
  auto area = reinterpret_cast<GtkWidget *>(data);
  // gtk_widget_grab_focus(area);
  auto surf = gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(window)));
  gdk_surface_set_input_region(surf, NULL);
}

static void activate(GtkApplication *app, [[maybe_unused]] gpointer data) {
  auto display = gdk_display_get_default();
  auto list = gdk_display_get_monitors(display);
  for (guint i = 0; i < g_list_model_get_n_items(list); i++) {
    auto mon = reinterpret_cast<GdkMonitor *>(g_list_model_get_item(list, i));

    /*auto connector = gdk_monitor_get_connector(mon);
    if (std::strcmp(connector, output) != 0) {
      continue;
    }*/

    auto window = GTK_WINDOW(gtk_application_window_new(app));
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, R""""(
window { background: rgba(0, 0, 0, 0); }
* { margin: 0; padding: 0; border: none; border-radius: 0; })"""");
    GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(window));
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    if (gtk_layer_is_supported()) {
      gtk_layer_init_for_window(window);
      gtk_layer_set_namespace(window, "wallace");
      gtk_layer_set_monitor(window, mon);
      gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
      // gtk_layer_set_exclusive_zone(window, 0);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
      gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
      GdkRectangle geometry;
      gdk_monitor_get_geometry(mon, &geometry);
      gtk_window_set_default_size(window, geometry.width, geometry.height);
      // gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
      // gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
      gtk_layer_set_keyboard_mode(window,
                                  GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    }
    gtk_window_set_title(GTK_WINDOW(window), "wallace");
    // GtkWidget *frame = gtk_frame_new(NULL);
    GtkWidget *drawing_area = gtk_drawing_area_new();
    // gtk_frame_set_child(GTK_FRAME(frame), drawing_area);
    gtk_window_set_child(GTK_WINDOW(window), drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_cb,
                                   NULL, NULL);
    g_signal_connect_after(drawing_area, "resize", G_CALLBACK(resize_cb), NULL);

    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(drag));
    g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin), drawing_area);
    g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update),
                     drawing_area);
    g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), drawing_area);

    GtkGesture *erase = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(erase),
                                  GDK_BUTTON_SECONDARY);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(erase));
    g_signal_connect(erase, "drag-begin", G_CALLBACK(erase_begin),
                     drawing_area);
    g_signal_connect(erase, "drag-update", G_CALLBACK(erase_update),
                     drawing_area);
    g_signal_connect(erase, "drag-end", G_CALLBACK(erase_end), drawing_area);

    GtkGesture *press = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(press), GDK_BUTTON_MIDDLE);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(press));
    g_signal_connect_after(press, "pressed", G_CALLBACK(pressed), drawing_area);

    GtkEventController *scroll =
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    gtk_widget_add_controller(drawing_area, GTK_EVENT_CONTROLLER(scroll));
    g_signal_connect(scroll, "scroll", G_CALLBACK(scrolled), drawing_area);

    auto *keys = gtk_event_controller_key_new();
    gtk_widget_add_controller(GTK_WIDGET(window), GTK_EVENT_CONTROLLER(keys));
    g_signal_connect(keys, "key-pressed", G_CALLBACK(key_press), window);

    gtk_widget_set_cursor_from_name(drawing_area, "crosshair");

    g_signal_connect_after(window, "realize", G_CALLBACK(realize),
                           drawing_area);

    gtk_window_present(window);
    windows.push_back(window);
    if (!gtk_layer_is_supported())
      break;
  }
}

int main(int argc, char *argv[]) {
  signal(SIGUSR1, signal_handler);
  signal(SIGTERM, signal_handler);
  app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect_after(app, "activate", G_CALLBACK(activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
