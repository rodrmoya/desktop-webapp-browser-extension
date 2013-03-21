/*
 * This file is part of the desktop-webapp-browser-extension.
 * Copyright (C) Canonical Ltd. 2012
 * Copyright (C) Collabora Ltd. 2013
 *
 * Author:
 *   Rodrigo Moya <rodrigo.moya@collabora.co.uk>
 *
 * Based on webaccounts-browser-plugin by:
 *   Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include "webapp-monitor.h"

typedef struct {
  GObject object;

  GFileMonitor *file_monitor;
  NPP instance;
  NPObject *icon_loader_callback;
} WebappMonitor;

typedef struct {
  GObjectClass parent_class;
} WebappMonitorClass;

G_DEFINE_TYPE(WebappMonitor, webapp_monitor, G_TYPE_OBJECT)

static WebappMonitor *the_monitor = NULL;

static void
webapp_monitor_finalize (GObject *object)
{
  WebappMonitor *monitor = (WebappMonitor *) object;

  if (monitor->file_monitor != NULL) {
    g_object_unref (monitor->file_monitor);
    monitor->file_monitor = NULL;
  }

  if (monitor->icon_loader_callback != NULL) {
    NPN_ReleaseObject (monitor->icon_loader_callback);
    monitor->icon_loader_callback = NULL;
  }

  G_OBJECT_CLASS (webapp_monitor_parent_class)->finalize (object);
}

static void
webapp_monitor_class_init (WebappMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = webapp_monitor_finalize;
}

static void
retrieve_highres_icon (WebappMonitor *monitor, const gchar *desktop_file)
{
  GKeyFile *key_file;
  gchar *s;
  GError *error = NULL;

  /* Get URL to get icon for */
  key_file = g_key_file_new ();
  if (!g_key_file_load_from_data (key_file, desktop_file, strlen (desktop_file), 0, &error)) {
    g_warning ("Could not parse desktop file: %s", error->message);
    g_error_free (error);

    goto out;
  }

  s =  g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
  if (s != NULL) {
    gint n_args, i;
    gchar **args, *url = NULL;

    g_debug ("Parsing command line %s", s);

    if (!g_shell_parse_argv (s, &n_args, &args, &error)) {
      g_debug ("Failed parsing command line %s", s);
      goto out;
    }

    for (i = 0; i < n_args && args[i] != NULL; i++) {
      g_debug ("Processing argument %s", args[i]);
      if (g_str_has_prefix (args[i], "--app=")) {
	url = g_strdup (args[i] + 6);
	g_debug ("Found URL %s", url);
	break;
      }
    }

    if (url != NULL) {
      NPVariant url_varg, result;

      STRINGZ_TO_NPVARIANT(url, url_varg);
      NULL_TO_NPVARIANT(result);

      if (!NPN_InvokeDefault (monitor->instance, monitor->icon_loader_callback, &url_varg, 1, &result))
      	g_debug ("Failed calling JS callback");

      g_free (url);
      NPN_ReleaseVariantValue (&result);
    }

    g_strfreev (args);
    g_free (s);
  }

 out:
  g_key_file_free (key_file);
}

static void
on_directory_changed (GFileMonitor     *file_monitor,
		      GFile            *file,
		      GFile            *other_file,
		      GFileMonitorEvent event_type,
		      gpointer          user_data)
{
  WebappMonitor *monitor = (WebappMonitor *) user_data;

  g_debug ("%s called", G_STRFUNC);

  if (event_type == G_FILE_MONITOR_EVENT_CREATED) {
     GError *error = NULL;
     gchar *contents;
     gsize len;
     const gchar *file_path = g_file_get_path (file);

     if (!g_str_has_prefix (g_basename (file_path), "chrome-"))
       return;

     if (g_file_get_contents (file_path, &contents, &len, &error)) {
       gchar *tmp = contents;
       const gchar *shebang = "#!/usr/bin/env xdg-open";

       g_debug ("Old contents = %s\n", contents);

       /* Read 1st line */
       if (g_str_has_prefix (contents, shebang)) {
	 tmp += strlen (shebang);
	 if (*tmp == '[') {
	   GString *new_contents = g_string_new (shebang);

	   new_contents = g_string_append (new_contents, "\n");
	   new_contents = g_string_append (new_contents, tmp);

	   if (!g_file_set_contents (file_path, new_contents->str, new_contents->len, &error)) {
	     g_warning ("Could not write %s file: %s", file_path, error->message);
	     g_error_free (error);
	   } else {
	     g_debug ("New contents: %s\n", new_contents->str);
	     retrieve_highres_icon (monitor, new_contents->str);
	   }

	   g_string_free (new_contents, TRUE);
	 }
       } else
	 retrieve_highres_icon (monitor, contents);

       g_free (contents);

     } else {
       g_warning ("Could not read %s file: %s", file_path, error->message);
       g_error_free (error);
     }
  }
}

static void
webapp_monitor_init (WebappMonitor *monitor)
{
  gchar *path = g_strdup_printf ("%s/.local/share/applications", g_get_home_dir ());
  GFile *file = g_file_new_for_path (path);
  GError *error = NULL;

  monitor->instance = NULL;
  monitor->icon_loader_callback = NULL;

  monitor->file_monitor = g_file_monitor_directory (file, 0, NULL, &error);
  if (monitor->file_monitor) {
    GDir *dir;

    /* Check already existing files on startup */
    dir = g_dir_open (path, 0, &error);
    if (dir) {
      const gchar *name;

      while (name = g_dir_read_name (dir)) {
	if (g_str_has_prefix (name, "chrome-")) {
	  gchar *full_path = g_strdup_printf ("%s/%s", path, name);
	  GFile *file_to_check = g_file_new_for_path (full_path);

	  on_directory_changed (monitor->file_monitor, file_to_check, NULL, G_FILE_MONITOR_EVENT_CREATED, monitor);

	  g_free (full_path);
	  g_object_unref (file_to_check);
	}
      }

      g_dir_close (dir);
    } else {
      g_error ("Error opening directory %s: %s\n", path, error->message);
      g_error_free (error);
    }

    /* Listen to changes in the ~/.local/share/applications directory */
    g_signal_connect (monitor->file_monitor, "changed", G_CALLBACK (on_directory_changed), monitor);
  } else {
    g_error ("Error monitoring directory: %s\n", error->message);
    g_error_free (error);
  }

  g_free (path);
  g_object_unref (file);
}

void
webapp_initialize_monitor (NPP instance)
{
  g_debug ("%s called", G_STRFUNC);

  if (the_monitor != NULL) {
    g_debug ("%s monitor already initialized", G_STRFUNC);
    return;
  }

  the_monitor = g_object_new (webapp_monitor_get_type (), NULL);
  the_monitor->instance = instance;
}

void
webapp_monitor_set_icon_loader_callback (NPObject *callback)
{
  g_debug ("%s called", G_STRFUNC);

  if (!the_monitor) {
    g_debug ("%s monitor not initialized", G_STRFUNC);
    return;
  }

  the_monitor->icon_loader_callback = NPN_RetainObject (callback);
}

void
webapp_destroy_monitor (void)
{
  g_debug ("%s called", G_STRFUNC);

  if (the_monitor != NULL) {
    g_clear_object (&the_monitor);
  }
}
