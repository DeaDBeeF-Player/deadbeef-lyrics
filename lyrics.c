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
#include <pcre.h>
#include <gtk/gtk.h>
#include <deadbeef/deadbeef.h>

//#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define trace(fmt,...)

static DB_misc_t plugin;
static DB_functions_t *deadbeef;

typedef struct _LyricsInfo
{
  char *artist;
  char *title;
  char *url;
  char *text;
  int text_size;
  GtkTextBuffer *text_buffer;
} LyricsInfo;

DB_plugin_t *
lyrics_load (DB_functions_t *api) {
    deadbeef = api;
    return DB_PLUGIN (&plugin);
}

static void
lyrics_free(LyricsInfo *lyricsInfo) {
    if (lyricsInfo->artist) {
        free(lyricsInfo->artist);
    }
    if (lyricsInfo->title) {
        free(lyricsInfo->title);
    }
    if (lyricsInfo->url) {
        free(lyricsInfo->url);
    }
    if (lyricsInfo->text) {
        free(lyricsInfo->text);
    }
    free(lyricsInfo);
}


static void
lyrics_free_callback(GtkWidget *widget, LyricsInfo *lyricsInfo) {
    lyrics_free(lyricsInfo);
}

// returns number of encoded chars on success, or -1 in case of error
static int
lyrics_uri_encode (char *out, int outl, const char *str) {
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
        ))
        {
            if (outl <= 3) {
                return -1;
            }
            snprintf (out, outl, "%%%02x", (uint8_t)*str);
            outl -= 3;
            str++;
            out += 3;
        }
        else {
            *out = *str == ' ' ? '_' : *str;
            out++;
            str++;
            outl--;
        }
    }
    *out = 0;
    return l - outl;
}

static void
lyrics_window_create (LyricsInfo *lyricsInfo) {
    GtkWidget *window;
    GtkWidget *scrollWindow;
    GtkWidget *view;
    GtkWidget *separator;
    GtkWidget *close;
    GtkWidget *vbox;
    GtkWidget *hbox;

    GtkTextBuffer *buffer;
    GtkTextIter iter;


    gdk_threads_enter();
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 600);
    gtk_window_set_title(GTK_WINDOW(window), "Lyrics");
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    GTK_WINDOW(window)->allow_shrink = TRUE;

    gtk_signal_connect (GTK_OBJECT (window), "destroy",
        G_CALLBACK(lyrics_free_callback), lyricsInfo);

    vbox = gtk_vbox_new(FALSE, 5);

    /* Text View */
    view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);

    /* GtkScrolledWindow */
    scrollWindow = gtk_scrolled_window_new(
        gtk_text_view_get_hadjustment (GTK_TEXT_VIEW(view)),
        gtk_text_view_get_vadjustment (GTK_TEXT_VIEW(view))
    );
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrollWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    gtk_container_add (GTK_CONTAINER(scrollWindow), view);

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
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, 
        lyricsInfo->artist, -1, "title", NULL);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, 
        " - ", -1, "title", NULL);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, 
        lyricsInfo->title, -1, "title", NULL);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, 
        "\n", -1, "title", NULL);

    gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
        "Loading...", -1, "text", NULL);

    /* Separator */
    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);

    /* HBox */
    hbox = gtk_hbutton_box_new();
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

    /* Close Button */
    close = gtk_button_new_with_label ("Close");
    gtk_signal_connect_object (GTK_OBJECT (close), "clicked",
                               G_CALLBACK (gtk_widget_destroy),
                               GTK_OBJECT (window));

    gtk_box_pack_start(GTK_BOX(hbox), close, FALSE, FALSE, 0);

    gtk_widget_grab_focus (close);
    gtk_widget_show (close);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    gtk_widget_show_all(window);

    gdk_threads_leave();
}

static void
lyrics_window_update (LyricsInfo *lyricsInfo) {
    GtkTextIter startIter, endIter;

    gdk_threads_enter ();
    gtk_text_buffer_get_iter_at_line (lyricsInfo->text_buffer, &startIter, 1);
    gtk_text_buffer_get_end_iter (lyricsInfo->text_buffer, &endIter);
    gtk_text_buffer_delete (lyricsInfo->text_buffer, &startIter, &endIter);

    gtk_text_buffer_insert_with_tags_by_name(lyricsInfo->text_buffer,
        &startIter, lyricsInfo->text, -1, "text", NULL);
    gdk_threads_leave ();
}

static void
lyrics_lookup_thread (void *lyricsInfo_ptr) {
    LyricsInfo *lyricsInfo = lyricsInfo_ptr;

    lyrics_window_create(lyricsInfo);

    DB_FILE *fp = deadbeef->fopen (lyricsInfo->url);
    if (!fp) {
        trace ("lyrics: failed to open %s\n", lyricsInfo->url);
        lyrics_free(lyricsInfo);
        return;
    }

    lyricsInfo->text = malloc(1);
    lyricsInfo->text_size = 1;

    char *buffer[4096];
    int len;

    while ((len = deadbeef->fread (buffer, 1, sizeof (buffer), fp)) > 0) {
        lyricsInfo->text = (char *)realloc(lyricsInfo->text, lyricsInfo->text_size + sizeof (buffer) + 1);

        if (lyricsInfo->text) {
            memcpy(lyricsInfo->text + lyricsInfo->text_size - 1, buffer, sizeof (buffer));
            lyricsInfo->text_size += sizeof (buffer);
            lyricsInfo->text[lyricsInfo->text_size] = 0;
        } else {
            lyrics_free(lyricsInfo);
            return;
            
        }        
    }

    deadbeef->fclose (fp);
    
    const char *error;
    int erroffset;
    int ovector[6];
    
    // Compile regexp
    pcre *re = pcre_compile("&lt;lyrics>(.*)&lt;/lyrics>",
    PCRE_MULTILINE | PCRE_DOTALL | PCRE_UTF8,
    &error, &erroffset, NULL);

    if (re == NULL)
    {
        printf("PCRE compilation failed at offset %d: %s\n", erroffset, error);
        lyrics_free(lyricsInfo);
        return;
    }

    int rc = pcre_exec(re, NULL, lyricsInfo->text, lyricsInfo->text_size, 0, 0, ovector, 6);

    if (rc < 0)
    {
        switch(rc)
        {
            case PCRE_ERROR_NOMATCH:
                trace("No match\n");

                gdk_threads_enter();

                GtkWidget* dialog = gtk_message_dialog_new (NULL,
                 GTK_DIALOG_DESTROY_WITH_PARENT,
                 GTK_MESSAGE_ERROR,
                 GTK_BUTTONS_CLOSE,
                 "Lyrics not found");

                g_signal_connect_swapped (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy),
                    dialog);

                gtk_widget_show(dialog);

                gdk_threads_leave();
                break;
            default:
                printf("Matching error %d\n", rc); break;
        }
        lyrics_free(lyricsInfo);
        return;
    }

    char *substring_start = lyricsInfo->text + ovector[2];
    int substring_length = ovector[3] - ovector[2];
    char *lyrics_text = NULL;
    if (-1 == asprintf (&lyrics_text, "%.*s\n", substring_length, substring_start))
    {
        lyrics_free(lyricsInfo);
        return;
    }

    free(lyricsInfo->text);
    lyricsInfo->text = lyrics_text;
    
    lyrics_window_update(lyricsInfo);
}

static int
lyrics_action_lookup (DB_plugin_action_t *action, DB_playItem_t *it)
{
    const char *artist_meta = deadbeef->pl_find_meta (it, "artist");
    const char *title_meta = deadbeef->pl_find_meta (it, "title");

    if (!title_meta || !artist_meta)
        return 0;

    char *artist = (char *)malloc(strlen(artist_meta) + 1);
    char *title = (char *)malloc(strlen(title_meta) + 1);

    strcpy(artist, artist_meta);
    strcpy(title, title_meta);

    char eartist [strlen (artist) * 3 + 1];
    char etitle [strlen (title) * 3 + 1];

    if (-1 == lyrics_uri_encode (eartist, sizeof (eartist), artist))
        return 0;

    if (-1 == lyrics_uri_encode (etitle, sizeof (etitle), title))
        return 0;

    char *url = NULL;
    if (-1 == asprintf (&url, "http://lyrics.wikia.com/index.php?title=%s:%s&action=edit", eartist, etitle))
        return 0;

    LyricsInfo *lyricsInfo = malloc(sizeof(LyricsInfo));
    lyricsInfo->artist = artist;
    lyricsInfo->title = title;
    lyricsInfo->text = NULL;
    lyricsInfo->text_size = 0;
    lyricsInfo->url = url;

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
lyrics_get_actions (DB_playItem_t *it)
{
    if (!it ||
        !deadbeef->pl_find_meta (it, "artist") ||
        !deadbeef->pl_find_meta (it, "title"))
    {
         lookup_action.flags |= DB_ACTION_DISABLED;
    }
    else
    {
         lookup_action.flags &= ~DB_ACTION_DISABLED;
    }
    return &lookup_action;
}

// define plugin interface
static DB_misc_t plugin = {
    DB_PLUGIN_SET_API_VERSION
    .plugin.version_major = 1,
    .plugin.version_minor = 0,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.name = "lyrics",
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
    .plugin.website = "",
    .plugin.get_actions = lyrics_get_actions
};
