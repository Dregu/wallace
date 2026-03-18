#include <cstdio>
#include <map>
#include <vector>

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <signal.h>

#define COLORS 6

static std::map<int, double> draw_x;
static std::map<int, double> draw_y;
static std::map<int, double> prev_x;
static std::map<int, double> prev_y;
static std::map<int, bool> drawing;
static std::map<int, bool> erasing;
static const char* colors[COLORS] = {"#E40303", "#FF8C00", "#FFED00", "#008026", "#24408E", "#732982"};
static int color_index = 0;

GtkApplication* app;
static std::vector<GtkWindow*> windows;
static std::map<GtkWidget*, cairo_surface_t*> surface;
static bool passthrough = false;

static void clear_surface(GtkWidget* widget)
{
    cairo_t* cr = cairo_create(surface[widget]);
    cairo_set_source_rgba(cr, 1, 1, 1, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_destroy(cr);
}

static void resize_cb(GtkWidget* widget, int width, int height, gpointer data)
{
    if (gtk_native_get_surface(gtk_widget_get_native(widget)))
    {
        auto new_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, gtk_widget_get_width(widget), gtk_widget_get_height(widget));
        if (surface[widget])
        {
            cairo_t* cr = cairo_create(new_surface);
            cairo_set_source_surface(cr, surface[widget], 0, 0);
            cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
            cairo_rectangle(cr, 0, 0, width, height);
            cairo_paint(cr);
            cairo_destroy(cr);
            cairo_surface_destroy(surface[widget]);
        }
        else
        {
            clear_surface(widget);
        }
        surface[widget] = new_surface;
    }
}

static void draw_cb(GtkDrawingArea* drawing_area, cairo_t* cr, int width, int height, gpointer data)
{
    cairo_set_source_surface(cr, surface[GTK_WIDGET(drawing_area)], 0, 0);
    cairo_paint(cr);
}

static void draw_brush(double x, double y, int i, GtkWidget* widget)
{
    GdkRGBA color;
    int idx = (color_index + (i > 0 ? i - 1 + drawing[0] : 0)) % COLORS;
    gdk_rgba_parse(&color, colors[idx]);
    cairo_t* cr = cairo_create(surface[widget]);
    cairo_move_to(cr, prev_x[i], prev_y[i]);
    cairo_line_to(cr, x, y);
    cairo_set_line_width(cr, 4.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_stroke(cr);
    cairo_destroy(cr);
    gtk_widget_queue_draw(widget);
    prev_x[i] = x;
    prev_y[i] = y;
}

static void erase_brush(double x, double y, int i, GtkWidget* widget)
{
    GdkRGBA color;
    gdk_rgba_parse(&color, colors[color_index]);
    cairo_t* cr = cairo_create(surface[widget]);
    cairo_move_to(cr, prev_x[i], prev_y[i]);
    cairo_line_to(cr, x, y);
    cairo_set_line_width(cr, 60.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 1, 1, 1, 0);
    cairo_stroke(cr);
    cairo_destroy(cr);
    gtk_widget_queue_draw(widget);
    prev_x[i] = x;
    prev_y[i] = y;
}

static void pressed_left(double x, double y, GtkWidget* area)
{
    prev_x[0] = x;
    prev_y[0] = y;
    drawing[0] = true;
    erasing[0] = false;
    draw_brush(x, y, 0, area);
}

static void released_left(double x, double y, GtkWidget* area)
{ drawing[0] = false; }

static void pressed_right(double x, double y, GtkWidget* area)
{
    prev_x[0] = x;
    prev_y[0] = y;
    erasing[0] = true;
    drawing[0] = false;
    erase_brush(x, y, 0, area);
    for (auto window : windows)
        gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "not-allowed");
}

static void released_right(double x, double y, GtkWidget* area)
{
    erasing[0] = false;
    for (auto window : windows)
        gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "crosshair");
}

static void pressed_middle(double x, double y, GtkWidget* area)
{
    clear_surface(area);
    gtk_widget_queue_draw(area);
}

static void draw_update(double x, double y, int i, GtkWidget* area)
{
    if (drawing[i])
        draw_brush(x, y, i, area);
    else if (erasing[i])
        erase_brush(x, y, i, area);
}

static void scrolled(GtkEventControllerScroll* scroll, double dx, double dy, GtkWidget* area)
{
    if (dy < 0)
        color_index++;
    if (dy > 0)
        color_index--;
    color_index = (color_index + COLORS) % COLORS;
}

static void touch_begin(double x, double y, int i, GtkWidget* area)
{
    draw_x[i] = x;
    draw_y[i] = y;
    prev_x[i] = x;
    prev_y[i] = y;
    drawing[i] = true;
    erasing[i] = false;
    draw_brush(x, y, i, area);
}

static void touch_end(double x, double y, int i, GtkWidget* area)
{ drawing[i] = false; }

static void touch_update(double x, double y, int i, GtkWidget* area)
{
    if (!drawing[i])
        touch_begin(x, y, i, area);
    else
        draw_brush(x, y, i, area);
}

static void raw_update(GtkEventController* controller, GdkEvent* event, GtkWidget* area)
{
    auto type = gdk_event_get_event_type(event);
    double x, y;
    if (type == GDK_TOUCH_BEGIN || type == GDK_TOUCH_UPDATE | type == GDK_TOUCH_END)
    {
        auto seq = gdk_event_get_event_sequence(event);
        uintptr_t i = (uintptr_t)seq;
        gdk_event_get_position(event, &x, &y);
        if (type == GDK_TOUCH_BEGIN)
            touch_begin(x, y, i, area);
        else if (type == GDK_TOUCH_END)
            touch_end(x, y, i, area);
        else
            touch_update(x, y, i, area);
        return;
    }
    else if (type == GDK_BUTTON_PRESS)
    {
        gdk_event_get_position(event, &x, &y);
        auto button = gdk_button_event_get_button(event);
        if (button == GDK_BUTTON_PRIMARY)
            pressed_left(x, y, area);
        else if (button == GDK_BUTTON_SECONDARY)
            pressed_right(x, y, area);
        else if (button == GDK_BUTTON_MIDDLE)
            pressed_middle(x, y, area);
    }
    else if (type == GDK_BUTTON_RELEASE)
    {
        gdk_event_get_position(event, &x, &y);
        auto button = gdk_button_event_get_button(event);
        if (button == GDK_BUTTON_PRIMARY)
            released_left(x, y, area);
        else if (button == GDK_BUTTON_SECONDARY)
            released_right(x, y, area);
    }
    else if (type == GDK_MOTION_NOTIFY)
    {
        gdk_event_get_position(event, &x, &y);
        draw_update(x, y, 0, area);
    }
}

static void signal_handler(int sig)
{
    static const auto empty_region = cairo_region_create();
    if (sig == SIGUSR1 && gtk_layer_is_supported())
    {
        for (auto window : windows)
        {
            if (gtk_layer_get_layer(window) == GTK_LAYER_SHELL_LAYER_BOTTOM)
            {
                gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
                gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
            }
            else
            {
                gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
                gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_BOTTOM);
            }
        }
    }
    else if (sig == SIGUSR2 && gtk_layer_is_supported())
    {
        for (auto window : windows)
        {
            auto surf = gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(window)));
            if (passthrough)
            {
                gdk_surface_set_input_region(surf, NULL);
                gtk_widget_remove_css_class(GTK_WIDGET(window), "pass");
                // gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
                // gtk_widget_set_visible(GTK_WIDGET(window), TRUE);
            }
            else
            {
                gdk_surface_set_input_region(surf, empty_region);
                gtk_widget_add_css_class(GTK_WIDGET(window), "pass");
                // gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
                // gtk_widget_set_visible(GTK_WIDGET(window), TRUE);
            }
        }
        passthrough ^= true;
    }
    else if (sig == SIGTERM)
    {
        g_application_quit(G_APPLICATION(app));
    }
}

static void key_press(GtkEventControllerKey* self, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
    auto window = reinterpret_cast<GtkWindow*>(data);
    if (keyval == GDK_KEY_Escape)
    {
        g_application_quit(G_APPLICATION(app));
    }
}

static void realize(GtkWindow* window, gpointer data)
{
    auto area = reinterpret_cast<GtkWidget*>(data);
    auto surf = gtk_native_get_surface(gtk_widget_get_native(GTK_WIDGET(window)));
    gdk_surface_set_input_region(surf, NULL);
}

static void activate(GtkApplication* app, [[maybe_unused]] gpointer data)
{
    auto display = gdk_display_get_default();
    auto list = gdk_display_get_monitors(display);
    for (guint i = 0; i < g_list_model_get_n_items(list); i++)
    {
        auto mon = reinterpret_cast<GdkMonitor*>(g_list_model_get_item(list, i));

        /*auto connector = gdk_monitor_get_connector(mon);
        if (std::strcmp(connector, output) != 0) {
          continue;
        }*/

        auto window = GTK_WINDOW(gtk_application_window_new(app));
        gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
        GtkCssProvider* provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(provider, R""""(
window { background: rgba(0, 0, 0, 0); }
window.pass { opacity: 0.33; }
* { margin: 0; padding: 0; border: none; border-radius: 0; })"""");
        GdkDisplay* display = gtk_widget_get_display(GTK_WIDGET(window));
        gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
        if (gtk_layer_is_supported())
        {
            gtk_layer_init_for_window(window);
            gtk_layer_set_namespace(window, "wallace");
            gtk_layer_set_monitor(window, mon);
            gtk_layer_set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
            gtk_layer_set_exclusive_zone(window, -1);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            GdkRectangle geometry;
            gdk_monitor_get_geometry(mon, &geometry);
            gtk_window_set_default_size(window, geometry.width, geometry.height);
            gtk_layer_set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        }
        gtk_window_set_title(GTK_WINDOW(window), "wallace");
        GtkWidget* drawing_area = gtk_drawing_area_new();
        gtk_widget_set_can_target(drawing_area, FALSE);
        gtk_window_set_child(GTK_WINDOW(window), drawing_area);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_cb, NULL, NULL);
        g_signal_connect_after(drawing_area, "resize", G_CALLBACK(resize_cb), NULL);

        auto* legacy = gtk_event_controller_legacy_new();
        gtk_widget_add_controller(GTK_WIDGET(window), GTK_EVENT_CONTROLLER(legacy));
        g_signal_connect(legacy, "event", G_CALLBACK(raw_update), drawing_area);

        GtkEventController* scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
        gtk_widget_add_controller(GTK_WIDGET(window), GTK_EVENT_CONTROLLER(scroll));
        g_signal_connect(scroll, "scroll", G_CALLBACK(scrolled), drawing_area);

        auto* keys = gtk_event_controller_key_new();
        gtk_widget_add_controller(GTK_WIDGET(window), GTK_EVENT_CONTROLLER(keys));
        g_signal_connect(keys, "key-pressed", G_CALLBACK(key_press), window);

        gtk_widget_set_cursor_from_name(drawing_area, "crosshair");

        g_signal_connect_after(window, "realize", G_CALLBACK(realize), drawing_area);

        gtk_window_present(window);

        gtk_widget_set_visible(GTK_WIDGET(window), FALSE);
        gtk_widget_set_visible(GTK_WIDGET(window), TRUE);
        gtk_widget_set_cursor_from_name(GTK_WIDGET(window), "crosshair");
        windows.push_back(window);
        if (!gtk_layer_is_supported())
            break;
    }
}

int main(int argc, char* argv[])
{
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGTERM, signal_handler);
    app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect_after(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
