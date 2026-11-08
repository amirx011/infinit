#include "my_application.h"

#include <flutter_linux/flutter_linux.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "flutter/generated_plugin_registrant.h"

struct _MyApplication {
  GtkApplication parent_instance;
  char** dart_entrypoint_arguments;
};

G_DEFINE_TYPE(MyApplication, my_application, GTK_TYPE_APPLICATION)

// Loads the brand icons that ship inside the bundle.  A list of sizes lets
// GTK/X11 pick a crisp icon for the title bar, taskbar and app switcher
// instead of downscaling one large bitmap.
static void my_application_set_brand_icon(GtkWindow* window) {
  g_autofree gchar* exe_path = g_file_read_link("/proc/self/exe", nullptr);
  if (exe_path == nullptr) {
    return;
  }
  g_autofree gchar* exe_dir = g_path_get_dirname(exe_path);
  const gchar* names[] = {
      "infinit-icon-16.png",  "infinit-icon-24.png",
      "infinit-icon-32.png",  "infinit-icon-48.png",
      "infinit-icon-64.png",  "infinit-icon-96.png",
      "infinit-icon-128.png", "infinit-icon-192.png",
      "infinit-icon-256.png",
  };
  GList* icons = nullptr;
  for (gsize i = 0; i < G_N_ELEMENTS(names); i++) {
    g_autofree gchar* path = g_build_filename(
        exe_dir, "data/flutter_assets/assets/branding", names[i], nullptr);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
      continue;
    }
    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(path, &error);
    if (pixbuf != nullptr) {
      icons = g_list_append(icons, pixbuf);
    } else if (error != nullptr) {
      g_error_free(error);
    }
  }
  if (icons != nullptr) {
    gtk_window_set_icon_list(window, icons);
    gtk_window_set_default_icon_list(icons);
    g_list_free_full(icons, g_object_unref);
  }
}

// Modern, borderless window buttons for the custom header bar.  The system
// decorations are replaced so the title bar matches the app instead of the
// desktop theme, while the icons stay symbolic and follow the GTK theme.
static void my_application_install_window_css(void) {
  static gboolean installed = FALSE;
  if (installed) {
    return;
  }
  installed = TRUE;

  GtkCssProvider* provider = gtk_css_provider_new();
  const gchar* css =
      "headerbar.infinit-header {"
      "  min-height: 48px;"
      "  padding: 0 6px;"
      "  border-radius: 0;"
      "  box-shadow: none;"
      "}"
      ".infinit-window-button {"
      "  min-width: 32px;"
      "  min-height: 32px;"
      "  padding: 0;"
      "  margin: 0 1px;"
      "  border-radius: 9px;"
      "  background-image: none;"
      "  background-color: transparent;"
      "  border: 1px solid transparent;"
      "  box-shadow: none;"
      "}"
      ".infinit-window-button:hover {"
      "  background-color: rgba(127, 127, 127, 0.18);"
      "  border-color: rgba(127, 127, 127, 0.12);"
      "}"
      ".infinit-window-button:active {"
      "  background-color: rgba(127, 127, 127, 0.28);"
      "}"
      ".infinit-window-button.close:hover {"
      "  background-color: #e01b24;"
      "  border-color: #c01c28;"
      "  color: #ffffff;"
      "}"
      ".infinit-window-button.close:active {"
      "  background-color: #c01c28;"
      "  color: #ffffff;"
      "}";
  gtk_css_provider_load_from_data(provider, css, -1, nullptr);
  gtk_style_context_add_provider_for_screen(
      gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

typedef enum {
  INFINIT_WINDOW_ICON_MINIMIZE,
  INFINIT_WINDOW_ICON_MAXIMIZE,
  INFINIT_WINDOW_ICON_RESTORE,
  INFINIT_WINDOW_ICON_CLOSE
} InfinitWindowIcon;

// Draw the window controls with Cairo instead of loading them from the icon
// theme.  Symbolic names such as window-minimize-symbolic are absent on many
// desktops, which left the buttons blank.
static gboolean my_application_draw_window_icon(GtkWidget* widget,
                                                cairo_t* cr,
                                                gpointer data) {
  (void)data;
  GtkWidget* owner = gtk_widget_get_parent(widget);
  if (owner == nullptr) {
    owner = widget;
  }
  GtkStyleContext* context = gtk_widget_get_style_context(owner);
  GdkRGBA color;
  gtk_style_context_get_color(context, gtk_widget_get_state_flags(owner),
                              &color);
  gdk_cairo_set_source_rgba(cr, &color);

  const InfinitWindowIcon icon = (InfinitWindowIcon)GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(widget), "infinit-window-icon"));
  const double width = gtk_widget_get_allocated_width(widget);
  const double height = gtk_widget_get_allocated_height(widget);
  const double cx = width / 2.0;
  const double cy = height / 2.0;
  const double half = 4.6;
  cairo_set_line_width(cr, 1.9);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

  switch (icon) {
    case INFINIT_WINDOW_ICON_MINIMIZE:
      cairo_move_to(cr, cx - half, cy);
      cairo_line_to(cr, cx + half, cy);
      break;
    case INFINIT_WINDOW_ICON_MAXIMIZE:
      cairo_rectangle(cr, cx - half, cy - half, 2 * half, 2 * half);
      break;
    case INFINIT_WINDOW_ICON_RESTORE:
      cairo_rectangle(cr, cx - half, cy - half + 2, 2 * half - 2,
                      2 * half - 2);
      cairo_rectangle(cr, cx - half + 2, cy - half, 2 * half - 2,
                      2 * half - 2);
      break;
    case INFINIT_WINDOW_ICON_CLOSE:
      cairo_move_to(cr, cx - half, cy - half);
      cairo_line_to(cr, cx + half, cy + half);
      cairo_move_to(cr, cx + half, cy - half);
      cairo_line_to(cr, cx - half, cy + half);
      break;
  }
  cairo_stroke(cr);
  return FALSE;
}

static GtkButton* my_application_window_button(InfinitWindowIcon icon,
                                               const gchar* tooltip,
                                               const gchar* css_class) {
  GtkButton* button = GTK_BUTTON(gtk_button_new());
  GtkWidget* drawing = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing, 20, 20);
  g_object_set_data(G_OBJECT(drawing), "infinit-window-icon",
                    GINT_TO_POINTER((gint)icon));
  g_signal_connect(drawing, "draw",
                   G_CALLBACK(my_application_draw_window_icon), nullptr);
  gtk_widget_show(drawing);
  gtk_button_set_image(button, drawing);
  gtk_button_set_relief(button, GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text(GTK_WIDGET(button), tooltip);
  gtk_widget_set_valign(GTK_WIDGET(button), GTK_ALIGN_CENTER);
  gtk_widget_set_focus_on_click(GTK_WIDGET(button), FALSE);
  GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(button));
  gtk_style_context_add_class(context, "infinit-window-button");
  if (css_class != nullptr) {
    gtk_style_context_add_class(context, css_class);
  }
  return button;
}

static void my_application_minimize(GtkButton* button, gpointer data) {
  (void)button;
  gtk_window_iconify(GTK_WINDOW(data));
}

static void my_application_update_maximize_icon(GtkWindow* window) {
  GtkWidget* drawing = GTK_WIDGET(
      g_object_get_data(G_OBJECT(window), "infinit-maximize-image"));
  if (drawing == nullptr) {
    return;
  }
  g_object_set_data(
      G_OBJECT(drawing), "infinit-window-icon",
      GINT_TO_POINTER((gint)(gtk_window_is_maximized(window)
                                 ? INFINIT_WINDOW_ICON_RESTORE
                                 : INFINIT_WINDOW_ICON_MAXIMIZE)));
  gtk_widget_queue_draw(drawing);
}

static void my_application_toggle_maximize(GtkButton* button, gpointer data) {
  (void)button;
  GtkWindow* window = GTK_WINDOW(data);
  if (gtk_window_is_maximized(window)) {
    gtk_window_unmaximize(window);
  } else {
    gtk_window_maximize(window);
  }
  my_application_update_maximize_icon(window);
}

static gboolean my_application_window_state_changed(GtkWidget* widget,
                                                     GdkEventWindowState* event,
                                                     gpointer data) {
  (void)event;
  (void)data;
  my_application_update_maximize_icon(GTK_WINDOW(widget));
  return FALSE;
}

static void my_application_close(GtkButton* button, gpointer data) {
  (void)button;
  gtk_window_close(GTK_WINDOW(data));
}

static GtkHeaderBar* my_application_build_header_bar(GtkWindow* window) {
  GtkHeaderBar* header_bar = GTK_HEADER_BAR(gtk_header_bar_new());
  gtk_widget_show(GTK_WIDGET(header_bar));
  gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(header_bar)),
                              "infinit-header");
  gtk_header_bar_set_title(header_bar, "Infinit Lab");
  gtk_header_bar_set_subtitle(header_bar, "Differential equations & calculus");
  gtk_header_bar_set_show_close_button(header_bar, FALSE);
  gtk_header_bar_set_decoration_layout(header_bar, "");

  GtkWidget* controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_valign(controls, GTK_ALIGN_CENTER);
  gtk_widget_show(controls);

  GtkButton* minimize = my_application_window_button(
      INFINIT_WINDOW_ICON_MINIMIZE, "Minimize", nullptr);
  GtkButton* maximize = my_application_window_button(
      INFINIT_WINDOW_ICON_MAXIMIZE, "Maximize", nullptr);
  GtkButton* close = my_application_window_button(
      INFINIT_WINDOW_ICON_CLOSE, "Close", "close");

  g_object_set_data(G_OBJECT(window), "infinit-maximize-image",
                    gtk_button_get_image(maximize));

  g_signal_connect(minimize, "clicked",
                   G_CALLBACK(my_application_minimize), window);
  g_signal_connect(maximize, "clicked",
                   G_CALLBACK(my_application_toggle_maximize), window);
  g_signal_connect(close, "clicked", G_CALLBACK(my_application_close), window);

  gtk_box_pack_start(GTK_BOX(controls), GTK_WIDGET(minimize), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(controls), GTK_WIDGET(maximize), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(controls), GTK_WIDGET(close), FALSE, FALSE, 0);
  gtk_widget_show(GTK_WIDGET(minimize));
  gtk_widget_show(GTK_WIDGET(maximize));
  gtk_widget_show(GTK_WIDGET(close));
  gtk_header_bar_pack_end(header_bar, controls);

  g_signal_connect(window, "window-state-event",
                   G_CALLBACK(my_application_window_state_changed), nullptr);
  return header_bar;
}

// Called when first Flutter frame received.
static void first_frame_cb(MyApplication* self, FlView* view) {
  gtk_widget_show(gtk_widget_get_toplevel(GTK_WIDGET(view)));
}

// Implements GApplication::activate.
static void my_application_activate(GApplication* application) {
  MyApplication* self = MY_APPLICATION(application);
  GtkWindow* window =
      GTK_WINDOW(gtk_application_window_new(GTK_APPLICATION(application)));

  // A custom header bar gives every desktop the same three modern window
  // controls (minimize, maximize/restore, close) instead of the toolkit or
  // window-manager defaults.  The bar also keeps the drag area and title.
  my_application_install_window_css();
  GtkHeaderBar* header_bar = my_application_build_header_bar(window);
  gtk_window_set_titlebar(window, GTK_WIDGET(header_bar));
  gtk_window_set_title(window, "Infinit Lab");

  gtk_window_set_default_size(window, 1280, 720);
  my_application_set_brand_icon(window);

  g_autoptr(FlDartProject) project = fl_dart_project_new();
  fl_dart_project_set_dart_entrypoint_arguments(
      project, self->dart_entrypoint_arguments);

  FlView* view = fl_view_new(project);
  GdkRGBA background_color;
  // Background defaults to black, override it here if necessary, e.g. #00000000
  // for transparent.
  gdk_rgba_parse(&background_color, "#000000");
  fl_view_set_background_color(view, &background_color);
  gtk_widget_show(GTK_WIDGET(view));
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));

  // Show the window when Flutter renders.
  // Requires the view to be realized so we can start rendering.
  g_signal_connect_swapped(view, "first-frame", G_CALLBACK(first_frame_cb),
                           self);
  gtk_widget_realize(GTK_WIDGET(view));

  fl_register_plugins(FL_PLUGIN_REGISTRY(view));

  gtk_widget_grab_focus(GTK_WIDGET(view));
}

// Implements GApplication::local_command_line.
static gboolean my_application_local_command_line(GApplication* application,
                                                  gchar*** arguments,
                                                  int* exit_status) {
  MyApplication* self = MY_APPLICATION(application);
  // Strip out the first argument as it is the binary name.
  self->dart_entrypoint_arguments = g_strdupv(*arguments + 1);

  g_autoptr(GError) error = nullptr;
  if (!g_application_register(application, nullptr, &error)) {
    g_warning("Failed to register: %s", error->message);
    *exit_status = 1;
    return TRUE;
  }

  g_application_activate(application);
  *exit_status = 0;

  return TRUE;
}

// Implements GApplication::startup.
static void my_application_startup(GApplication* application) {
  // MyApplication* self = MY_APPLICATION(object);

  // Perform any actions required at application startup.

  G_APPLICATION_CLASS(my_application_parent_class)->startup(application);
}

// Implements GApplication::shutdown.
static void my_application_shutdown(GApplication* application) {
  // MyApplication* self = MY_APPLICATION(object);

  // Perform any actions required at application shutdown.

  G_APPLICATION_CLASS(my_application_parent_class)->shutdown(application);
}

// Implements GObject::dispose.
static void my_application_dispose(GObject* object) {
  MyApplication* self = MY_APPLICATION(object);
  g_clear_pointer(&self->dart_entrypoint_arguments, g_strfreev);
  G_OBJECT_CLASS(my_application_parent_class)->dispose(object);
}

static void my_application_class_init(MyApplicationClass* klass) {
  G_APPLICATION_CLASS(klass)->activate = my_application_activate;
  G_APPLICATION_CLASS(klass)->local_command_line =
      my_application_local_command_line;
  G_APPLICATION_CLASS(klass)->startup = my_application_startup;
  G_APPLICATION_CLASS(klass)->shutdown = my_application_shutdown;
  G_OBJECT_CLASS(klass)->dispose = my_application_dispose;
}

static void my_application_init(MyApplication* self) {}

MyApplication* my_application_new() {
  // Set the program name to the application ID, which helps various systems
  // like GTK and desktop environments map this running application to its
  // corresponding .desktop file. This ensures better integration by allowing
  // the application to be recognized beyond its binary name.
  g_set_prgname(APPLICATION_ID);

  return MY_APPLICATION(g_object_new(my_application_get_type(),
                                     "application-id", APPLICATION_ID, "flags",
                                     G_APPLICATION_NON_UNIQUE, nullptr));
}
