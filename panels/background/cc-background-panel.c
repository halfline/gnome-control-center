/*
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdesktop-enums.h>

#include "cc-background-panel.h"

#include "bg-wallpapers-source.h"
#include "cc-background-item.h"
#include "cc-background-grid-item.h"
#include "cc-background-resources.h"
#include "cc-background-xml.h"

#include "bg-pictures-source.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_URI_KEY "picture-uri"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

struct _CcBackgroundPanel
{
  CcPanel parent_instance;

  GtkBuilder *builder;
  GDBusConnection *connection;
  GSettings *settings;

  GnomeDesktopThumbnailFactory *thumb_factory;

  CcBackgroundItem *current_background;

  BgWallpapersSource *wallpapers_source;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;

  GdkPixbuf *display_screenshot;
  char *screenshot_path;
};

CC_PANEL_REGISTER (CcBackgroundPanel, cc_background_panel)

#define WID(y) (GtkWidget *) gtk_builder_get_object (panel->builder, y)

static const char *
cc_background_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/look-background";
}

static void
cc_background_panel_dispose (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->builder);

  /* destroying the builder object will also destroy the spinner */
  panel->spinner = NULL;

  if (panel->copy_cancellable)
    {
      /* cancel any copy operation */
      g_cancellable_cancel (panel->copy_cancellable);

      g_clear_object (&panel->copy_cancellable);
    }

  if (panel->capture_cancellable)
    {
      /* cancel screenshot operations */
      g_cancellable_cancel (panel->capture_cancellable);

      g_clear_object (&panel->capture_cancellable);
    }

  g_clear_object (&panel->wallpapers_source);
  g_clear_object (&panel->thumb_factory);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->current_background);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_background_panel_get_help_uri;

  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;
}

static CcBackgroundItem *
get_current_background (CcBackgroundPanel *panel, GSettings *settings)
{
  if (settings == panel->settings)
    return panel->current_background;
  else
    return panel->current_lock_background;
}

static void
update_preview (CcBackgroundPanel *panel,
                GSettings *settings,
                CcBackgroundItem  *item)
{
  gboolean changes_with_time;
  CcBackgroundItem *current_background;

  current_background = panel->current_background;

  if (item && current_background)
    {
      g_object_unref (current_background);
      current_background = cc_background_item_copy (item);
      panel->current_background = current_background;
      cc_background_item_load (current_background, NULL);
    }

  changes_with_time = FALSE;

  if (current_background)
    {
      changes_with_time = cc_background_item_changes_with_time (current_background);
    }

  gtk_widget_set_visible (WID ("slide_image"), changes_with_time);
  gtk_widget_set_visible (WID ("slide-label"), changes_with_time);

  gtk_widget_queue_draw (WID ("background-desktop-drawingarea"));
}

static gchar *
get_save_path (CcBackgroundPanel *panel, GSettings *settings)
{
  return g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           settings == panel->settings ? "last-edited.xml" : "last-edited-lock.xml",
                           NULL);
}

static GdkPixbuf*
get_or_create_cached_pixbuf (CcBackgroundPanel *panel,
                             GtkWidget         *widget,
                             CcBackgroundItem  *background)
{
  GtkAllocation allocation;
  const gint preview_width = 309;
  const gint preview_height = 168;
  gint scale_factor;
  GdkPixbuf *pixbuf;
  GdkPixbuf *pixbuf_tmp;

  pixbuf = g_object_get_data (G_OBJECT (background), "pixbuf");
  if (pixbuf == NULL)
    {
      gtk_widget_get_allocation (widget, &allocation);
      scale_factor = gtk_widget_get_scale_factor (widget);
      pixbuf = cc_background_item_get_frame_thumbnail (background,
                                                       panel->thumb_factory,
                                                       preview_width,
                                                       preview_height,
                                                       scale_factor,
                                                       -2, TRUE);

      if (background == panel->current_background &&
          panel->display_screenshot != NULL)
        {
          /* we need to add an alpha channel for for copy aera */ 
          pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0,0,0);

          pixbuf_tmp = gdk_pixbuf_scale_simple (panel->display_screenshot,
                                                preview_width,
                                                (preview_width
                                                 * gdk_pixbuf_get_height (panel->display_screenshot) 
                                                 / gdk_pixbuf_get_width(panel->display_screenshot)),
                                                GDK_INTERP_BILINEAR);

          gdk_pixbuf_copy_area (pixbuf_tmp,
                                0,
                                0,
                                preview_width,
                                gdk_pixbuf_get_height(pixbuf_tmp),
                                pixbuf,
                                0,
                                0);

          g_object_unref (pixbuf_tmp);
        }

      g_object_set_data_full (G_OBJECT (background), "pixbuf", pixbuf, g_object_unref);
    }

  return pixbuf;
}

static void
update_display_preview (CcBackgroundPanel *panel,
                        GtkWidget         *widget,
                        CcBackgroundItem  *background)
{
  GdkPixbuf *pixbuf;
  cairo_t *cr;

  pixbuf = get_or_create_cached_pixbuf (panel, widget, background);

  cr = gdk_cairo_create (gtk_widget_get_window (widget));
  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}

typedef struct {
  CcBackgroundPanel *panel;
  GdkRectangle capture_rect;
  GdkRectangle monitor_rect;
  GdkRectangle workarea_rect;
  gboolean whole_monitor;
} ScreenshotData;

static void
on_screenshot_finished (GObject *source,
                        GAsyncResult *res,
                        gpointer user_data)
{
  ScreenshotData *data = user_data;
  CcBackgroundPanel *panel = data->panel;
  GError *error;
  GdkPixbuf *pixbuf;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                          res,
                                          &error);

  if (result == NULL) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      g_error_free (error);
      g_free (data);
      return;
    }
    g_debug ("Unable to get screenshot: %s",
             error->message);
    g_error_free (error);
    /* fallback? */
    goto out;
  }
  g_variant_unref (result);

  pixbuf = gdk_pixbuf_new_from_file (panel->screenshot_path, &error);
  if (pixbuf == NULL)
    {
      g_debug ("Unable to use GNOME Shell's builtin screenshot interface: %s",
               error->message);
      g_error_free (error);
      goto out;
    }

  g_clear_object (&panel->display_screenshot);

  if (data->whole_monitor) {
    /* copy only top panel area from pixbuf */
    gdk_pixbuf_copy_area (pixbuf,
                          0,
                          0,
                          data->monitor_rect.width,
                          data->monitor_rect.height - data->workarea_rect.height,
                          panel->display_screenshot,
                          0,
                          0);
    g_object_unref (pixbuf);

  }
  else {
    panel->display_screenshot = pixbuf;
  }

  /* invalidate existing cached pixbuf */
  g_object_set_data (G_OBJECT (panel->current_background), "pixbuf", NULL);

  /* remove the temporary file created by the shell */
  g_unlink (panel->screenshot_path);
  g_clear_pointer (&panel->screenshot_path, g_free);

out:
  update_display_preview (panel, WID ("background-desktop-drawingarea"), panel->current_background);
  g_free (data);
}

static gboolean
calculate_contiguous_workarea (ScreenshotData *data)
{
  /* Optimise for the shell panel being the only non-workarea
   * object at the top of the screen */
  if (data->workarea_rect.x != data->monitor_rect.x)
    return FALSE;
  if ((data->workarea_rect.y + data->workarea_rect.height) != (data->monitor_rect.y + data->monitor_rect.height))
    return FALSE;

  data->capture_rect.x = data->monitor_rect.x;
  data->capture_rect.width = data->monitor_rect.width;
  data->capture_rect.y = data->monitor_rect.y;
  data->capture_rect.height = data->monitor_rect.height - data->workarea_rect.height;

  return TRUE;
}

static void
get_screenshot_async (CcBackgroundPanel *panel)
{
  gchar *path, *tmpname;
  const gchar *method_name;
  GVariant *method_params;
  GtkWidget *widget;
  ScreenshotData *data;
  int primary;

  data = g_new0 (ScreenshotData, 1);
  data->panel = panel;

  widget = WID ("background-desktop-drawingarea");
  primary = gdk_screen_get_primary_monitor (gtk_widget_get_screen (widget));
  gdk_screen_get_monitor_geometry (gtk_widget_get_screen (widget), primary, &data->monitor_rect);
  gdk_screen_get_monitor_workarea (gtk_widget_get_screen (widget), primary, &data->workarea_rect);
  if (calculate_contiguous_workarea (data)) {
    g_debug ("Capturing only a portion of the screen");
  } else {
    g_debug ("Capturing the whole monitor");
    data->whole_monitor = TRUE;
    data->capture_rect = data->monitor_rect;
  }

  g_debug ("Trying to capture rectangle %dx%d (at %d,%d)",
           data->capture_rect.width, data->capture_rect.height, data->capture_rect.x, data->capture_rect.y);

  path = g_build_filename (g_get_user_cache_dir (), "gnome-control-center", NULL);
  g_mkdir_with_parents (path, USER_DIR_MODE);

  tmpname = g_strdup_printf ("scr-%d.png", g_random_int ());
  g_free (panel->screenshot_path);
  panel->screenshot_path = g_build_filename (path, tmpname, NULL);
  g_print (panel->screenshot_path);
  g_free (path);
  g_free (tmpname);

  method_name = "ScreenshotArea";
  method_params = g_variant_new ("(iiiibs)",
                                 data->capture_rect.x, data->capture_rect.y,
                                 data->capture_rect.width, data->capture_rect.height,
                                 FALSE, /* flash */
                                 panel->screenshot_path);

  g_dbus_connection_call (panel->connection,
                          "org.gnome.Shell.Screenshot",
                          "/org/gnome/Shell/Screenshot",
                          "org.gnome.Shell.Screenshot",
                          method_name,
                          method_params,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          panel->capture_cancellable,
                          on_screenshot_finished,
                          data);
}

static gboolean
on_preview_draw (GtkWidget         *widget,
                 cairo_t           *cr,
                 CcBackgroundPanel *panel)
{
  /* we have another shot in flight or an existing cache */
  if (panel->display_screenshot == NULL
      && panel->screenshot_path == NULL)
    {
      //get_screenshot_async (panel);
    }
  else
    update_display_preview (panel, widget, panel->current_background);

  return TRUE;
}

static void
reload_current_bg (CcBackgroundPanel *panel,
                   GSettings *settings
                  )
{
  g_autoptr(CcBackgroundItem) saved = NULL;
  CcBackgroundItem *configured;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *pcolor = NULL;
  g_autofree gchar *scolor = NULL;

  /* Load the saved configuration */
  uri = get_save_path ("last-edited.xml");
  saved = cc_background_xml_get_item (uri);

  /* initalise the current background information from settings */
  uri = g_settings_get_string (settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    {
      g_clear_pointer (&uri, g_free);
    }
  else
    {
      g_autoptr(GFile) file = NULL;
      file = g_file_new_for_commandline_arg (uri);
    }
  configured = cc_background_item_new (uri);

  pcolor = g_settings_get_string (settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
                "name", _("Current background"),
                "placement", g_settings_get_enum (settings, WP_OPTIONS_KEY),
                "shading", g_settings_get_enum (settings, WP_SHADING_KEY),
                "primary-color", pcolor,
                "secondary-color", scolor,
                NULL);

  if (saved != NULL && cc_background_item_compare (saved, configured))
    {
      CcBackgroundItemFlags flags;
      flags = cc_background_item_get_flags (saved);
      /* Special case for colours */
      if (cc_background_item_get_placement (saved) == G_DESKTOP_BACKGROUND_STYLE_NONE)
        flags &=~ (CC_BACKGROUND_ITEM_HAS_PCOLOR | CC_BACKGROUND_ITEM_HAS_SCOLOR);
      g_object_set (G_OBJECT (configured),
                    "name", cc_background_item_get_name (saved),
                    "flags", flags,
                    "source-url", cc_background_item_get_source_url (saved),
                    "source-xml", cc_background_item_get_source_xml (saved),
                    NULL);
    }

  g_clear_object (&panel->current_background);
  panel->current_background = configured;

  cc_background_item_load (configured, NULL);
}

static gboolean
create_save_dir (void)
{
  g_autofree char *path = NULL;

  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           NULL);
  if (g_mkdir_with_parents (path, USER_DIR_MODE) < 0)
    {
      g_warning ("Failed to create directory '%s'", path);
      return FALSE;
    }
  return TRUE;
}

static void
copy_finished_cb (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      pointer)
{
  g_autoptr(GError) err = NULL;
  g_autoptr(CcBackgroundPanel) panel = (CcBackgroundPanel *) pointer;
  CcBackgroundItem *item;
  CcBackgroundItem *current_background;
  GSettings *settings;

  if (!g_file_copy_finish (G_FILE (source_object), result, &err))
    {
      if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        return;
      }
      g_warning ("Failed to copy image to cache location: %s", err->message);
    }
  item = g_object_get_data (source_object, "item");
  settings = g_object_get_data (source_object, "settings");
  current_background = panel->current_background;

  g_settings_apply (settings);

  /* the panel may have been destroyed before the callback is run, so be sure
   * to check the widgets are not NULL */

  if (panel->spinner)
    {
      gtk_widget_destroy (GTK_WIDGET (panel->spinner));
      panel->spinner = NULL;
    }

  if (current_background)
    cc_background_item_load (current_background, NULL);

  if (panel->builder)
    {
      g_autofree gchar *filename = NULL;

      update_preview (panel, settings, item);
      current_background = panel->current_background;

      /* Save the source XML if there is one */
      filename = get_save_path ("last-edited.xml");
      if (create_save_dir ())
        cc_background_xml_save (current_background, filename);
    }
}

static void
set_background (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item)
{
  GDesktopBackgroundStyle style;
  gboolean save_settings = TRUE;
  const char *uri;
  CcBackgroundItemFlags flags;

  if (item == NULL)
    return;

  uri = cc_background_item_get_uri (item);
  flags = cc_background_item_get_flags (item);

  if ((flags & CC_BACKGROUND_ITEM_HAS_URI) && uri == NULL)
    {
      g_settings_set_enum (settings, WP_OPTIONS_KEY, G_DESKTOP_BACKGROUND_STYLE_NONE);
      g_settings_set_string (settings, WP_URI_KEY, "");
    }
  else if (cc_background_item_get_source_url (item) != NULL &&
           cc_background_item_get_needs_download (item))
    {
      g_autoptr(GFile) source = NULL;
      g_autoptr(GFile) dest = NULL;
      g_autofree gchar *cache_path = NULL;
      g_autofree gchar *basename = NULL;
      g_autofree gchar *display_name = NULL;
      g_autofree gchar *dest_path = NULL;
      g_autofree gchar *dest_uri = NULL;
      g_autoptr(GdkPixbuf) pixbuf = NULL;

      cache_path = bg_pictures_source_get_cache_path ();
      if (g_mkdir_with_parents (cache_path, USER_DIR_MODE) < 0)
        {
          g_warning ("Failed to create directory '%s'", cache_path);
          return;
        }

      dest_path = bg_pictures_source_get_unique_path (cc_background_item_get_source_url (item));
      dest = g_file_new_for_path (dest_path);
      source = g_file_new_for_uri (cc_background_item_get_source_url (item));
      basename = g_file_get_basename (source);
      display_name = g_filename_display_name (basename);
      dest_path = g_file_get_path (dest);

      /* create a blank image to use until the source image is ready */
      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
      gdk_pixbuf_fill (pixbuf, 0x00000000);
      gdk_pixbuf_save (pixbuf, dest_path, "png", NULL, NULL);

      if (panel->copy_cancellable)
        {
          g_cancellable_cancel (panel->copy_cancellable);
          g_cancellable_reset (panel->copy_cancellable);
        }

      if (panel->spinner)
        {
          gtk_widget_destroy (GTK_WIDGET (panel->spinner));
          panel->spinner = NULL;
        }

      /* create a spinner while the file downloads */
      panel->spinner = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (panel->spinner));
      gtk_box_pack_start (GTK_BOX (WID ("bottom-hbox")), panel->spinner, FALSE,
                          FALSE, 6);
      gtk_widget_show (panel->spinner);

      /* reference the panel in case it is removed before the copy is
       * finished */
      g_object_ref (panel);
      g_object_set_data_full (G_OBJECT (source), "item", g_object_ref (item), g_object_unref);
      g_object_set_data (G_OBJECT (source), "settings", settings);
      g_file_copy_async (source, dest, G_FILE_COPY_OVERWRITE,
                         G_PRIORITY_DEFAULT, panel->copy_cancellable,
                         NULL, NULL,
                         copy_finished_cb, panel);
      dest_uri = g_file_get_uri (dest);

      g_settings_set_string (settings, WP_URI_KEY, dest_uri);
      g_object_set (G_OBJECT (item),
                    "uri", dest_uri,
                    "needs-download", FALSE,
                    "name", display_name,
                    NULL);

      /* delay the updated drawing of the preview until the copy finishes */
      save_settings = FALSE;
    }
  else
    {
      g_settings_set_string (settings, WP_URI_KEY, uri);
    }

  /* Also set the placement if we have a URI and the previous value was none */
  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT)
    {
      g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }
  else if (uri != NULL)
    {
      style = g_settings_get_enum (settings, WP_OPTIONS_KEY);
      if (style == G_DESKTOP_BACKGROUND_STYLE_NONE)
        g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_SHADING)
    g_settings_set_enum (settings, WP_SHADING_KEY, cc_background_item_get_shading (item));

  g_settings_set_string (settings, WP_PCOLOR_KEY, cc_background_item_get_pcolor (item));
  g_settings_set_string (settings, WP_SCOLOR_KEY, cc_background_item_get_scolor (item));

  /* update the preview information */
  if (save_settings != FALSE)
    {
      g_autofree gchar *filename = NULL;

      /* Apply all changes */
      g_settings_apply (settings);

      /* Save the source XML if there is one */
      filename = get_save_path ("last-edited.xml");
      if (create_save_dir ())
        cc_background_xml_save (panel->current_background, filename);
    }
}

static void
on_settings_changed (GSettings         *settings,
                     gchar             *key,
                     CcBackgroundPanel *self)
{
  reload_current_bg (self, settings);
  update_preview (self, settings, NULL);
}

static GtkWidget *
create_view (GtkWidget *parent, GtkTreeModel *model)
{
  GtkCellRenderer *renderer;
  GtkWidget *icon_view;
  GtkWidget *sw;

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);

  icon_view = gtk_icon_view_new ();
  gtk_icon_view_set_model (GTK_ICON_VIEW (icon_view), model);
  gtk_widget_set_hexpand (icon_view, TRUE);
  gtk_container_add (GTK_CONTAINER (sw), icon_view);

  gtk_icon_view_set_columns (GTK_ICON_VIEW (icon_view), 3);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_view),
                              renderer,
                              FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
                                  renderer,
                                  "surface", 0,
                                  NULL);

  return sw;
}

static void
on_background_select (GtkFlowBox      *box,
                      GtkFlowBoxChild *child,
                      gpointer         user_data)
{
  CcBackgroundGridItem *selected = (CcBackgroundGridItem *) child;
  CcBackgroundPanel *panel = user_data;
  CcBackgroundItem *item;
  item = cc_background_grid_item_get_ref (selected);

  set_background (panel, panel->settings, item);
}

gboolean
do_foreach_background_item (GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         gpointer data)
{
  CcBackgroundPanel *panel = data;
  CcBackgroundGridItem *flow;
  GtkWidget *widget;
  GdkPixbuf *pixbuf;
  CcBackgroundItem *item;
  gint scale_factor;
  const gint preview_width = 309;
  const gint preview_height = 168;

  gtk_tree_model_get (model, iter, 1, &item, -1);

  scale_factor = gtk_widget_get_scale_factor (panel);

  pixbuf = cc_background_item_get_frame_thumbnail (item,
                                                   panel->thumb_factory,
                                                   preview_width,
                                                   preview_height,
                                                   scale_factor,
                                                   -2, TRUE);

  widget = gtk_image_new_from_pixbuf (pixbuf);

  flow = cc_background_grid_item_new(item);
  cc_background_grid_item_set_ref (flow, item);
  gtk_widget_show (flow);
  gtk_widget_show (widget);
  gtk_container_add (flow, widget);

  gtk_flow_box_insert (GTK_FLOW_BOX (WID("background-gallery")), flow, -1);
  return TRUE;
}

static void
on_source_added_cb (GtkTreeModel *model,
                    GtkTreePath  *path,
                    GtkTreeIter  *iter,
                    gpointer     user_data)
{
  //gtk_tree_model_foreach (model, foreach_background_item, user_data);
  do_foreach_background_item (model, path, iter, user_data);
}

static void
load_wallpapers (CcBackgroundPanel *panel, GtkWidget *parent)
{
  GtkListStore *model;
  GtkTreeIter iter;
  GtkTreePath  *path;
  GValue *value = NULL;
  gint scale_factor;

  scale_factor = gtk_widget_get_scale_factor (panel);

  panel->wallpapers_source = bg_wallpapers_source_new (GTK_WINDOW (NULL));
  model = bg_source_get_liststore (BG_SOURCE (panel->wallpapers_source));

  gtk_tree_model_foreach (model, do_foreach_background_item, panel);

  g_signal_connect (model, "row-inserted", G_CALLBACK (on_source_added_cb), panel);
  //g_signal_connect (model, "row-deleted", G_CALLBACK (on_source_removed_cb), chooser);
  //g_signal_connect (model, "row-changed", G_CALLBACK (on_source_modified_cb), chooser);
}

static void
cc_background_panel_init (CcBackgroundPanel *panel)
{
  gchar *objects[] = {"background-panel", NULL };
  g_autoptr(GError) err = NULL;
  GtkCssProvider *provider;
  GtkStyleProvider *provider;
  GtkStyleContext *context;
  GtkWidget *widget;

  panel->connection = g_application_get_dbus_connection (g_application_get_default ());
  g_resources_register (cc_background_get_resource ());

  panel->builder = gtk_builder_new ();
  gtk_builder_add_objects_from_resource (panel->builder,
                                         "/org/gnome/control-center/background/background.ui",
                                         objects, &err);

  if (err)
    {
      g_warning ("Could not load ui: %s", err->message);
      return;
    }

  panel->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (panel->settings);

  /* add the top level widget */
  widget = WID ("background-panel");

  gtk_container_add (GTK_CONTAINER (panel), widget);
  gtk_widget_show_all (GTK_WIDGET (panel));

  /* add style */
  widget = WID ("background-preview-top");
  provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
  gtk_css_provider_load_from_resource (provider,
                                       "org/gnome/control-center/background/background.css");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  /* setup preview area */
  widget = WID ("background-desktop-drawingarea");
  g_signal_connect (widget, "draw", G_CALLBACK (on_preview_draw), panel);

  panel->copy_cancellable = g_cancellable_new ();

  panel->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

  /* add the gallery widget */
  widget = WID ("background-gallery");

  g_signal_connect (G_OBJECT (widget), "child-activated",
                    G_CALLBACK (on_background_select), panel);

  load_wallpapers (panel, widget);

  /* Load the backgrounds */
  reload_current_bg (panel, panel->settings);
  update_preview (panel, panel->settings, NULL);

  /* Background settings */
  g_signal_connect (panel->settings, "changed", G_CALLBACK (on_settings_changed), panel);
}
