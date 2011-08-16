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
#include "config.h"

/* macros */
#define LENGTH(X)          (sizeof X / sizeof X[0])

#define READ               0
#define WRITE              1
#define ERR                2


/* enums */
enum { STOP, PAUSE, PLAYPAUSE, PLAY, NEXT, PREV, DIE, STATUS, SHRTSTAT, RND, LIST, SHRTLST, SETLIST, CommandLast };
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


/* function declarations */
// helper
static void die(const char *errstr, ...);
static void sock_printf(int cmd_sock, const char *format, ...);
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
char* ctrl_cmds[]    = { "stop", "pause", "playpause", "play", "next", "prev", "die", "status", "shrtstat", "random", "tracklist", "shorttracklist", "setlist" };

track** tracks;        // Array of pointers to track structs.
int ctrl_sock = -1;
int cur_track = 0;     // current track in track list
int cur_pl_track = 0;  // current track in playlist or tracklist. Only used by scout!
int num_tracks;
int num_pl_tracks;
int state = IM_Stopped;
pthread_mutex_t lock;
pid_t player_pid = -1;
int rnd = 0;
static const int debug = 0;
int* playlist = NULL;


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

	//if(debug)
	//	printf("to socket: %s\n", msg);

	send(cmd_sock, msg, strlen(msg), MSG_NOSIGNAL);
	va_end(ap);
}

int munchIn() {
    char type[10], path[BUF_SIZE], artist[BUF_SIZE], album[BUF_SIZE], title[BUF_SIZE], date[BUF_SIZE], genre[BUF_SIZE], number[BUF_SIZE];
    int i = 0, j = 0, k, len, n;

    if((tracks = calloc(sizeof(track*), TRACKS_BUF_SIZE)) == NULL)
        die("Lost memory on Mars Error: failed to allocate some memory\n");

    while(!feof(stdin)) {
		n = fscanf(stdin, "%[^\t]\t%[^\t]\t%[^\t]\t%[^\t]\t%[^\t]\t%[^\t]\t%[^\t]\t%[^\n]\n", type, path, artist, album, title, date, genre, number);
//printf("%d, %d:%d, %s, %s, %s, %s\n", n,i,j,path,artist,album,title);
		if(n!=2 && n!=8) {
			printf("Runaway Indian in the prairie of Spain Error: failed to read track line %d\n", i);
			continue;
		}

        if((tracks[i] = calloc(sizeof(track), /*len + */1)) == NULL)
	        die("Lost memory on Mars Error: failed to allocate some memory\n");

        len = strlen(path);
        if((tracks[i]->path = calloc(sizeof(char), len)) == NULL)
	        die("Lost memory on Mars Error: failed to allocate some memory\n");
        strncpy(tracks[i]->path, path, len);

        for(k=0; k<LENGTH(typenames); k++)
            if(strncmp(type, typenames[k], LENGTH(typenames[k]))==0)
                tracks[i]->type = types[k];

		if(n==8) {
			len = strlen(artist);
			if((tracks[i]->artist = calloc(sizeof(char), len)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->artist, artist, len);
			len = strlen(album);
			if((tracks[i]->album = calloc(sizeof(char), strlen(album) + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->album, album, len);
			len = strlen(title);
			if((tracks[i]->title = calloc(sizeof(char), strlen(title) + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->title, title, len);
			len = strlen(date);
			if((tracks[i]->date = calloc(sizeof(char), strlen(date) + 1)) == NULL)
				die("Lost memory on Mars Error: failed to allocate some memory\n");
	        strncpy(tracks[i]->date, date, len);
			len = strlen(genre);
			if((tracks[i]->genre = calloc(sizeof(char), strlen(genre) + 1)) == NULL)
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

    if(ferror(stdin)) {
        free(tracks);
        perror("Error reading from stdin.");
        return -1;
    }

    return i;
}

int opensock() {
    int len;
    struct sockaddr_in sain;
    //struct hostent *server;

    if((ctrl_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("Bare feet in Nakatomi Tower Error: failed to open socket.\n");

	//flags = fcntl(ctrl_sock, F_GETFL, 0);

    //server = gethostbyname(SOCKET_ADRESS);
	sain.sin_addr.s_addr = INADDR_ANY;
    sain.sin_family = AF_INET;
    sain.sin_port = htons(SOCKET_PORT);
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
	char path[BUF_SIZE+2];

    if(pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        die("No Pizza in the Sewer Error: failed to open pipe\n");

    pid = fork();

    if(pid < 0)
        die("There is no Fork in Zion Error: failed to fork\n");
    else if (pid == 0) { // 
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], ERR);
        snprintf(path, BUF_SIZE+2, "\"%s\"", track->path);
		execl(play_cmds[type][0], play_cmds[type][1], track->path, play_cmds[type][2], NULL);
        // we should not be here, it means the exec did not work...
		die("Conduction error in Cold Mountain: failed to exec\n");
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

	//sock_printf(*cmd_sock, "There is only madasul\n");

	n = read(*cmd_sock, buf, BUF_SIZE);
	buf[n] = 0;
	val[0] = 0;

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
		if(n>len)
			strncpy(val, buf+len+1, n-len-1);
	}

	return cmd;
}

// you know, like track selection...
void scout(unsigned int whatdo, unsigned int val) {
	int nt = playlist==NULL ? num_tracks: num_pl_tracks;
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
					sock_printf(cmd_sock, "Track (%d of %d): %s\nGenre: %s\nArtist: %s\nAlbum: %s\nTrack: [%d] %s\n",
						cur_track, num_tracks, tracks[cur_track]->path,
						tracks[cur_track]->genre,
						tracks[cur_track]->artist,
						tracks[cur_track]->album,
						tracks[cur_track]->number, tracks[cur_track]->title);
					break;
				case SHRTSTAT:
					sock_printf(cmd_sock, "%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%d\t%s\n",
						cur_track, num_tracks,
						state/*state==2 ? 0 : (pauseed ? 2 : 1)*/, rnd,
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
					for(i=0; i<num_tracks; i++)
						sock_printf(cmd_sock, "%d:file: %s\n", i, tracks[i]->path);
					sock_printf(cmd_sock, "OK\n", cur_track, tracks[cur_track]->path);
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
					sock_printf(cmd_sock, "OK\n", cur_track, tracks[cur_track]->path);
					break;
				case SETLIST:
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
									//o = playlist;
									playlist = realloc(playlist, sizeof(int) * (ii + TRACKS_BUF_SIZE));
									//memcpy(playlist, o, sizeof(int)*iii);
									iii = 0;
								}
								i = 0;
							} else if(*buf!=' ') {
								if(i<20)
									abuf[i++] = *buf;
								else {
									sock_printf(cmd_sock, "invalid list\n");
									close(cmd_sock);
									return NULL; // I do not think that that is good idea!
								}
							}
							buf++;
						}
					} while((n = read(cmd_sock, val, BUF_SIZE))>0);
					num_pl_tracks = ii + 1;
					scout(SET,0);
					//for(i=0; i<num_pl_tracks; i++)
						//printf("%d: %d\n", i, playlist[i]);
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
	//FILE* fp;
    int infp, /*w, */outfp, stat;
    //char line[BUF_SIZE];

	if(player_pid>=0)
		die("Heile Welt at Sehbuehl Error: pid is already in use\n");

	pthread_mutex_lock(&lock);
	player_pid = play(tracks[cur_track], &infp, &outfp);
	pthread_mutex_unlock(&lock);

	/*w = */waitpid(player_pid, &stat, 0);
	if(WIFEXITED(stat)!=0) // WEXITSTATUS(stat_val)
		scout(INC, 1);

	player_pid = -1;

	close(infp);
	close(outfp);

	return NULL;
}

int main(/*int argc, char *argv[]*/) {
	pthread_t p_player, p_listener;

	srandom((unsigned int)time(NULL));
	num_tracks = munchIn();
	printf("yay, i munched %d lines\n", num_tracks);
	opensock();
	printf("and my socket is open!\n");

	pthread_mutex_init(&lock, NULL);

	pthread_create(&p_listener, NULL, listener, NULL);
	state = IM_Playing;
	while(state>IM_Dying) {
		if(state==IM_Playing) {
			pthread_create(&p_player, NULL, player, NULL);
			pthread_join(p_player, NULL);
		}
		sleep(1); // Stop going crazy option
	}

    return 0;
}
