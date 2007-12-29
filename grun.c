/* gRun - GTK application launcher dialog
 * Copyright (C) 1998 Southern Gold Development <tangomanrulz@geocities.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "config.h"
#include <unistd.h>
#include <dirent.h>
#include "grun2.xpm"

#if defined (HAVE_GETTEXT) || defined (HAVE_CATGETS)
#include <libintl.h>
#else
#include "intl/libintl.h"
#endif

#define PATH_CHAR	":"
#define DIR_CHAR	"/"
#define DIR_INT 	'/'
#define DOT_CHAR	"."
#define ARG_BASE	1

#include <sys/stat.h>

#define MAX_BUFF	1024

typedef struct grun sgrun;

struct grun {
	GtkWidget *cmb;
	GtkWidget *rsp;
	GtkWidget *win;
	GList *history;
	guint cmdLen;
	guchar status;
};

/* Function prototypes */

int isFileX(const gchar *file);

#ifdef ASSOC
gchar *getAssoc(const gchar *ext);
#endif /* ASSOC */

char *getLine(FILE *file);
int isFileExec(const gchar *file);
GList *loadHistory();
int saveHistory(const char *cmd, sgrun *gdat);
void gquit();
void startApp(const gchar *cmd, sgrun *gdat);
gint gcomplete(sgrun *gdat, const gchar *twrk);
gint gdircomplete(sgrun *gdat, const gchar *twrk);
gint gdirmapcomplete(sgrun *gdat, const gchar *twrk);
gint gclick(GtkWidget *widget, GdkEventKey *event, gpointer data);
gint gexit(GtkWidget *widget, GdkEvent *event, gpointer data);
void launch(GtkWidget *widget, gpointer data);
void cancel(GtkWidget *widget, gpointer data);
void bclose(GtkWidget *widget, gpointer data);
void bcancel(GtkWidget *widget, gpointer data);
void browse(GtkWidget *widget, gpointer data);

/* End prototypes */

char *getLine(FILE *file) {
	char *tmp, in;
	int cnt = 0, retIn;

	if (feof(file)) {
		return NULL;
	}
	else {
		tmp = g_malloc(sizeof(char) * MAX_BUFF);
		retIn = fread((void *) &in, sizeof(char), 1, file);
		if (retIn != 1) {
			g_free(tmp);
			return NULL;
		}
		while ((in != '\n') && (cnt < MAX_BUFF)) {
			tmp[cnt] = in;
			cnt++;
			retIn = fread((void *) &in, sizeof(char), 1, file);
			if (retIn != 1) {
				g_free(tmp);
				return NULL;
			}
		}
		if (cnt == MAX_BUFF) {
			g_free(tmp);
			return NULL;
		}
		tmp[cnt] = '\0';
		return tmp;
	}	
}

int isFileX(const gchar *file) {
#ifndef TESTFILE
        return TRUE; // Ie, we'll consider everything an executable.
#else
	FILE *fHnd;
	gchar *fname, *home_env, *line, *twrk, *hold, *split, *rdx;
	struct stat buff;
	int err;
	
	twrk = g_strdup(file);
	hold = twrk;
	split = strsep(&twrk, " ");
	rdx = rindex(split, '/');
	if (rdx) {
		split = ++rdx;
	}

	home_env = getenv("HOME");
	fname = g_strconcat(home_env, DIR_CHAR, DOT_CHAR, "consfile", NULL);
	
	err = stat(fname, &buff);
	if (err == -1) {
			g_free(fname);
			fname = g_strconcat(DATADIR, "/grun/consfile", NULL);
	}

	fHnd = fopen(fname, "rb");
	g_free(fname);

	if (!fHnd) {
		return -1;
	}
	line = getLine(fHnd);
	if (!line) {
		g_free(hold);
		fclose(fHnd);
		return TRUE;
	}
	while (line) {
		if (strcmp(line, split) == 0) {
			g_free(hold);
			fclose(fHnd);
			return FALSE;
		}
		g_free(line);
		line = getLine(fHnd);
	}
	g_free(hold);
	fclose(fHnd);
	return TRUE;
#endif /* TESTFILE */
}

#ifdef ASSOC
gchar *getAssoc(const gchar *ext) {
	FILE *fHnd;
	gchar *line, *split, *fname, *rsp, *home_env;
	struct stat buff;
	int err;

	home_env = getenv("HOME");
	fname = g_strconcat(home_env, DIR_CHAR, DOT_CHAR, "gassoc", NULL);

	err = stat(fname, &buff);
	if (err == -1) {
			g_free(fname);
			fname = g_strconcat(DATADIR, "/grun/gassoc", NULL);
	}

	fHnd = fopen(fname, "rb");
	g_free(fname);

	if (!fHnd) {
		return NULL;
	}

	line = getLine(fHnd);
	while (line) {
		fname = line;
		split = strsep(&line, ":");
		if (strcasecmp(split, ext) == 0) {
			rsp = g_strdup(line);
			g_free(fname);
			fclose(fHnd);
			return rsp;
		}
		g_free(fname);
		line = getLine(fHnd);
	}
	fclose(fHnd);
	return NULL;
}
#endif

int isFileExec(const gchar *file) {
	int err;
	char *twrk, *awrk, *split, *hold, *path, *inc;
	struct stat buff;

	inc = g_strdup(file);
	hold = inc;
	split = strsep(&inc, " ");

	err = stat(split, &buff);
	if (err == -1) {
		twrk = getenv("PATH");
		path = g_strdup(twrk);
		inc = path;
		twrk = strsep(&path, PATH_CHAR);
		while (twrk) {
			awrk = g_strconcat(twrk, DIR_CHAR, split, NULL);
			err = stat(awrk, &buff);
			g_free(awrk);
			if (err != -1) {
				g_free(hold);
				g_free(inc);
				if (buff.st_mode & S_IEXEC) {
					return TRUE;
				}
				else {
					return FALSE;
				}
			}
			else {
				twrk = strsep(&path, PATH_CHAR);
			}
		}
	}
	else {
		g_free(hold);
		if (buff.st_mode & S_IEXEC) {
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	return -1;
}

GList *loadHistory() {
	GList *tmp;
	char *line, *home_env, *fname;
	FILE *fHnd;

	home_env = getenv("HOME");
	fname = g_strconcat(home_env, DIR_CHAR, DOT_CHAR, "grun_history", NULL);

	fHnd = fopen(fname, "rb");
	g_free(fname);

	if (!fHnd) {
		return NULL;
	}
	tmp = g_list_alloc();
	line = getLine(fHnd);
	if (!line) {
		fclose(fHnd);
		g_list_free(tmp);
		return NULL;
	}
	tmp->data = (gpointer) line;
	while (line) {
		line = getLine(fHnd);
		if (line) {
			tmp = g_list_prepend(tmp, (gpointer) line);
		}
	}
	fclose(fHnd);
	return tmp;
}

int saveHistory(const char *cmd, sgrun *gdat) {
	FILE *fHnd;
	char *fname, *home_env;
	int err, found;
	GtkWidget *list;

	found = isFileExec(cmd);
	if (found != -1) {
		home_env = getenv("HOME");
		fname = g_strconcat(home_env, DIR_CHAR, DOT_CHAR, "grun_history", NULL);
	
		fHnd = fopen(fname, "ab");
		g_free(fname);
	
		if (!fHnd) {
			return FALSE;
		}
		
		err = FALSE;
		list = GTK_COMBO(gdat->cmb)->list;
		if (!(GTK_LIST (list)->selection)) {
			fprintf(fHnd, "%s\n", cmd);
			err = TRUE;
		}
		fclose(fHnd);
		return err;
	}
	return FALSE;
}

void gquit() {
	gtk_main_quit();
	exit(0);
	return;
}

void startApp(const gchar *cmd, sgrun *gdat) {
	char **args, *work, *twrk, *assoc, *hold, *split;
	int cnt, len, scnt, pid, res;

	res = isFileExec(cmd);
	if (res == -1) {
			if (gdat) {
				if (!(gdat->status & 0x80)) {
					g_list_free(gdat->history);
					gtk_widget_destroy(gdat->win);
				}
			}
			else {
				exit(0);
			}
	}
	else {
		if (res) {
			res = isFileX(cmd);
			if (res) {
				work = g_strdup(cmd);
			}
			else {
				work = g_strconcat(XTERM, " -e ", cmd, NULL);
			}
		}
		else {
#ifdef ASSOC
			twrk = g_strdup(cmd);
			hold = twrk;
			split = strsep(&twrk, " ");
			twrk = g_strdup(split);
			g_free(hold);
			split = rindex(twrk, '.');
			if (split) {
				split++;
				assoc = getAssoc(split);
				g_free(twrk);
				if (assoc) {
					work = g_strconcat(assoc, " ", cmd, NULL);
					g_free(assoc);
				}
				else {
					if (gdat) {
						if (!(gdat->status & 0x80)) {
							g_list_free(gdat->history);
							gtk_widget_destroy(gdat->win);
						}
					}
					else {
						exit(0);
					}
				}
			}
			else {
				if (gdat) {
					if (!(gdat->status & 0x80)) {
						g_list_free(gdat->history);
						gtk_widget_destroy(gdat->win);
					}
				}
				else {
					exit(0);
				}
			}
#else
			if (gdat) {
				if (!(gdat->status & 0x80)) {
					g_list_free(gdat->history);
					gtk_widget_destroy(gdat->win);
				}
			}
			else {
				exit(0);
			}
#endif
		}
	}

	pid = fork();
	if (pid == 0) {
		pid = fork();
		if (pid == 0) {
			len = strlen(work);
			scnt = 1;
			for (cnt = 0; cnt < len; cnt++) {
				if (work[cnt] == ' ') {
					scnt++;
				}
			}
			args = g_malloc(sizeof(char *) * (scnt + 1));
			args[scnt] = NULL;
			if (scnt == 1) {
					args[0] = g_strdup(work);
			}
			else {
				twrk = (char *) strsep(&work, " ");
				args[0] = twrk;
				cnt = 1;
				while (twrk) {
					twrk = (char *) strsep(&work, " ");
					args[cnt] = twrk;
					cnt++;
				}
				args[cnt] = work;
			}
			execvp(args[0], args);
			_exit(0);
		}
		else {
			_exit(0);
		}
	}
	else {
		if (gdat) {
			cnt = saveHistory(cmd, gdat);
			if (gdat->status & 0x80) {
				if (cnt) {
					gdat->history = g_list_prepend(gdat->history, (gpointer) cmd);
					gtk_combo_set_popdown_strings(GTK_COMBO (gdat->cmb), gdat->history);
				}
                                /* the order does matter here;
                                   move the cursor first,
                                   then select the region */
				gtk_entry_set_position(GTK_ENTRY (GTK_COMBO (gdat->cmb)->entry), 0);
				gtk_entry_select_region(GTK_ENTRY (GTK_COMBO (gdat->cmb)->entry), 0, -1);
				gdat->cmdLen = 0;
				gtk_signal_emit_stop_by_name(GTK_OBJECT ((GTK_COMBO (gdat->cmb))->entry), "key_press_event");
			}
			else {
				g_list_free(gdat->history);
				gtk_widget_destroy(gdat->win);
			}
		}
		else {
			_exit(0);
		}
	}
}

/* This function takes a command fragment and searches through
   the history for the first match it finds. This is loaded into
   the entry box */

gint gcomplete(sgrun *gdat, const gchar *twrk) {
		GList *work;
		gint pos, len;

		pos = strlen(twrk);
		work = gdat->history;
		while (work) {
			if (strncmp(twrk, (gchar *) work->data, pos) == 0) {
				gdat->cmdLen = pos;
				gtk_entry_set_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), (gchar *) work->data);
				len = strlen(work->data);
				gtk_entry_select_region(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), pos, len);
				gtk_entry_set_position(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), pos);
				gtk_signal_emit_stop_by_name(GTK_OBJECT ((GTK_COMBO (gdat->cmb))->entry), "key_press_event");
				return TRUE;
			}
			work = work->next;
		}
		return FALSE;
}

/* This function takes a command fragment and searches through
   all the directories in the PATH and looks for the first match
   that it finds. This is loaded into the entry box. This is bound
	to Tab/Esc/F2 if the first character is not / */

gint gdircomplete(sgrun *gdat, const gchar *twrk) {
	DIR *dir;
	gchar *path, *inc, *wdir, *hold;
	struct dirent *file;
	struct stat buf;
	gint pos, len, err;

	pos = strlen(twrk);
	inc = getenv("PATH");
	path = g_strdup(inc);
	hold = path;
	if (strlen(path) < 1) {
		return FALSE;
	}
	wdir = strsep(&path, PATH_CHAR);
	while (wdir) {
		dir = opendir(wdir);
		if (dir) {
			file = readdir(dir);
			while (file) {
				if (strncmp(twrk, file->d_name, pos) == 0) {
					inc = g_strconcat(wdir, DIR_CHAR, file->d_name, NULL);
					err = stat(inc, &buf);
					g_free(inc);
					if (buf.st_mode & S_IEXEC) {
						gdat->cmdLen = pos;
						gtk_entry_set_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), (gchar *) file->d_name);
						len = strlen(file->d_name);
						gtk_entry_select_region(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), pos, len);
						gtk_entry_set_position(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), pos);
						closedir(dir);
						g_free(hold);
						return TRUE;
					}
				}
				file = readdir(dir);
			}
			closedir(dir);
		}
		wdir = strsep(&path, PATH_CHAR);
	}
	g_free(hold);
	return FALSE;
}

/* This function takes a command fragment and splits it into
	a path and a fragment. The path is then checked for the
	first match of the fragment and completes the command. If
	the match is a directory, a / is appended. This is bound
	to Tab/Esc/F2 if the first character is a / */

gint gdirmapcomplete(sgrun *gdat, const gchar *twrk) {
	DIR *dir;
	gchar *path, *frag, *wdir, *hold, *inc, *whold;
	struct dirent *file;
	struct stat buf;
	gint pos, len, err,lenf;

	pos = strlen(twrk);
	path = g_strdup(twrk);
	hold = path;
	frag = rindex(path, DIR_INT);
	frag[0] = '\0';
	frag++;
	lenf = strlen(frag);
	wdir = g_strconcat(path, "/", NULL);
	
	dir = opendir(wdir);
	if (dir) {
		file = readdir(dir);
		while (file) {
			if (strncmp(frag, file->d_name, lenf) == 0) {
				inc = g_strconcat(wdir, file->d_name, NULL);
				err = stat(inc, &buf);
				if (buf.st_mode & S_IFDIR) {
					whold = g_strconcat(inc, "/", NULL);
				}
				else {
					whold = g_strdup(inc);
				}
				g_free(inc);
				if (err != -1) {
					gdat->cmdLen = pos;
					gtk_entry_set_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), (gchar *) whold);
					len = strlen(whold);
					gtk_entry_select_region(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), pos, len);
					gtk_entry_set_position(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry), pos);
					closedir(dir);
					g_free(whold);
					g_free(hold);
					return TRUE;
				}
				else {
					g_free(whold);
				}
			}
			file = readdir(dir);
		}
		closedir(dir);
	}	
	g_free(hold);
	return FALSE;		
}
	
gint gclick(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	gchar *cmd, *twrk, *tmp;
	gint res;
	sgrun *gdat;

	gdat = (sgrun *) data;
/* Fire off executable in command box */
	switch (event->keyval) {
		case GDK_Return:
		case GDK_KP_Enter:
			cmd = gtk_entry_get_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry));
			if (strlen(cmd) > 0) {
				startApp(cmd, gdat);
			}
			return FALSE;
			break;
/* Handle backspace with autocomplete */
		case GDK_BackSpace:
			cmd = gtk_entry_get_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry));
			if ((strlen(cmd) > 0) && (gdat->cmdLen > 0)) {
				twrk = g_strdup(cmd);
				gdat->cmdLen--;
				twrk[gdat->cmdLen] = '\0';
				if (strlen(twrk) > 0) {
					res = gcomplete(gdat, twrk);
				}
				else {
					res = TRUE;
					gtk_entry_set_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry),twrk);
					gtk_signal_emit_stop_by_name(GTK_OBJECT ((GTK_COMBO (gdat->cmb))->entry), "key_press_event");
				}
				g_free(twrk);
				return res;
			}
			else {
				return FALSE;
			}
			break;
/* Autocomplete from PATH if Tab */
		case GDK_Tab:
		case GDK_Escape:
			cmd = gtk_entry_get_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry));
			if ((strlen(cmd) == 0) || (gdat->cmdLen == 0)) {
				return FALSE;
			}
			tmp = g_strdup(cmd);
			twrk = g_strdup(cmd);
			if ((gint)strlen(cmd) != gdat->cmdLen) {
				twrk[gdat->cmdLen] = '\0';
			}
			if (twrk[0] == DIR_INT) {
				res = gdirmapcomplete(gdat, twrk);
			}
			else {
				res = gdircomplete(gdat, twrk);
			}
			cmd = gtk_entry_get_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry));
			if (strcmp(cmd, tmp) != 0) {
					gtk_signal_emit_stop_by_name(GTK_OBJECT ((GTK_COMBO (gdat->cmb))->entry), "key_press_event");
			}
			g_free(twrk);
			g_free(tmp);
			return res;
			break;
/* Autocomplete from history */
		default:
			/* make sure it's a non-command key */
			cmd = gtk_entry_get_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry));
			if (event->length > 0) {
				twrk = g_strdup(cmd);
				twrk = g_realloc(twrk, (strlen(twrk) + event->length + 1));
				twrk[gdat->cmdLen] = '\0';
				strncat(twrk, event->string, event->length);
				if (strlen (twrk) > 0) {
					res = gcomplete(gdat, twrk);
				}
				else {
					res = TRUE;
					gtk_entry_set_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry),twrk);
					gtk_signal_emit_stop_by_name(GTK_OBJECT ((GTK_COMBO (gdat->cmb))->entry), "key_press_event");
				}
				if (res == FALSE) {
					if (event->length > 0) {
						gdat->cmdLen++;
					}
				}
				g_free(twrk);
				return res;
			}
			else {
			/* Fixes problem with command length and End */
				if ((event->keyval == GDK_End) && (strlen(cmd) != gdat->cmdLen)) {
					gdat->cmdLen = strlen(cmd);
				}
				return FALSE;
			}
			break;
	}
	return FALSE;
}

gint gexit(GtkWidget *widget, GdkEvent *event, gpointer data) {
	gquit();
	return FALSE;
}

void launch(GtkWidget *widget, gpointer data) {
	gchar *cmd;
	sgrun *gdat;

	gdat = (sgrun *) data;
	cmd = gtk_entry_get_text(GTK_ENTRY ((GTK_COMBO (gdat->cmb))->entry));
	if (strlen(cmd) > 0) {
		startApp(cmd, gdat);
	}
}

void cancel(GtkWidget *widget, gpointer data) {
	sgrun *gdat;

	gdat = (sgrun *) data;
	g_list_free(gdat->history);
	gtk_widget_destroy(gdat->win);
}

void bclose(GtkWidget *widget, gpointer data) {
	char *cmd;
	sgrun *gdat;

	gdat = (sgrun *) data;
	cmd = gtk_file_selection_get_filename(GTK_FILE_SELECTION (gdat->rsp));
	gtk_entry_set_text(GTK_ENTRY (GTK_COMBO (gdat->cmb)->entry), cmd);
	gtk_widget_destroy(gdat->rsp);
	gdat->rsp = NULL;
}

void bcancel(GtkWidget *widget, gpointer data) {
	sgrun *gdat;

	gdat = (sgrun *) data;
	gtk_widget_destroy(gdat->rsp);
	gdat->rsp = NULL;
}

void browse(GtkWidget *widget, gpointer data) {
	GtkWidget *rsp;
	sgrun *gdat;

	gdat = (sgrun *) data;
	rsp = gtk_file_selection_new(gettext("Choose Application"));
	gdat->rsp = rsp;
	gtk_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (rsp)->ok_button), "clicked", (GtkSignalFunc) bclose, data);
	gtk_signal_connect(GTK_OBJECT (GTK_FILE_SELECTION (rsp)->cancel_button), "clicked", (GtkSignalFunc) bcancel, data);
	gtk_widget_show(rsp);
}

int main(int argc, char **argv) {
	GtkWidget *win, *btn, *lbl, *cmb, *fix;
	GtkTooltips *tips;
	int c, cnt, len, persist, tooltips;
	char *home_env, *fname, *cmd, *icmd;
	GList *list;
	sgrun *gdat;
	GdkPixmap *pix;
	GdkBitmap *mask;
	GtkStyle *style;

	gtk_init(&argc, &argv);
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);
 
/* New parameter scan routine, checks all parameters */
	persist = FALSE;
	tooltips = TRUE;
	icmd = NULL;
	if (argc > ARG_BASE) {
		for (c = ARG_BASE; c < argc; c++) {
			if ((strcmp(argv[c], "--help") == 0) && (argv[1][0] == '-')) {
				g_print(gettext("gRun launches applications under X\n\nUsage grun [options] [file options]\n\n   file options     Launch file directly, no dialog boxes\n   --persist        Dialog persistance\n   --preload file   Load file as default command\n   --notips         Disable tooltips\n   --version        Show version\n   --help           Show this guide \n\nCopyright Southern Gold Development 1998\n"));
				exit(0);
			}
			if ((strcmp(argv[c], "--version") == 0) && (argv[1][0] == '-')) {
				g_print(gettext("gRun version %s\n\nCopyright Southern Gold Development 1998\n"), VERSION);
				exit(0);
			}
			if (strcmp(argv[c], "--persist") == 0) {
				persist = TRUE;
			}
			if (strcmp(argv[c], "--notips") == 0) {
				tooltips = FALSE;
			}
			if (strcmp(argv[c], "--preload") == 0) {
				c++;
				icmd = g_malloc0(sizeof(gchar) * 1);
				while (c < argc) {
						cmd = g_strconcat(icmd, " ", argv[c], NULL);
						g_free(icmd);
						icmd = cmd;
						c++;
				}
				cmd = icmd + 1;
				g_free(icmd);
				icmd = g_strdup(cmd);
			}
		}
			
		if (argv[1][0] != '-') {
			len = 0;
			for (c = 1; c < argc; c++) {
				len += strlen(argv[c]) + 1;
			}
			cmd = g_malloc0(sizeof(gchar) * (len + 1));
			for (c = 1; c < argc; c++) {
				strcat(cmd, argv[c]);
				strcat(cmd, " ");
			}
			cmd[len - 1] = '\0';
			startApp(cmd, NULL);
		}
	}

	home_env = getenv("HOME");
	fname = g_strconcat(home_env, DIR_CHAR, DOT_CHAR, "grunrc", NULL);

	gtk_rc_parse(fname);
	g_free(fname);
	gdat = g_malloc(sizeof(sgrun));
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gdat->win = win;
	gtk_window_position(GTK_WINDOW(win), GTK_WIN_POS_NONE);
	fname = g_malloc(sizeof(gchar) * (strlen(VERSION) + strlen(gettext("gRun %s")) + 1));
	sprintf(fname, gettext("gRun %s"), VERSION);
	gtk_window_set_title(GTK_WINDOW(win), fname);
	g_free(fname);
	gtk_widget_set_usize(GTK_WIDGET(win), 250, 100);
	gtk_signal_connect(GTK_OBJECT (win), "destroy", (GtkSignalFunc) gexit, NULL);
	gtk_container_border_width(GTK_CONTAINER (win), 5);

	tips = gtk_tooltips_new();
	if (tooltips) {
		gtk_tooltips_enable(tips);
	}
	else {
		gtk_tooltips_disable(tips);
	}

	fix = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(win), fix);

	lbl = gtk_label_new(gettext("Launch Application"));
	gtk_fixed_put(GTK_FIXED (fix), lbl, 65, 4);
	gtk_widget_show(lbl);

	list = loadHistory();
	gdat->history = list;
	gdat->cmdLen = 0;
	if (persist) {
		gdat->status = 0x80;	
	}
	else {
		gdat->status = 0x00;
	}

	cmb = gtk_combo_new();
	gdat->cmb = cmb;
	gdat->rsp = NULL;
	gtk_widget_set_usize(GTK_WIDGET (GTK_COMBO(cmb)->entry), 215, 24);
	gtk_fixed_put(GTK_FIXED (fix), cmb, 5, 27);
	if (list) {
		gtk_combo_set_popdown_strings(GTK_COMBO (cmb), list);
		gtk_entry_set_text(GTK_ENTRY (GTK_COMBO (cmb)->entry), list->data);
		gtk_entry_set_position(GTK_ENTRY (GTK_COMBO (cmb)->entry), 0);
		gtk_entry_select_region(GTK_ENTRY (GTK_COMBO (cmb)->entry), 0, -1);
	} else {
		gtk_entry_set_text(GTK_ENTRY (GTK_COMBO (cmb)->entry), "");
	}
	if (icmd) {
		gtk_entry_set_text(GTK_ENTRY (GTK_COMBO (cmb)->entry), icmd);
		gtk_entry_set_position(GTK_ENTRY (GTK_COMBO (cmb)->entry), 0);
		gtk_entry_select_region(GTK_ENTRY (GTK_COMBO (cmb)->entry), 0, -1);
	}
	gtk_signal_connect(GTK_OBJECT (GTK_COMBO (cmb)->entry), "key_press_event", (GtkSignalFunc) gclick, (gpointer) gdat);
	gtk_window_set_focus(GTK_WINDOW (win), GTK_COMBO (cmb)->entry);
	gtk_widget_show(cmb);

	btn = gtk_button_new_with_label(gettext("OK"));
	gtk_widget_set_usize(GTK_WIDGET(btn), 70, 24);
	gtk_fixed_put(GTK_FIXED (fix), btn, 5, 60);
	gtk_signal_connect(GTK_OBJECT (btn), "clicked", (GtkSignalFunc) launch, (gpointer) gdat);
	gtk_tooltips_set_tip(tips, btn, gettext("Launch application"), gettext("The OK button launches the application given in the entry box"));
	gtk_widget_show(btn);

	if (persist) {
		btn = gtk_button_new_with_label(gettext("Close"));
	}
	else {
		btn = gtk_button_new_with_label(gettext("Cancel"));
	}

	gtk_widget_set_usize(GTK_WIDGET(btn), 70, 24);
	gtk_fixed_put(GTK_FIXED (fix), btn, 85, 60);
	gtk_signal_connect(GTK_OBJECT (btn), "clicked", (GtkSignalFunc) cancel, (gpointer) gdat);
	if (persist) {
		gtk_tooltips_set_tip(tips, btn, gettext("Close gRun"), gettext("The Close button will close the persistant gRun window"));
	}
	else {
		gtk_tooltips_set_tip(tips, btn, gettext("Cancel launch"), gettext("The Cancel button cancels the launch process and closes gRun"));
	}
	gtk_widget_show(btn);

	btn = gtk_button_new_with_label(gettext("Browse"));
	gtk_widget_set_usize(GTK_WIDGET(btn), 70, 24);
	gtk_fixed_put(GTK_FIXED (fix), btn, 165, 60);
	gtk_signal_connect(GTK_OBJECT (btn), "clicked", (GtkSignalFunc) browse, (gpointer) gdat);
	gtk_tooltips_set_tip(tips, btn, gettext("Browse for command"), gettext("The Browse button opens a file selection box so the user can browse for the command to launch"));
	gtk_widget_show(btn);

	gtk_widget_show(fix);
	gtk_window_set_policy(GTK_WINDOW (win), FALSE, FALSE, FALSE);
	gtk_widget_realize(win);
	
	gtk_window_set_icon(GTK_WINDOW(win), gdk_pixbuf_new_from_xpm_data(grun2));

	gtk_widget_show(win);
	gtk_main();
	return 0;
}
