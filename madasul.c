#define _POSIX_C_SOURCE 1
#include <stdio.h>
#define __USE_BSD
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/un.h>
#include <fcntl.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/wait.h>
#include <stdarg.h>

/* macros */
#define LENGTH(X)          (sizeof X / sizeof X[0])

#define READ               0
#define WRITE              1
#define ERR                2
#define BUF_SIZE           1024
#define TRACKS_BUF_SIZE	   128

/* enums */
enum { STOP, PAUSE, PLAYPAUSE, PLAY, NEXT, PREV, DIE, STATUS, SHRTSTAT, RND, LIST, SHRTLST, SETLIST, SHOWLIST, LOADLIST, CommandLast };
enum { INC, DEC, SET };
enum { IM_Dying, IM_Stopped, IM_Paused, IM_Playing, StateLast };


/* structs */
typedef struct track {
    int type;
    char* path;
	char* artist;
	char* album;
	char* title;
	char* date;
	char* genre;
	unsigned int number;
} track;

typedef struct handler {
	char* executable;
	int out;
} handler;

#include "config.h"

/* function declarations */
// helper
static void die(const char *errstr, ...);
static void sock_printf(int cmd_sock, const char *format, ...);
static void usage(void);
// TODO: malloc wrapper
// init
static int munchIn();
static int opensock();
// worker
static pid_t play(track* track, int* infp, int* outfp);
static int get_cmd(int *cmd_sock, char *val);
// player helper
static void scout(unsigned int whatdo, unsigned int val);
// threads
static void* listener();
static void* player();


/* variables */
// commands
char* ctrl_cmds[]    = { "stop", "pause", "playpause", "play", "next", "prev", "die", "status", "shrtstat", "random", "tracklist", "shorttracklist", "setlist", "showlist", "loadlist" };
static int socket_port     = 6666;
track** tracks;        // Array of pointers to track structs.
int ctrl_sock = -1;
int cur_track = 0;     // current track in track list
int cur_pl_track = 0;  // current track in playlist or tracklist. Only used by scout!
int num_tracks;
int num_pl_tracks;
int state = IM_Stopped;
int userlistprovided = 0;
pthread_mutex_t lock;
pid_t player_pid = -1;
int rnd = 0;
static const int debug = 0;
int* playlist = NULL;
char *listfile = NULL;


/* function definitions */
void die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void sock_printf(int cmd_sock, const char *format, ...) {
	char msg[BUF_SIZE];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, BUF_SIZE, format, ap);

	send(cmd_sock, msg, strlen(msg), MSG_NOSIGNAL);
	va_end(ap);
}

void usage() {
	fputs("madasul - evil media daemon\n", stderr);
	die("usage: madasul [-p port] [-f list file]\n");
}

int munchIn() {
    FILE *f;
    char type[10], path[BUF_SIZE], artist[BUF_SIZE], album[BUF_SIZE], title[BUF_SIZE], date[BUF_SIZE], genre[BUF_SIZE], number[BUF_SIZE];
    int i = 0, j = 0, k, len, n;

    if((tracks = calloc(sizeof(track*), TRACKS_BUF_SIZE)) == NULL)
        die("Lost memory on Mars Error: failed to allocate some memory\n");

    if(!(f = fopen(listfile, "r"))) {
        if(userlistprovided)
        	die("[TODO: find puny punch line!]: failed to open file %s\n", listfile);
        else return -1;
    }

    while(!feof(f)) {
		n = fscanf(f, "%10[^\t]\t%1023[^\t]\t%1023[^\t]\t%1023[^\t]\t%1023[^\t]\t%1023[^\t]\t%1023[^\t]\t%1023[^\n]\n", type, path, artist, album, title, date, genre, number);
		if(n!=2 && n!=8) {
			printf("Runaway Indian in the prairie of Spain Error: failed to read track line %d\n", i);
			continue;
		}

        if((tracks[i] = calloc(sizeof(track), /*len + */1)) == NULL)
	        die("Lost memory on Mars Error: failed to allocate some memory\n");

        len = strlen(path);
        if((tracks[i]->path = calloc(sizeof(char), len+1)) == NULL)
	        die("Lost memory on Mars Error: failed to allocate some memory\n");
        strncpy(tracks[i]->path, path, len);

        for(k=0; k<LENGTH(typenames); k++)
            if(strncmp(type, typenames[k], LENGTH(typenames[k]) || strncmp("*", typenames[k], 1))==0)
                tracks[i]->type = types[k];

		if(n==8) {
			len = strlen(artist);
			if((tracks[i]->artist = calloc(sizeof(char), len + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->artist, artist, len);
			len = strlen(album);
			if((tracks[i]->album = calloc(sizeof(char), len + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->album, album, len);
			len = strlen(title);
			if((tracks[i]->title = calloc(sizeof(char), len + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->title, title, len);
			len = strlen(date);
			if((tracks[i]->date = calloc(sizeof(char), len + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->date, date, len);
			len = strlen(genre);
			if((tracks[i]->genre = calloc(sizeof(char), len + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->genre, genre, len);
			tracks[i]->number = atoi(number);
		} else {
			tracks[i]->artist = NULL;
			tracks[i]->album = NULL;
			tracks[i]->title = NULL;
			tracks[i]->date = NULL;
			tracks[i]->genre = NULL;
			tracks[i]->number = 0;
		}

        i++;
        j++;
        if(j>=TRACKS_BUF_SIZE) {
            track **tmp = tracks;
            if((tracks = calloc(sizeof(track*), i + TRACKS_BUF_SIZE - 1)) == NULL)
		        die("Lost memory on Mars Error: failed to allocate some memory\n");
            memcpy(tracks, tmp, (i)*sizeof(track*));
            j = 0;
        }
    }

    if(ferror(f)) {
        free(tracks);
		die("[TODO: find another punchline]: failed to read from file\n");
        return -1;
    }
    fclose(f);

    return i;
}

int opensock() { // TODO: clean up
    int len;
    struct sockaddr_in sain;
    //struct hostent *server;

    if((ctrl_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("Bare feet in Nakatomi Tower Error: failed to open socket.\n");

	//flags = fcntl(ctrl_sock, F_GETFL, 0);
	//fcntl(ctrl_sock, SO_REUSEADDR, 0);

    //server = gethostbyname(SOCKET_ADRESS);
	sain.sin_addr.s_addr = INADDR_ANY;
    sain.sin_family = AF_INET;
    sain.sin_port = htons(socket_port);
    //bcopy((char *)server->h_addr, (char *)&sain.sin_addr.s_addr, server->h_length);
	//strcpy(sain.sin_addr, SOCKET_ADRESS);
    len = sizeof(sain.sin_family) + sizeof(sain);

/*
	flags = fcntl(ctrl_sock, F_GETFL, 0);
    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, SOCKET_ADDRESS);
    len = sizeof(saun.sun_family) + strlen(saun.sun_path);
*/

    if(bind(ctrl_sock, (struct sockaddr *)&sain, len)<0)
        die("Bare feet in Nakatomi Tower Error: failed to bind socket.\n");

	listen(ctrl_sock, 5);
    return(1);
}

pid_t play(track* track, int* infp, int* outfp) {
    int p_stdin[2], p_stdout[2], type = track->type;
    pid_t pid;
	static char pbuf[BUF_SIZE], spbuf[BUF_SIZE];
	char *p, *ps[16];
	int i = 0;

    if(pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        die("No Pizza in the Sewer Error: failed to open pipe\n");

    pid = fork();

    if(pid < 0)
        die("There is no Fork in Zion Error: failed to fork\n");
    else if (pid == 0) { //
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], handlers[type].out);
		strncpy(pbuf, handlers[type].executable, BUF_SIZE);

		p = strtok(pbuf," ");
		while(p!=NULL && i<15) {
	        snprintf(spbuf, BUF_SIZE, p, track->path);
			ps[i] = calloc(strlen(spbuf)+1, sizeof(char));
			strncpy(ps[i++], spbuf, strlen(spbuf));
			p = strtok(NULL, " ,");
		}
		ps[i] = NULL;

		execvp(ps[0], ps);
        // we should not be here, it means the exec did not work...
		die("[TODO]: failed to exec\n");
    }

    if(infp==NULL)
        close(p_stdin[WRITE]);
    else
        *infp = p_stdin[WRITE];

    if(outfp==NULL)
        close(p_stdout[READ]);
    else
        *outfp = p_stdout[READ];

    return pid;
}

int get_cmd(int *cmd_sock, char *val) {
	struct sockaddr_un cli_addr;
	socklen_t clilen;
	char buf[BUF_SIZE];
	int i, n, len, cmd = -1;


	clilen = sizeof(cli_addr);
	if((*cmd_sock = accept(ctrl_sock, (struct sockaddr *)&cli_addr, &clilen)) < 0)
		die("Bare feet in Nakatomi Tower Error: failed to open a socket to listen\n");
	*buf = 0;
	n = read(*cmd_sock, buf, BUF_SIZE);
	buf[n] = 0;
	*val = 0;

	if(debug)
		printf("command: %s\n", buf);

	if(n>0) {
		for(i=0; i<LENGTH(ctrl_cmds); i++) {
			len = strlen(ctrl_cmds[i]);
			if(strncmp(ctrl_cmds[i], buf, len)==0) {
				cmd = i;
				break;
			}
		}
		if(n-len-2>0) {
			strncpy(val, buf+len+1, n-len-2);
			val[n-len-2] = 0;
		}
	}

	return cmd;
}

// you know, like track selection...
void scout(unsigned int whatdo, unsigned int val) {
	int nt = playlist==NULL ? num_tracks: num_pl_tracks;

	if(num_tracks<1)
		return;

	pthread_mutex_lock(&lock);
	switch(whatdo) {
		case INC:
			if(rnd) {
				cur_pl_track = random(); // valid because % num_tracks later. Haha!
			} else
				cur_pl_track += val;
			break;
		case DEC:
			if(rnd) {
				cur_pl_track = random(); // valid because % num_tracks later. Haha!
			} else
				cur_pl_track -= val;
			break;
		case SET:
			cur_pl_track = val;
			break;
	}
	if(cur_pl_track<0) cur_pl_track = nt - cur_pl_track;
	if(cur_pl_track>=nt) cur_pl_track = cur_pl_track % nt;
	pthread_mutex_unlock(&lock);
	cur_track = playlist==NULL ? cur_pl_track : playlist[cur_pl_track];
}

void* listener() {
	char val[BUF_SIZE], *buf, abuf[20];
	int cmd, cmd_sock, i, ii, iii, n;

	while(1) {
		if((cmd=get_cmd(&cmd_sock, val))>=0) {
			switch(cmd) {
				case STOP:
					if(player_pid>0)
						kill(player_pid, SIGKILL);
					state = IM_Stopped;
					sock_printf(cmd_sock, "OK\n");
					break;
				case PAUSE:
					if(state>IM_Stopped && player_pid>0) {// Just to never come to kill(0) or stuff...
						if(state>IM_Paused) {
							kill(player_pid, 19);
							state = IM_Paused;
						} else {
							kill(player_pid, 18);
							state = IM_Playing;
						}
					}
					sock_printf(cmd_sock, "OK\n");
					break;
				case PLAYPAUSE:
					if(player_pid>0 && state>IM_Paused && !strlen(val)) {
						kill(player_pid, 19);
						state = StateLast;
					}
				case PLAY:
					if(num_tracks<1) {
						sock_printf(cmd_sock, "No tracklist loaded\n");
						break;
					}
					if(strlen(val)) {
						// if a value is given, always play that song!
						i = atoi(val);
						if(player_pid>0)
							kill(player_pid, SIGKILL);
						scout(SET, i);
						sock_printf(cmd_sock, "OK\n");
					}
					// unpause if no value is given
					if(state==IM_Paused && player_pid>0) {
						kill(player_pid, 18);
						state = IM_Playing;
					} else if(state==StateLast)
						state = IM_Paused;
					// always set state running, resumes from stop
					if(state<IM_Paused)
						state = IM_Playing;
					break;
				case NEXT:
					if(num_tracks<1) {
						sock_printf(cmd_sock, "No tracklist loaded\n");
						break;
					}
					if(strlen(val)) i = atoi(val);
					else i = 1;
					if(i>0) {
						if(player_pid>0)
							kill(player_pid, SIGKILL);
						scout(INC, i);
						sock_printf(cmd_sock, "OK\n");
					} else
						sock_printf(cmd_sock, "bad value\n");
					state = IM_Playing;
					break;
				case PREV:
					if(num_tracks<1) {
						sock_printf(cmd_sock, "No tracklist loaded\n");
						break;
					}
					if(strlen(val)) i = atoi(val);
					else i = 1;
					if(i>0) {
						if(player_pid>0)
							kill(player_pid, SIGKILL);
						scout(DEC, i);
						sock_printf(cmd_sock, "OK\n");
					} else
						sock_printf(cmd_sock, "bad value\n");
					state = IM_Playing;
					break;
				case DIE:
					if(player_pid>0)
						kill(player_pid, SIGKILL);
					state = IM_Playing;
					sock_printf(cmd_sock, "OK\n");
					break;
				case STATUS:
					if(num_tracks<1) {
						sock_printf(cmd_sock, "No tracklist loaded\n");
						break;
					}
					sock_printf(cmd_sock, "Track (%d of %d): %s\nGenre: %s\nArtist: %s\nAlbum: %s\nTrack: [%d] %s\n",
						cur_track, num_tracks, tracks[cur_track]->path,
						tracks[cur_track]->genre,
						tracks[cur_track]->artist,
						tracks[cur_track]->album,
						tracks[cur_track]->number, tracks[cur_track]->title);
					break;
				case SHRTSTAT:
					if(num_tracks<1)
						break;
					sock_printf(cmd_sock, "%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%d\t%s\n",
						cur_track, num_tracks,
						state, rnd,
						tracks[cur_track]->path,
						tracks[cur_track]->genre,
						tracks[cur_track]->artist,
						tracks[cur_track]->album,
						tracks[cur_track]->number, tracks[cur_track]->title);
					break;
				case RND:
					if(strlen(val)) i = atoi(val);
					else i = !rnd;
					if(i>=0 && i<=1) {
						rnd = i;
						sock_printf(cmd_sock, "OK\n", cur_track, tracks[cur_track]->path);
					} else
						sock_printf(cmd_sock, "bad value\n", cur_track, tracks[cur_track]->path);
					break;
				case LIST:
					if(num_tracks<1) {
						sock_printf(cmd_sock, "No tracklist loaded\n");
						break;
					}
					for(i=0; i<num_tracks; i++)
						sock_printf(cmd_sock, "%d:\t%s\n""\t[%s] (%s - %s - %d) %s\n",
							i, tracks[i]->path,
							tracks[i]->genre,
							tracks[i]->artist,
							tracks[i]->album,
							tracks[i]->number,
							tracks[i]->title);
					break;
				case SHRTLST:
					for(i=0; i<num_tracks; i++)
						sock_printf(cmd_sock, "%d\t%s\t%s\t%s\t%s\t%d\t%s\t\n",
							i, tracks[i]->path,
							tracks[i]->genre,
							tracks[i]->artist,
							tracks[i]->album,
							tracks[i]->number,
							tracks[i]->title);
					break;
				case SETLIST:
					if(num_tracks<1) {
						sock_printf(cmd_sock, "No tracklist loaded\n");
						break;
					}
					i = ii = iii = 0;
					n = strlen(val);
					if(playlist!=NULL)
						free(playlist);
					playlist = calloc(sizeof(int), TRACKS_BUF_SIZE);
					do {
						buf = val;
						while(*buf!=0) {
							if(*buf==',') {
								abuf[i] = 0;
								playlist[ii++] = atoi(abuf);
								iii++;
								if(iii>TRACKS_BUF_SIZE) {
									playlist = realloc(playlist, sizeof(int) * (ii + TRACKS_BUF_SIZE));
									iii = 0;
								}
								i = 0;
							} else if(*buf!=' ') {
								if(i<20)
									abuf[i++] = *buf;
								else {
									sock_printf(cmd_sock, "invalid list\n");
									close(cmd_sock);
									return NULL; // TODO: Is this a good idea?
								}
							}
							buf++;
						}
					} while((n = read(cmd_sock, val, BUF_SIZE))>0);
					num_pl_tracks = ii + 1;
					scout(SET,0);
					break;
				case SHOWLIST:
					if(num_tracks<1) {
						sock_printf(cmd_sock, "No tracklist loaded\n");
						break;
					}
					for(i=0; i<num_pl_tracks; i++)
						sock_printf(cmd_sock, "%d (%d): %s\n", i, playlist[i], tracks[playlist[i]]->title);
					break;
				case LOADLIST:
						free(tracks);
						free(playlist);
						num_pl_tracks = num_tracks = 0;
						i = strlen(val);
						if(i>0) {
							if(listfile)
								free(listfile);
							listfile = calloc(i+1, sizeof(char));
							strncpy(listfile, val, i);
							num_tracks = munchIn();
							if(num_tracks>0)
								sock_printf(cmd_sock, "yay, i munched %d lines\n", num_tracks);
							else if(num_tracks==0)
								sock_printf(cmd_sock, "awww, nothing found in your list\n");
							else
								sock_printf(cmd_sock, "awww, file not found %d\n", num_tracks);
						} else
							sock_printf(cmd_sock, "no file specified\n");
					break;
				default:
					sock_printf(cmd_sock, "unknown command\n");
					break;
			}
		} else {
			sock_printf(cmd_sock, "invalid command\n");
		}
		close(cmd_sock);
	}

	return NULL;
}

void* player() {
    int infp, /*w, */outfp, stat;

	if(player_pid>=0)
		die("Heile Welt at Sehbuehl Error: pid is already in use\n");

	if(num_tracks<1)
		return NULL;

	pthread_mutex_lock(&lock);
	player_pid = play(tracks[cur_track], &infp, &outfp);
	pthread_mutex_unlock(&lock);

	waitpid(player_pid, &stat, 0);
	if(WIFEXITED(stat)!=0)
		scout(INC, 1);

	player_pid = -1;

	close(infp);
	close(outfp);

	return NULL;
}

int main(int argc, char *argv[]) {
	pthread_t p_player, p_listener;
    pid_t pid;
	int i, l;


	for(i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0' && argv[i][2] == '\0'; i++) {
		if(!strcmp(argv[i], "--")) {
			i++;
			break;
		}
		switch(argv[i][1]) {
			case 'p':
				if(++i < argc)
					socket_port = atoi(argv[i]);
				else
					usage();
				break;
			case 'f':
				if(++i < argc) {
					l = strlen(argv[i]);
					listfile = calloc(sizeof(char), l+1);
					strncpy(listfile, argv[i], l);
					userlistprovided = 1;
				} else
					usage();
				break;
		}
	}

	if(listfile==NULL) {
		l = strlen(getenv("XDG_CONFIG_HOME"));
		listfile = calloc(sizeof(char), l+15);
		snprintf(listfile, l+15, "%s/madasul/list", getenv("XDG_CONFIG_HOME"));
	}

	srandom((unsigned int)time(NULL));
	num_tracks = munchIn();
	if(num_tracks>0)
		printf("yay, i munched %d lines\n", num_tracks);
	else if(num_tracks==0)
		printf("awww, nothing found in your list\n");
	else
		printf("awww, file not found\n");
	opensock();
	printf("and my socket is open!\n");

    pid = fork();

    if(pid < 0)
        die("There is no Fork in Zion Error: failed to fork\n");
    else if (pid != 0)
    	return 0;

	pthread_mutex_init(&lock, NULL);

	pthread_create(&p_listener, NULL, listener, NULL);
	state = num_tracks>0 ? IM_Playing : IM_Stopped;
	while(state>IM_Dying) {
		if(state==IM_Playing) {
			pthread_create(&p_player, NULL, player, NULL);
			pthread_join(p_player, NULL);
		}
		sleep(1); // Stop going crazy option
	}

    return 0;
}
