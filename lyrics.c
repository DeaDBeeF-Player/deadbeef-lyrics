/*
    Lyrics plugin for DeaDBeeF music player
    Copyright (C) 2011 Oleg Shparber <trollixx@users.sourceforge.net>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <deadbeef/deadbeef.h>

#define trace(...) { fprintf(stderr, __VA_ARGS__); }
//#define trace(fmt,...)

#if !GTK_CHECK_VERSION(2,22,0)
GtkAdjustment* gtk_text_view_get_hadjustment(GtkTextView *text_view) {
    return text_view->hadjustment;
}
GtkAdjustment* gtk_text_view_get_vadjustment(GtkTextView *text_view) {
    return text_view->vadjustment;
}
#endif

static DB_misc_t plugin;
static DB_functions_t *deadbeef;

typedef struct _LyricsInfo {
  char *artist;
  char *title;
  char *url;
  char *text;
  int text_size;
  GtkTextBuffer *text_buffer;
  gboolean window_closed;
  gboolean error;
  uintptr_t mutex;
} LyricsInfo;

DB_plugin_t *
ddb_lyrics_load(DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN(&plugin);
}

static void
lyrics_free(void *lyricsInfo_ptr) {
    LyricsInfo *lyricsInfo = lyricsInfo_ptr;

    deadbeef->mutex_lock(lyricsInfo->mutex);
    free(lyricsInfo->artist);
    free(lyricsInfo->title);
    free(lyricsInfo->url);
    free(lyricsInfo->text);
    deadbeef->mutex_unlock(lyricsInfo->mutex);
    deadbeef->mutex_free(lyricsInfo->mutex);
    free(lyricsInfo);
}


static void
lyrics_free_callback(GtkWidget *widget, LyricsInfo *lyricsInfo) {
    lyricsInfo->window_closed = TRUE;
    deadbeef->thread_start(lyrics_free, lyricsInfo);
}

// returns number of encoded chars on success, or -1 in case of error
static int
lyrics_uri_encode(char *out, int outl, const char *str) {
    int l = outl;

    while (*str) {
        if (outl <= 1) {
            return -1;
        }

        if (!(
            (*str >= '0' && *str <= '9') ||
            (*str >= 'a' && *str <= 'z') ||
            (*str >= 'A' && *str <= 'Z') ||
            (*str == ' ')
        )) {
            if (outl <= 3) {
                return -1;
            }
            snprintf(out, outl, "%%%02x", (uint8_t)*str);
            outl -= 3;
            str++;
            out += 3;
        } else {
            *out = *str == ' ' ? '_' : *str;
            out++;
            str++;
            outl--;
        }
    }
    *out = 0;
    return l - outl;
}

static gboolean
lyrics_window_close(GtkWidget *widget, GdkEventKey *event, GtkWindow *window) {
    if (event->keyval == GDK_Escape) {
        gtk_widget_destroy(GTK_WIDGET(window));
    }
    return TRUE;
}

static void
lyrics_window_create(LyricsInfo *lyricsInfo) {
    GtkWidget *window;
    GtkWidget *scrollWindow;
    GtkWidget *view;
    GtkWidget *separator;
    GtkWidget *close;
    GtkWidget *vbox;
    GtkWidget *hbox;

    GtkTextBuffer *buffer;
    GtkTextIter iter;

    char *window_title = NULL;
    if (-1 == asprintf(&window_title, "%s - %s", lyricsInfo->artist, lyricsInfo->title)) {
        lyrics_free(lyricsInfo);
        return;
    }

    gdk_threads_enter();
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 600);
    gtk_window_set_title(GTK_WINDOW(window), window_title);
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    GTK_WINDOW(window)->allow_shrink = TRUE;

    gtk_signal_connect(GTK_OBJECT(window), "destroy",
        G_CALLBACK(lyrics_free_callback), lyricsInfo);

    gtk_signal_connect(GTK_OBJECT(window), "key-release-event",
        G_CALLBACK(lyrics_window_close), GTK_OBJECT(window));

    vbox = gtk_vbox_new(FALSE, 5);

    /* Text View */
    view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);

    /* GtkScrolledWindow */
    scrollWindow = gtk_scrolled_window_new(
        gtk_text_view_get_hadjustment(GTK_TEXT_VIEW(view)),
        gtk_text_view_get_vadjustment(GTK_TEXT_VIEW(view))
    );
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(scrollWindow), view);

    gtk_box_pack_start(GTK_BOX(vbox), scrollWindow, TRUE, TRUE, 0);

    /* GtkTextViewBuffer */
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    lyricsInfo->text_buffer = buffer;

    /* Tags */
    gtk_text_buffer_create_tag(buffer, "title",
        "weight", PANGO_WEIGHT_BOLD,
        "pixels_below_lines", 10,
        "font", "20",
        "left_margin", 5,
        NULL);

    gtk_text_buffer_create_tag(buffer, "text",
        "left_margin", 5,
        NULL);

    gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

    /* Text inserts */
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
        window_title, -1, "title", NULL);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
        "\n", -1, "title", NULL);

    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
        "Loading...", -1, "text", NULL);

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, TRUE, 0);

    /* HBox */
    hbox = gtk_hbutton_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    /* Close Button */
    close = gtk_button_new_with_label("Close");
    gtk_signal_connect_object(GTK_OBJECT(close), "clicked",
                              G_CALLBACK(gtk_widget_destroy),
                              GTK_OBJECT(window));

    gtk_box_pack_start(GTK_BOX(hbox), close, FALSE, FALSE, 0);

    gtk_widget_grab_focus(close);
    gtk_widget_show(close);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    gtk_widget_show_all(window);

    free(window_title);

    gdk_threads_leave();
}

static void
lyrics_window_update(LyricsInfo *lyricsInfo) {
    GtkTextIter startIter, endIter;

    gdk_threads_enter();
    gtk_text_buffer_get_iter_at_line(lyricsInfo->text_buffer, &startIter, 1);
    gtk_text_buffer_get_end_iter(lyricsInfo->text_buffer, &endIter);
    gtk_text_buffer_delete(lyricsInfo->text_buffer, &startIter, &endIter);

    if (!lyricsInfo->error) {
        gtk_text_buffer_insert_with_tags_by_name(lyricsInfo->text_buffer,
            &startIter, lyricsInfo->text, -1, "text", NULL);
    } else {
        gtk_text_buffer_insert_with_tags_by_name(lyricsInfo->text_buffer,
            &startIter, "Not found", -1, "text", NULL);
    }
    gdk_threads_leave();
}

static void
lyrics_lookup_thread(void *lyricsInfo_ptr) {
    LyricsInfo *lyricsInfo = lyricsInfo_ptr;

    deadbeef->mutex_lock(lyricsInfo->mutex);

    lyrics_window_create(lyricsInfo);

    DB_FILE *fp = deadbeef->fopen(lyricsInfo->url);
    if (!fp) {
        trace("lyrics: failed to open %s\n", lyricsInfo->url);
        lyricsInfo->error = TRUE;
        goto update;
    }

    lyricsInfo->text = malloc(1);
    lyricsInfo->text_size = 1;

    char *buffer[4096];
    int len;

    while ((len = deadbeef->fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        lyricsInfo->text = (char *)realloc(lyricsInfo->text, lyricsInfo->text_size + sizeof(buffer) + 1);

        if (lyricsInfo->text) {
            memcpy(lyricsInfo->text + lyricsInfo->text_size - 1, buffer, sizeof(buffer));
            lyricsInfo->text_size += sizeof(buffer);
            lyricsInfo->text[lyricsInfo->text_size] = 0;
        } else {
            deadbeef->fclose(fp);
            lyricsInfo->error = TRUE;
            goto update;
        }
    }

    deadbeef->fclose(fp);

    if (lyricsInfo->window_closed) {
        deadbeef->mutex_unlock(lyricsInfo->mutex);
        return;
    }

    // Catch placeholder
    char *placeholder = "PUT LYRICS HERE";
    if (strstr(lyricsInfo->text, placeholder)) {
        lyricsInfo->error = TRUE;
        goto update;
    }

    char *startAnchor = "&lt;lyrics>";
    char *endAnchor = "&lt;/lyrics>";
    char *startIndex = strstr(lyricsInfo->text, startAnchor);
    char *endIndex = strstr(lyricsInfo->text, endAnchor);

    if (startIndex && endIndex) {
        startIndex += strlen(startAnchor);
        memcpy(lyricsInfo->text, startIndex, endIndex - startIndex + 1);
        lyricsInfo->text = realloc(lyricsInfo->text, endIndex - startIndex + 1);
        lyricsInfo->text[endIndex - startIndex] = 0;
    } else {
        lyricsInfo->error = TRUE;
    }


update:
    if (!lyricsInfo->window_closed) {
        lyrics_window_update(lyricsInfo);
    }

    deadbeef->mutex_unlock(lyricsInfo->mutex);
}

static int
lyrics_action_lookup(DB_plugin_action_t *action, DB_playItem_t *it) {
    const char *artist_meta = deadbeef->pl_find_meta(it, "artist");
    const char *title_meta = deadbeef->pl_find_meta(it, "title");

    if (!title_meta || !artist_meta) {
        return 0;
    }

    char *artist = (char *)malloc(strlen(artist_meta) + 1);
    char *title = (char *)malloc(strlen(title_meta) + 1);

    strcpy(artist, artist_meta);
    strcpy(title, title_meta);

    char eartist [strlen(artist) * 3 + 1];
    char etitle [strlen(title) * 3 + 1];

    if (-1 == lyrics_uri_encode(eartist, sizeof(eartist), artist)) {
        return 0;
    }

    if (-1 == lyrics_uri_encode(etitle, sizeof(etitle), title)) {
        return 0;
    }

    char *url = NULL;
    if (-1 == asprintf(&url, "http://lyrics.wikia.com/index.php?title=%s:%s&action=edit", eartist, etitle)) {
        return 0;
    }

    LyricsInfo *lyricsInfo = malloc(sizeof(LyricsInfo));
    lyricsInfo->artist = artist;
    lyricsInfo->title = title;
    lyricsInfo->text = NULL;
    lyricsInfo->text_size = 0;
    lyricsInfo->url = url;
    lyricsInfo->window_closed = FALSE;
    lyricsInfo->error = FALSE;
    lyricsInfo->mutex = deadbeef->mutex_create();

    deadbeef->thread_start(lyrics_lookup_thread, lyricsInfo);
    return 0;
}

static DB_plugin_action_t lookup_action = {
    .title = "Find lyrics",
    .name = "lyrics_lookup",
    .flags = DB_ACTION_SINGLE_TRACK,
    .callback = lyrics_action_lookup,
    .next = NULL
};

static DB_plugin_action_t *
lyrics_get_actions(DB_playItem_t *it) {
    if (!it ||
        !deadbeef->pl_find_meta(it, "artist") ||
        !deadbeef->pl_find_meta(it, "title")) {
         lookup_action.flags |= DB_ACTION_DISABLED;
    } else {
         lookup_action.flags &= ~DB_ACTION_DISABLED;
    }
    return &lookup_action;
}

// define plugin interface
static DB_misc_t plugin = {
    DB_PLUGIN_SET_API_VERSION
    .plugin.version_major = 0,
    .plugin.version_minor = 2,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.name = "Lyrics",
    .plugin.descr = "Lyrics plugin for DeaDBeeF music player",
    .plugin.copyright =
        "Copyright (C) 2011 Oleg Shparber <trollixx@users.sourceforge.net>\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website = "https://bitbucket.org/trollixx/deadbeef-lyrics",
    .plugin.get_actions = lyrics_get_actions
};
