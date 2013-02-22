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

#include "config.h"

#include <string.h>
#include "object.h"
#include <glib.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct {
  NPObject object;
  GHashTable *methods;
} WebappObjectWrapper;

typedef NPVariant (*WebappMethod) (NPObject *object,
				   const NPVariant *args,
				   uint32_t argc);

static gchar *variant_to_string(const NPVariant variant)
{
  return g_strndup (NPVARIANT_TO_STRING (variant).UTF8Characters,
		    NPVARIANT_TO_STRING (variant).UTF8Length);
}

static NPObject *
NPClass_Allocate (NPP instance, NPClass *klass)
{
  g_return_val_if_fail (instance != NULL, NULL);

  WebappObjectWrapper *wrapper = g_new0 (WebappObjectWrapper, 1);

  wrapper->methods = g_hash_table_new (g_str_hash, g_str_equal);

  return (NPObject *) wrapper;
}

static void
NPClass_Deallocate (NPObject *npobj)
{
  WebappObjectWrapper *wrapper = (WebappObjectWrapper *) npobj;

  g_return_if_fail (wrapper != NULL);

  g_free (wrapper);
}

static void
NPClass_Invalidate (NPObject *npobj)
{
}

static bool
NPClass_HasMethod (NPObject *npobj, NPIdentifier name)
{
  WebappObjectWrapper *wrapper = (WebappObjectWrapper *) npobj;
  gchar *method_name;
  gboolean has_method;

  g_return_val_if_fail (wrapper != NULL, false);

  method_name = NPN_UTF8FromIdentifier (name);
  has_method = (g_hash_table_lookup (wrapper->methods, method_name) != NULL);

  g_debug ("%s(\"%s\")", G_STRFUNC, method_name);

  NPN_MemFree (method_name);
  return has_method;
}

static bool
NPClass_Invoke (NPObject *npobj,
		NPIdentifier name,
		const NPVariant *args,
		uint32_t argc,
		NPVariant *result)
{
  WebappObjectWrapper *wrapper = (WebappObjectWrapper *) npobj;
  gchar *method_name;
  WebappMethod method;

  g_return_val_if_fail (wrapper != NULL, false);

  method_name = NPN_UTF8FromIdentifier (name);
  method = g_hash_table_lookup (wrapper->methods, method_name);
  NPN_MemFree (method_name);

  if (G_UNLIKELY (method == NULL))
    return false;

  *result = method (npobj, args, argc);
  return true;
}

static bool
NPClass_InvokeDefault (NPObject *npobj,
		       const NPVariant *args,
		       uint32_t argc,
                       NPVariant *result)
{
  return false;
}

static bool
NPClass_HasProperty (NPObject *npobj, NPIdentifier name)
{
  return false;
}

static bool
NPClass_GetProperty (NPObject *npobj, NPIdentifier name, NPVariant *result)
{
    return false;
}


static bool
NPClass_SetProperty (NPObject *npobj, NPIdentifier name, const NPVariant *value)
{
    return false;
}


static bool
NPClass_RemoveProperty (NPObject *npobj, NPIdentifier name)
{
    return false;
}


static bool
NPClass_Enumerate (NPObject *npobj, NPIdentifier **identifier, uint32_t *count)
{
    return false;
}


static bool
NPClass_Construct (NPObject *npobj, const NPVariant *args, uint32_t argc,
                   NPVariant *result)
{
    return false;
}

static NPClass js_object_class = {
  .structVersion = NP_CLASS_STRUCT_VERSION,
  .allocate = NPClass_Allocate,
  .deallocate = NPClass_Deallocate,
  .invalidate = NPClass_Invalidate,
  .hasMethod = NPClass_HasMethod,
  .invoke = NPClass_Invoke,
  .invokeDefault = NPClass_InvokeDefault,
  .hasProperty = NPClass_HasProperty,
  .getProperty = NPClass_GetProperty,
  .setProperty = NPClass_SetProperty,
  .removeProperty = NPClass_RemoveProperty,
  .enumerate = NPClass_Enumerate,
  .construct = NPClass_Construct
};

static gboolean
save_extension_icon (gchar *icon_data, const gchar *destination)
{
  GdkPixbuf *pixbuf;
  GInputStream *stream;
  gsize len;
  GError *error = NULL;
  gboolean result = FALSE;

  g_base64_decode_inplace (icon_data, &len);
  stream = g_memory_input_stream_new_from_data (icon_data, len, NULL);
  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);
  if (error != NULL) {
    g_debug ("%s error: %s", G_STRFUNC, error->message);
    g_error_free (error);

    return FALSE;
  }

  if (gdk_pixbuf_save (pixbuf, destination, "png", &error, NULL)) {
    result = TRUE;
  } else {
    g_debug ("%s error: %s", G_STRFUNC, error->message);
    g_error_free (error);
  }

  g_object_unref (pixbuf);

  return result;
}

static NPVariant
install_chrome_app_wrapper (NPObject *object,
			    const NPVariant *args,
			    uint32_t argc)
{
  NPVariant result;
  gchar *app_id = NULL, *name = NULL, *description = NULL, *command = NULL, *icon = NULL;
  gchar *desktop_file_path = NULL, *desktop_file = NULL;

  NULL_TO_NPVARIANT (result);

  g_debug ("%s called", G_STRFUNC);

  if (G_UNLIKELY (argc < 5 &&
		  !NPVARIANT_IS_STRING (args[0]) &&
		  !NPVARIANT_IS_STRING (args[1]) &&
		  !NPVARIANT_IS_STRING (args[2]) &&
		  !NPVARIANT_IS_STRING (args[3]) &&
		  !NPVARIANT_IS_STRING (args[4]))) {
    g_debug ("%s() string expected for all arguments", G_STRFUNC);
    return result;
  }

  app_id = variant_to_string (args[0]);
  if (G_UNLIKELY (app_id == NULL)) {
    g_debug ("%s empty app id", G_STRFUNC);
    return result;
  }

  name = variant_to_string (args[1]);
  if (G_UNLIKELY (name == NULL)) {
    g_debug ("%s empty name", G_STRFUNC);
    goto out;
  }
  description = variant_to_string (args[2]);
  command = variant_to_string (args[3]);
  if (G_UNLIKELY (name == NULL)) {
    g_debug ("%s empty URL", G_STRFUNC);
    goto out;
  }

  /* Create .desktop file in ~/.local/share/applications */
  desktop_file = g_strdup_printf ("chrome-%s-Default.desktop", app_id);
  desktop_file_path = g_strdup_printf ("%s/.local/share/applications/%s", g_get_home_dir (), desktop_file);
  if (desktop_file_path != NULL) {
    GKeyFile *key_file = g_key_file_new ();
    gchar *exec, *contents;
    gsize size;
    GSettings *settings;
    gchar **favorite_apps;
    GPtrArray *apps_array;
    const gchar *categories[] = { "Network", "WebBrowser" };

    g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, name);
    g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME, name);
    if (description != NULL) {
      g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_COMMENT, description);
    }

    exec = g_strdup_printf ("chromium \"--app=%s\"", command);
    g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, exec);
    g_free (exec);

    g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TERMINAL, FALSE);
    g_key_file_set_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_CATEGORIES, categories, G_N_ELEMENTS (categories));
    g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TYPE, G_KEY_FILE_DESKTOP_TYPE_APPLICATION);
    g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, TRUE);
    g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			   G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS,
			   "chrome.google.com__webstore_category_home");

    /* Retrieve icon data and save it */
    icon = variant_to_string (args[4]);
    if (icon != NULL && g_str_has_prefix (icon, "data:image/png;base64,")) {
      gchar *icon_buffer, *icon_file_path, *icon_file;

      /* skip the 'data:image/png;base64,' mime prefix */
      icon_buffer = icon + 22;

      icon_file = g_strdup_printf ("chrome-%s", app_id);
      icon_file_path = g_strdup_printf ("%s/.local/share/icons/%s.png", g_get_home_dir (), icon_file);

      if (save_extension_icon (icon_buffer, icon_file_path))
	g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, icon_file);
      else {
	g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, "chromium-browser.png");
	g_debug ("%s failed saving %s file", G_STRFUNC, icon_file_path);
      }

      g_free (icon_file_path);
      g_free (icon_file);
    }

    g_free (icon);

    /* Save .desktop file */
    contents = g_key_file_to_data (key_file, &size, NULL);
    if (!g_file_set_contents (desktop_file_path, contents, size, NULL))
      g_debug ("%s failed saving %s file", G_STRFUNC, desktop_file_path);

    g_free (contents);
    g_key_file_free (key_file);
    g_free (desktop_file_path);

    /* Add newly-installed app to Shell's favorites */
    settings = g_settings_new ("org.gnome.shell");

    apps_array = g_ptr_array_new ();
    favorite_apps = g_settings_get_strv (settings, "favorite-apps");
    if (favorite_apps != NULL) {
      guint idx;

      for (idx = 0; favorite_apps[idx] != NULL; idx++) {
	g_ptr_array_add (apps_array, favorite_apps[idx]);
      }
    }

    g_ptr_array_add (apps_array, desktop_file);
    g_ptr_array_add (apps_array, NULL);

    g_settings_set_strv (settings, "favorite-apps", (const gchar *const *) apps_array->pdata);

    g_strfreev (favorite_apps);
    g_ptr_array_free (apps_array, TRUE);
    g_object_unref (settings);
  }

  g_free (desktop_file);

 out:
  g_free (app_id);
  g_free (name);
  g_free (description);
  g_free (command);

  return result;
}

static void
remove_file (const gchar *path)
{
  GFile *fh;

  fh = g_file_new_for_path (path);
  g_file_delete (fh, NULL, NULL);

  g_object_unref (fh);
}

static NPVariant
uninstall_chrome_app_wrapper (NPObject *object,
			      const NPVariant *args,
			      uint32_t argc)
{
  NPVariant result;
  gchar *app_id, *file_path, *desktop_file;

  NULL_TO_NPVARIANT (result);

  g_debug ("%s called", G_STRFUNC);

  if (G_UNLIKELY (argc < 1 || !NPVARIANT_IS_STRING (args[0]))) {
    g_debug ("%s() string expected for argument #1", G_STRFUNC);
    return result;
  }

  app_id = variant_to_string (args[0]);
  if (G_UNLIKELY (app_id == NULL)) {
    g_debug ("%s empty app id", G_STRFUNC);
    return result;
  }

  /* Remove the .desktop file in ~/.local/share/applications */
  desktop_file = g_strdup_printf ("chrome-%s-Default.desktop", app_id);
  file_path = g_strdup_printf ("%s/.local/share/applications/%s", g_get_home_dir (), desktop_file);
  if (file_path != NULL) {
    GSettings *settings;
    gchar **favorite_apps;
    GPtrArray *apps_array;

    remove_file (file_path);
    g_free (file_path);

    /* Remove app from Shell's favorites */
    settings = g_settings_new ("org.gnome.shell");

    apps_array = g_ptr_array_new ();
    favorite_apps = g_settings_get_strv (settings, "favorite-apps");
    if (favorite_apps != NULL) {
      gboolean changed = FALSE;
      guint idx;

      for (idx = 0; favorite_apps[idx] != NULL; idx++) {
	if (g_str_equal (desktop_file, favorite_apps[idx])) {
	  changed = TRUE;
	  continue;
	}

	g_ptr_array_add (apps_array, favorite_apps[idx]);
      }

      g_ptr_array_add (apps_array, NULL);
      if (changed) {
	g_settings_set_strv (settings, "favorite-apps", (const gchar *const *) apps_array->pdata);
      }
    }

    g_ptr_array_free (apps_array, TRUE);
    g_object_unref (settings);
  }

  /* Remove the icon file in ~/.local/share/icons */
  file_path = g_strdup_printf ("%s/.local/share/icons/chrome-%s.png", g_get_home_dir (), app_id);
  if (file_path) {
    remove_file (file_path);
    g_free (file_path);
  }

  g_free (desktop_file);
  g_free (app_id);

  return result;
}

static void
on_directory_changed (GFileMonitor     *monitor,
		      GFile            *file,
		      GFile            *other_file,
		      GFileMonitorEvent event_type,
		      gpointer          user_data)
{
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
       if (!strncmp (contents, shebang, strlen (shebang))) {
	 tmp += strlen (shebang);
	 if (*tmp == '[') {
	   GString *new_contents = g_string_new (shebang);

	   new_contents = g_string_append (new_contents, "\n");
	   new_contents = g_string_append (new_contents, tmp);

	   if (!g_file_set_contents (file_path, new_contents->str, new_contents->len, &error)) {
	     g_warning ("Could not write %s file: %s", file_path, error->message);
	     g_error_free (error);
	   } else
	     g_debug ("New contents: %s\n", new_contents->str);

	   g_string_free (new_contents, TRUE);
	 }
       }
       g_free (contents);

     } else {
       g_warning ("Could not read %s file: %s", file_path, error->message);
       g_error_free (error);
     }
  }
}

NPObject *
webapp_create_plugin_object (NPP instance)
{
  NPObject *object = NPN_CreateObject (instance, &js_object_class);
  g_return_val_if_fail (object != NULL, NULL);

  g_debug ("%s()", G_STRFUNC);

  g_type_init ();

  WebappObjectWrapper *wrapper = (WebappObjectWrapper *) object;

  /* Public methods */
  g_hash_table_insert (wrapper->methods,
		       (gchar *) "installChromeApp",
		       install_chrome_app_wrapper);
  g_hash_table_insert (wrapper->methods,
		       (gchar *) "uninstallChromeApp",
		       uninstall_chrome_app_wrapper);

  return object;
}

void
webapp_initialize_monitor (void)
{
  GFileMonitor *monitor;
  gchar *path = g_strdup_printf ("%s/.local/share/applications", g_get_home_dir ());
  GFile *file = g_file_new_for_path (path);

  GError *error = NULL;
  monitor = g_file_monitor_directory (file, 0, NULL, &error);
  if (monitor) {
    GDir *dir;

    /* Check already existing files on startup */
    dir = g_dir_open (path, 0, &error);
    if (dir) {
      const gchar *name;

      while (name = g_dir_read_name (dir)) {
	if (g_str_has_prefix (name, "chrome-")) {
	  gchar *full_path = g_strdup_printf ("%s/%s", path, name);
	  GFile *file_to_check = g_file_new_for_path (full_path);

	  on_directory_changed (monitor, file_to_check, NULL, G_FILE_MONITOR_EVENT_CREATED, NULL);

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
    g_signal_connect (monitor, "changed", G_CALLBACK (on_directory_changed), NULL);
  } else {
    g_error ("Error monitoring directory: %s\n", error->message);
    g_error_free (error);
  }

  g_free (path);
  g_object_unref (file);
}
