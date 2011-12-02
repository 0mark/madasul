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


/*  */
#define ALLOC_ERR "Error: calloc failed\n"
#define OPEN_FILE_ERR "Error: failed to open file %s\n"
#define OPEN_SOCKET_ERR "Error: failed to socket %d\n"

#define HANDLER_FORMAT_WARN "Warning: error in handler list in line %d\n"
#define FTYPES_COUNT_WARN "Warning: too many filetypes\n"
#define HANDLER_COUNT_WARN "Warning: too many handler\n"
#define OPEN_FILE_WARN "Warning: failed to open file %s\n"
#define LIB_LINE_FAIL_WARN "Warning: error reading line %d\n"

#define READ               0
#define WRITE              1
#define ERR                2
#define BUF_SIZE           1024
#define TRACKS_BUF_SIZE	   128


/* macros */
#define LENGTH(X)                  (sizeof X / sizeof X[0])
#define NEED_LIB(cmd)              (cmd==PLAYPAUSE || cmd==PLAY || cmd==NEXT || cmd==PREV || cmd==STATUS || cmd==SHRTSTAT || cmd==SHOWLIB || cmd==SHRTLIB || cmd==SETLIST || cmd==SHOWLIST)
#define STOPPLAYER                 if(player_pid>0) kill(player_pid, SIGKILL);
#define XALLOC(target, type, size) if((target = calloc(sizeof(type), size)) == NULL) die(ALLOC_ERR)


/* enums */
enum { STOP, PAUSE, PLAYPAUSE, PLAY, NEXT, PREV, RANDOM,
       DIE, STATUS, LOADHANDLER,
       LOADLIB, SHOWLIB,
       SETLIST, SHOWLIST,
       SHRTSTAT, SHRTLIB, CommandLast };
enum { INC, DEC, SET };
enum { IM_Dying, IM_Stopped, IM_Paused, IM_Playing, StateLast };


/* structs */
typedef struct file_t {
	int type;
	char* path;
	char* artist;
	char* album;
	char* title;
	char* date;
	char* genre;
	unsigned int number;
} file_t;

typedef struct handler_t {
	char* executable;
	int out;
} handler_t;

//#include "config.h"

/* function declarations */
// helper
static void die(const char *errstr, ...);
static void sock_printf(int cmd_sock, const char *format, ...);
static void usage(void);
// TODO: malloc wrapper
// init
static int load_lib(char* file);
static int load_handler(char* file);
static int opensock();
// worker
static pid_t play(file_t* track, int* infp, int* outfp);
static int get_cmd(int *cmd_sock, char *val);
// player helper
static void scout(unsigned int whatdo, unsigned int val);
// threads
static void* listener();
static void* player();


/* variables */
static int socket_port = 6666;
int ctrl_sock = -1;

file_t** library; // list of files, including metadata
handler_t** handlers; // list of file handlers
int* playlist = NULL;

int cur_track = 0; // current track in library
int cur_pl_track = 0; // current track in playlist
int num_tracks; // number of tracks in playlist
int num_pl_tracks; // number of tracks in playlist

int state = IM_Stopped; // state, one of IM_Dying, IM_Stopped, IM_Paused, IM_Playing, StateLast
int rnd = 0; // state of random

pthread_mutex_t lock;
pid_t player_pid = -1;

static const int debug = 0;

char** typenames; // list of strings naming file types (eg. ogg, mp3, ...)
int* types; // for every typename a number, indicating the correspondig handler
int num_types; // number of loaded file handlers

char* ctrl_cmds[] = { "stop", "pause", "playpause", "play", "next", "prev", "random",
                      "die", "status", "loadhandler",
                      "loadlib", "showlib",
                      "setlist", "showlist",
                      "shrtstat", "shrtlib" }; // help, loadhandler, ...


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
	die("usage: madasul [-p port] [-f library file] [-h handler file]\n");
}

char *nxt(char c, char *s) {
	char *b = s;
	while(*b && *b!=c) b++;
	if(b!=s && *b==c) {
		*b = 0;
		return ++b;
	} else return NULL;
}

int load_handler(char* file) {
	FILE *f;
	char *b, *s, *parts[3];
	int hc = 0, tc = 0, len, i, l = 0, cont = 0;

	XALLOC(b, char, BUF_SIZE);
	XALLOC(typenames, char, TRACKS_BUF_SIZE);
	XALLOC(types, char, TRACKS_BUF_SIZE);
	XALLOC(handlers, handler_t, TRACKS_BUF_SIZE);
    
	if(!(f = fopen(file, "r"))) {
		return -1;
	}

	while(!feof(f)) {
		fgets(b, BUF_SIZE, f);
		l++;
		if(*b == '#' || *b=='\n' || !*b) continue;

		// split string
		for(i = 0; i<3; i++) {
			parts[i] = b;
			if(!(b=nxt(i==2?'\n':',', b))) {
				printf(HANDLER_FORMAT_WARN, l);
				cont = 1;
				break;
			}
		}
		if(cont) {
			cont = 0;
			continue;
		}

		// typenames
		b = parts[0];
		i = 1;
		while(i) {
			s = b;
			while(*b && *b!=' ') b++;
			if(!*b) i = 0;
			*b = 0;
			len = strlen(s);
			XALLOC(typenames[tc], char, len);
			strncpy(typenames[tc], s, len);
			types[tc] = hc;
			b++;
			tc++;
			if(tc>TRACKS_BUF_SIZE) {
				printf(FTYPES_COUNT_WARN);
				return tc;
			}
		}

		XALLOC(handlers[hc], handler_t, 1);

		// output
		handlers[hc]->out = atoi(parts[1]);

		// executable
		len = strlen(parts[2]);
		XALLOC(handlers[hc]->executable, char, len);
		strncpy(handlers[hc]->executable, parts[2], len);

		hc++;
		if(hc>TRACKS_BUF_SIZE) {
			printf(HANDLER_COUNT_WARN);
			return tc;
		}
	}

	return tc;
}

int load_lib(char* file) {
    FILE *f;
    char *buf, *bufs[8], *b, *c;
    int i = 0, j = 0, l = 0, k, len, n;

    XALLOC(buf, char, BUF_SIZE);
	XALLOC(library, file_t, TRACKS_BUF_SIZE);

    if(!(f = fopen(file, "r"))) {
		return -1;
    }

    while(!feof(f) && !ferror(f)) {
    	fgets(buf, BUF_SIZE, f);
		l++;
		n = 0;
		b = strtok(buf, "\t");
		while(b!=NULL && n<8) {
			bufs[n++] = b;
			b = strtok(NULL, "\t");
		}

		if(n<2) {
			printf(LIB_LINE_FAIL_WARN, l);
			continue;
		}

        XALLOC(library[i], file_t, 1);

        for(k=0; k<num_types; k++) {
            if(strncmp(bufs[0], typenames[k], strlen(typenames[k]))==0 || strncmp("*", typenames[k], 1)==0) {
                library[i]->type = types[k];
            }
        }

		for(k=1; n-k>0 && k<7; k++) {
			len = strlen(bufs[k]);
			XALLOC(c, char, len + 1);
			strncpy(c, bufs[k], len);
			switch(k) { // TODO: i do not want this switch!
				case 1:	library[i]->path=c; break;
				case 2:	library[i]->artist=c; break;
				case 3:	library[i]->album=c; break;
				case 4:	library[i]->title=c; break;
				case 5:	library[i]->date=c; break;
				case 6:	library[i]->genre=c; break;
			}
		}
		if(n==7)
			library[i]->number = atoi(bufs[7]);

        i++;
        j++;
        if(j>=TRACKS_BUF_SIZE) {
            file_t **tmp = library;
            XALLOC(library, file_t, i + TRACKS_BUF_SIZE - 1);
            memcpy(library, tmp, (i)*sizeof(file_t*));
            j = 0;
        }
    }

    /*if(ferror(f)) {
        free(library);
		die("[TODO: find another punchline]: failed to read from file\n");
        return -1;
    }*/

    fclose(f);

    return i;
}

int opensock() { // TODO: clean up
    int len;
    struct sockaddr_in sain;
    //struct hostent *server;

    if((ctrl_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die(OPEN_SOCKET_ERR, socket_port);

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
        die("Error: failed to bind socket.\n");

	listen(ctrl_sock, 5);
    return(1);
}

pid_t play(file_t* track, int* infp, int* outfp) {
    int p_stdin[2], p_stdout[2], type = track->type;
    pid_t pid;
	static char pbuf[BUF_SIZE], spbuf[BUF_SIZE];
	char *p, *ps[16];
	int i = 0;

    if(pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        die("Error: failed to open pipe\n");

    pid = fork();

    if(pid < 0)
        die("Error: failed to fork\n");
    else if (pid == 0) { //
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], handlers[type]->out);
		strncpy(pbuf, handlers[type]->executable, BUF_SIZE);

		p = strtok(pbuf," ");
		while(p!=NULL && i<15) {
	        snprintf(spbuf, BUF_SIZE, p, track->path);
			// TODO: I gues its not wise to alloc it again and again without ever freeing it!
			XALLOC(ps[i], char, strlen(spbuf)+1);
			//ps[i] = calloc(strlen(spbuf)+1, sizeof(char));
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
		die(OPEN_SOCKET_ERR, socket_port);
	*buf = 0;
	n = read(*cmd_sock, buf, BUF_SIZE);
	buf[n] = 0;
	*val = 0;

	if(debug)
		printf("command line: %s\n", buf);

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

	if(debug)
		printf("command: %d\n", cmd);

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
			
			if(NEED_LIB(cmd) && num_tracks<1) {
				sock_printf(cmd_sock, "No files in library\n");
				continue;
			}
			switch(cmd) {
				case STOP:
					STOPPLAYER
					state = IM_Stopped;
					sock_printf(cmd_sock, "OK\n");
					break;
				case PAUSE:
					if(state>IM_Stopped && player_pid>0) {
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
					if(player_pid>0 && state>IM_Paused && !strlen(val) && num_tracks>0) {
						kill(player_pid, 19);
						// Undefined State
						state = StateLast;
					}
				case PLAY:
					if(num_tracks<1) {
					}
					if(strlen(val)) {
						// if a value is given, always play that song!
						i = atoi(val);
						STOPPLAYER
						scout(SET, i);
					}
					// unpause
					if(state==IM_Paused && player_pid>0) {
						kill(player_pid, 18);
						state = IM_Playing;
					} else if(state==StateLast)
						state = IM_Paused;
					// set state running when not paused (resume from stop)
					if(state<IM_Paused)
						state = IM_Playing;
					sock_printf(cmd_sock, "OK\n");
					break;
				case NEXT:
				case PREV:
					if(strlen(val)) i = atoi(val);
					else i = 1;
					STOPPLAYER
					scout(cmd==NEXT?INC:DEC, i);
					sock_printf(cmd_sock, "OK\n");
					state = IM_Playing;
					break;
				case DIE:
					STOPPLAYER
					state = IM_Dying;
					sock_printf(cmd_sock, "OK\n");
					break;
				case STATUS:
					sock_printf(cmd_sock, "Track (%d of %d): %s\nGenre: %s\nArtist: %s\nAlbum: %s\nTrack: [%d] %s\n",
                                          cur_track, num_tracks, library[cur_track]->path,
					                      library[cur_track]->genre,
					                      library[cur_track]->artist,
					                      library[cur_track]->album,
					                      library[cur_track]->number, library[cur_track]->title);
					break;
				case LOADHANDLER:
						free(handlers);
						free(typenames);
						free(types);
						num_types = 0;
						i = strlen(val);
						if(i>0) {
							num_types = load_handler(val);
							if(num_types>0)
								sock_printf(cmd_sock, "Loaded %d file handlers.\n", num_types);
							else if(num_types==0)
								sock_printf(cmd_sock, "No valid file handlers found.\n");
							else
								sock_printf(cmd_sock, "File not found\n");
						} else
							sock_printf(cmd_sock, "No file specified\n");
					break;
				case SHRTSTAT:
					if(num_tracks<1)
						break;
					sock_printf(cmd_sock, "%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\t%d\t%s\n",
						cur_track, num_tracks,
						state, rnd,
						library[cur_track]->path,
						library[cur_track]->genre,
						library[cur_track]->artist,
						library[cur_track]->album,
						library[cur_track]->number, library[cur_track]->title);
					break;
				case RANDOM:
					if(strlen(val)) i = atoi(val);
					else i = !rnd;
					if(i>=0 && i<=1) {
						rnd = i;
						sock_printf(cmd_sock, "OK\n", cur_track, library[cur_track]->path);
					} else
						sock_printf(cmd_sock, "bad value\n", cur_track, library[cur_track]->path);
					break;
				case SHOWLIB:
					for(i=0; i<num_tracks; i++)
						sock_printf(cmd_sock, "%d:\t%s\n""\t[%s] (%s - %s - %d) %s\n",
							i, library[i]->path,
							library[i]->genre,
							library[i]->artist,
							library[i]->album,
							library[i]->number,
							library[i]->title);
					break;
				case SHRTLIB:
					for(i=0; i<num_tracks; i++)
						sock_printf(cmd_sock, "%d\t%s\t%s\t%s\t%s\t%d\t%s\t\n",
							i, library[i]->path,
							library[i]->genre,
							library[i]->artist,
							library[i]->album,
							library[i]->number,
							library[i]->title);
					break;
				case SETLIST:
					i = ii = iii = 0;
					n = strlen(val);
					if(playlist!=NULL)
						free(playlist);
					XALLOC(playlist, int, TRACKS_BUF_SIZE);
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
									sock_printf(cmd_sock, "Invalid list\n");
									close(cmd_sock);
									//return NULL; // TODO: Is this a good idea?
									break;
								}
							}
							buf++;
						}
					} while((n = read(cmd_sock, val, BUF_SIZE))>0);
					num_pl_tracks = ii + 1;
					scout(SET,0);
					break;
				case SHOWLIST:
					for(i=0; i<num_pl_tracks; i++)
						sock_printf(cmd_sock, "%d (%d): %s\n", i, playlist[i], library[playlist[i]]->title);
					break;
				case LOADLIB:
					if(num_types<1) {
						sock_printf(cmd_sock, "No file handlers loaded\n");
						break;
					}
					free(library);
					free(playlist);
					num_pl_tracks = num_tracks = 0;
					i = strlen(val);
					if(i>0) {
						num_tracks = load_lib(val);
						if(num_tracks>0)
							sock_printf(cmd_sock, "Loaded %d files to library\n", num_tracks);
						else if(num_tracks==0)
							sock_printf(cmd_sock, "Empty File\n");
						else
							sock_printf(cmd_sock, "File not found\n");
					} else
						sock_printf(cmd_sock, "No file specified\n");
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
    int infp, outfp, stat;

	if(player_pid>=0)
		die("Error: pid is already in use\n");

	if(num_tracks<1)
		return NULL;

	pthread_mutex_lock(&lock);
	player_pid = play(library[cur_track], &infp, &outfp);
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
	char *listfile = NULL, *handlerfile = NULL, *listpath = "%s/madasul/list", *handlerpath = "%s/madasul/handler";
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
					listfile = argv[i];
				} else
					usage();
				break;
			case 'h':
				if(++i < argc) {
					handlerfile = argv[i];
				} else
					usage();
				break;
		}
	}

	if(handlerfile==NULL) {
		l = strlen(getenv("XDG_CONFIG_HOME")) + strlen(handlerpath);
		XALLOC(handlerfile, char, l);
		snprintf(handlerfile, l, handlerpath, getenv("XDG_CONFIG_HOME"));
		num_types = load_handler(handlerfile);
	} else {
		if((num_types=load_handler(handlerfile))<0)
			die(OPEN_FILE_ERR, handlerfile);
	}
	if(num_types>=0) printf("loaded %d file type handler\n", num_types);

	if(listfile==NULL) {
		l = strlen(getenv("XDG_CONFIG_HOME")) + strlen(listpath);
		XALLOC(listfile, char, l);
		snprintf(listfile, l, listpath, getenv("XDG_CONFIG_HOME"));
		num_tracks = load_lib(listfile);
	} else {
		if((num_tracks=load_lib(listfile))<0)
			die(OPEN_FILE_ERR, listfile);
	}
	if(num_tracks>=0) printf("loaded %d files from list\n", num_tracks);

	srandom((unsigned int)time(NULL));

	opensock();
	printf("Listening on Socket %d.\n", socket_port);

    pid = fork();
    if(pid < 0)
        die("Error: failed to fork\n");
    else if(pid != 0) {
    	printf("forked to pid %d\n", pid);
    	return 0;
    }

	pthread_mutex_init(&lock, NULL);
	pthread_create(&p_listener, NULL, listener, NULL);

	state = num_tracks>0 ? IM_Playing : IM_Stopped;
	while(state>IM_Dying) {
		if(state==IM_Playing) {
			pthread_create(&p_player, NULL, player, NULL);
			pthread_join(p_player, NULL); // waits for player to stop...
		}
		sleep(1); // Stop going crazy option
	}

    return 0;
}
