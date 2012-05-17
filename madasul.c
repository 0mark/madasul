/*
 *
 */

/* includes */
#define _POSIX_C_SOURCE 1
#include <stdio.h>
#define __USE_BSD
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <sys/un.h>
#include <fcntl.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/wait.h>
#include <stdarg.h>

/* constants */
#define OPEN_FILE_ERR      "Error: failed to open file %s\n"
#define READ               0
#define WRITE              1
#define ERR                2
#define BUF_SIZE           1024
#define TRACKS_BUF_SIZE	   128
#define DEFAULT_STATUS     "Track (#c of ##): $p\nGenre: $g\nArtist: $a\nAlbum: $l\nTrack: [#n] $t\n"
#define DEFAULT_SHOWLIB    "#i:\t$p\n""\t[$g] ($a - $l - #n) $t\n"
#define debug

/* macros */
#define LENGTH(X)                  (sizeof X / sizeof X[0])
#define NEED_LIB(cmd)              (cmd==PLAYPAUSE || cmd==PLAY || cmd==NEXT || cmd==PREV || cmd==STATUS || cmd==SHOWLIB || cmd==SETLIST || cmd==SHOWLIST)
#define STOPPLAYER                 if(player_pid>0) { call_hook(H_PLAY_STOP); kill(player_pid, SIGKILL); }
#define XALLOC(target, type, size) if((target = calloc(sizeof(type), size)) == NULL) die("Error: calloc failed\n")

/* enums */
enum { STOP, PAUSE, PLAYPAUSE, PLAY, NEXT, PREV, RANDOM,
       DIE, STATUS, REGISTERHANDLER,
       LOADLIB, SHOWLIB,
       SETLIST, SHOWLIST,
       SETHOOK, CommandLast };
enum { INC, DEC, SET };
enum { IM_Dying, IM_Stopped, IM_Paused, IM_Playing, StateLast };
enum { H_PLAY_BEFORE, H_PLAY_AFTER, H_PLAY_FAIL, H_SCOUT, H_PLAY_STOP, H_REGISTERHANDLER_FAIL, H_LOADLIB_FAIL, H_SETLIST_FAIL, H_PAUSE, H_UNPAUSE, HookLast };


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


/* function declarations */
// helper
static void die(const char *errstr, ...);
static void sock_printf(int cmd_sock, const char *format, ...);
static void usage(void);
// init
static int load_lib(char* file);
static void init_handlers();
static int register_handler(char* file);
static int opensock();
static void run();
// worker
static pid_t play(file_t* track, int* infp, int* outfp);
static int get_cmd(int *cmd_sock, char *val);
static void call_hook(int h);
// player helper
static void scout(unsigned int whatdo, unsigned int val);
// threads
static void* listener();
static void* player();


/* variables */
pthread_mutex_t lock;
pid_t player_pid = -1;
static int socket_port = 6666;
int ctrl_sock = -1;
// library and handlers
file_t** library; // list of files with including metadata
int num_tracks; // number of tracks in library
int* playlist = NULL; // list of index numbers in library
int num_pl_tracks; // number of tracks in playlist
char** typenames = NULL; // list of file types
int num_types; // number of file types
int* types = NULL; // maps files types to handlers
handler_t** handlers; // list of handlers
// states
int cur_track = 0; // current track in library
int state = IM_Stopped; // state, one of IM_Dying, IM_Stopped, IM_Paused, IM_Playing, StateLast
int rnd = 0; // randomize?
// commands an hooks
char* ctrl_cmds[] = { "stop", "pause", "playpause", "play", "next", "prev", "random",
                      "die", "status", "registerhandler",
                      "loadlib", "showlib",
                      "setlist", "showlist",
					  "sethook" };
char* hook_names[] = { "play_before", "play_after", "play_fail", "scout", "play_stop", "registerhandler_fail", "loadlib_fail", "setlist_fail", "pause", "unpause" };
char* hooks[CommandLast + HookLast];

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
#ifndef debug
    printf("sock_printf: %d, %s\n"format"\n", cmd_sock, format, ap);
#endif

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

void init_handlers() {
    int i;

    for(i=0; i<num_types; i++) {
        free(typenames[i]);
        free(handlers[i]);
    }
	free(typenames);
	free(types);
    free(handlers);
    XALLOC(typenames, char*, TRACKS_BUF_SIZE);
    XALLOC(types, char, TRACKS_BUF_SIZE);
	XALLOC(handlers, handler_t, TRACKS_BUF_SIZE);
}

int register_handler(char* b) {
	char *s, *parts[3];
	int hc = 0, tc = num_types, len, i, l = 0;

    for(i = 0; i<3; i++) {
        parts[i] = b;
        if(!(b=nxt(i==2?'\0':',', b))) {
            printf("Warning: error in handler list in line %d\n", l);
            return 0;
        }
    }

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
            printf("Warning: too many filetypes\n");
            return tc;
        }
    }

    if(handlers[hc]) free(handlers[hc]); // why?
    XALLOC(handlers[hc], handler_t, 1);

    // output
    handlers[hc]->out = atoi(parts[1]);

    // executable
    len = strlen(parts[2]);
    XALLOC(handlers[hc]->executable, char, len);
    strncpy(handlers[hc]->executable, parts[2], len);

    hc++;
    if(hc>TRACKS_BUF_SIZE) {
        free(b);
        printf("Warning: too many handler\n");
        return tc;
    }

	return tc;
}

int load_lib(char* file) {
    FILE *f;
    char *buf, *bufs[8], *b, *c;
    int i = 0, j = 0, l = 0, k, len, n; // TODO: propper names for i,j,l,k and n!

    XALLOC(buf, char, BUF_SIZE);
	if(library) free(library);
    XALLOC(library, file_t*, TRACKS_BUF_SIZE);

    if(!(f = fopen(file, "r"))) {
		free(buf);
		call_hook(H_LOADLIB_FAIL);
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
			printf("Warning: error reading line %d\n", l);
			continue;
		}

        XALLOC(library[i], file_t, 1);

        for(k=0; k<num_types; k++) {
            len = strlen(typenames[k]);
            if((strlen(bufs[0])==len && strncmp(bufs[0], typenames[k], len)==0) || strncmp("*", typenames[k], 1)==0) {
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

	if(playlist) free(playlist);
	XALLOC(playlist, int, i);
	for(k=0; k<i; k++)
		playlist[k] = k;
	num_pl_tracks = i;

	/*if(ferror(f)) {
        free(library);
		die("[TODO: find another punchline]: failed to read from file\n");
        return -1;
    }*/

    fclose(f);

	free(buf);
    return i;
}

int opensock() { // TODO: clean up
    int len;
    struct sockaddr_in sain;
    //struct hostent *server;

    if((ctrl_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("Error: failed to open socket %d\n", socket_port);

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
        die("Error: failed to bind socket %d\n", socket_port);

	listen(ctrl_sock, 5);
    return(1);
}

void sprintr(char **result, const char *cmd, ...) {
	va_list ap;
    char *p, *pp;
    char *tmp, *buf;
    int pos, len, n, val;

	va_start(ap, cmd);

#ifndef debug
    printf("sprintr: %s\n", cmd);
#endif

    XALLOC(buf, char, strlen(cmd));
    strcpy(buf, cmd);

    while(1) {
        if((p=va_arg(ap, char*))!=NULL) {
            switch(p[0]) {
                case '$':
                    if((pp=va_arg(ap, char*))!=NULL) {
                        while((pos = (int)strstr(buf, p))!=0) {
                            pos -= (int)buf;
                            len = strlen(pp);
                            XALLOC(tmp, char, strlen(buf) + len - 1);
                            strncpy(tmp, buf, pos);
                            strcpy(tmp + pos, pp);
                            strcpy(tmp + pos + len, buf + pos + 2);
                            free(buf);
                            buf = tmp;
                        }
                    } else {
                        die("Error: invalid sprintr call. This should never happen!\n  %s\n  %s\n", cmd, p);
                    }
                    break;
                case '#':
                    n = val = va_arg(ap, int);
                    len = 1;
                    if(n < 0) n = (n == INT_MIN) ? INT_MAX : -n;
                    while (n > 9) {
                        n /= 10;
                        len++;
                    }
                    while((pos = (int)strstr(buf, p))!=0) {
                        pos -= (int)buf;
                        XALLOC(tmp, char, strlen(buf) + len - 1);
                        strncpy(tmp, buf, pos);
                        snprintf(tmp + pos, len + 1, "%d", val);
                        strcpy(tmp + pos + len, buf + pos + 2);
                        free(buf);
                        buf = tmp;
                    }
                    break;
                default:
                    die("Error: invalid sprintr call. This should never happen!\n");
            }
        } else break;
    }

    *result = buf;
	va_end(ap);
}

pid_t play(file_t* track, int* infp, int* outfp) {
    int p_stdin[2], p_stdout[2], type = track->type;
    pid_t pid;
	char pbuf[BUF_SIZE], spbuf[BUF_SIZE];
	char *p, *ps[16] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	int i = 0;

    if(pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        die("Error: failed to open pipe\n");

	strncpy(pbuf, handlers[type]->executable, BUF_SIZE-1);
	p = strtok(pbuf, " ");
	while(p!=NULL && i<15) {
        snprintf(spbuf, BUF_SIZE, p, track->path);
		XALLOC(ps[i], char, strlen(spbuf)+1);

		strncpy(ps[i++], spbuf, strlen(spbuf));
		p = strtok(NULL, " ,");
	}

    pid = fork();

    if(pid < 0)
        die("Error: failed to fork\n");
    else if (pid == 0) { //
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], handlers[type]->out);
		execvp(ps[0], ps);
        // we should not be here, it means the exec did not work...
		die("Error: Failed to exec\n"); // TODO: dye?
    }

	for(--i; i>=0; i--)
		free(ps[i]);

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
		die("Error: failed to accept socket %d\n", socket_port); // TODO: dye?
	*buf = 0;
	n = read(*cmd_sock, buf, BUF_SIZE);
	buf[n] = 0;
	*val = 0;

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

void call_hook(int h) {
	char *cmd, *f=NULL, *g=NULL, *a=NULL, *l=NULL, *t=NULL, *y=NULL;

	if(hooks[h]) {
		if(num_tracks>0) {
            f = library[cur_track]->path;
            g = library[cur_track]->genre;
            a = library[cur_track]->artist;
            l = library[cur_track]->album;
            t = library[cur_track]->title;
            y = typenames[library[cur_track]->type];
        }
        sprintr(&cmd, hooks[h],
            "#c", cur_track, "##", num_tracks,
            "$f", f, "$g", g, "$a", a, "$l", l, "$t", t, "$y", y,
            "#p", socket_port, // "#ri" = PID,
            NULL
        );
        // Im ok with Hooks spawning two processes, they aint hang in memory forlong
        system(cmd);
	}
}

void scout(unsigned int whatdo, unsigned int val) {
	if(num_tracks<1)
		return;

	call_hook(H_SCOUT);

	pthread_mutex_lock(&lock);
	switch(whatdo) {
		case INC:
			if(rnd) {
				cur_track = random(); // valid because % num_tracks later. Haha!
			} else
				cur_track += val;
			break;
		case DEC:
			if(rnd) {
				cur_track = random();
			} else
				cur_track -= val;
			break;
		case SET:
			cur_track = val;
			break;
	}
	if(cur_track<0) cur_track = num_pl_tracks - cur_track; // sic!
	if(cur_track>=num_pl_tracks) cur_track = cur_track % num_pl_tracks;
	pthread_mutex_unlock(&lock);
}

void* listener() {
	char val[BUF_SIZE], *buf, abuf[20], *name, *ex;
	int cmd, cmd_sock, i, ii, iii, n;

	while(1) {
		if((cmd=get_cmd(&cmd_sock, val))>=0) {

			if(NEED_LIB(cmd) && num_tracks<1) {
				sock_printf(cmd_sock, "No files in library\n");
				continue;
			}
            if(cmd>=0 && cmd<CommandLast)
                call_hook(cmd + HookLast);
			switch(cmd) {
				case STOP:
					STOPPLAYER
					state = IM_Stopped;
					sock_printf(cmd_sock, "OK\n");
					break;
				case PAUSE:
					if(state>IM_Stopped && player_pid>0) {
						if(state>IM_Paused) {
							call_hook(H_PAUSE);
							kill(player_pid, 19);
							state = IM_Paused;
						} else {
							call_hook(H_UNPAUSE);
							kill(player_pid, 18);
							state = IM_Playing;
						}
					}
					sock_printf(cmd_sock, "OK\n");
					break;
				case PLAYPAUSE:
					if(player_pid>0 && state>IM_Paused && !strlen(val) && num_tracks>0) {
						call_hook(H_PAUSE);
						kill(player_pid, 19);
						state = StateLast;
					}
				case PLAY:
					if(num_tracks<1) {
					}
					if(strlen(val)) {
						i = atoi(val);
						STOPPLAYER
						scout(SET, i);
					}
					if(state==IM_Paused && player_pid>0) {
						call_hook(H_UNPAUSE);
						kill(player_pid, 18);
						state = IM_Playing;
					} else if(state==StateLast)
						state = IM_Paused;
					if(state<IM_Paused)
						state = IM_Playing;
					sock_printf(cmd_sock, "OK %d\n", state);
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
			        sprintr(&buf, strlen(val) ? val : DEFAULT_STATUS,
                              "#c", cur_track, "##", num_tracks, "$p", library[cur_track]->path,
		                      "$g", library[cur_track]->genre,
		                      "$a", library[cur_track]->artist,
		                      "$l", library[cur_track]->album,
		                      "#n", library[cur_track]->number,
							  "$t", library[cur_track]->title,
							  NULL);
					sock_printf(cmd_sock, "%s\n", buf);
					free(buf);
					break;
				case REGISTERHANDLER:
						i = strlen(val);
						if(i>0) {
							num_types = register_handler(val);
							if(num_types>0)
								sock_printf(cmd_sock, "Loaded %d file handlers.\n", num_types);
							else if(num_types==0)
								sock_printf(cmd_sock, "No valid file handlers found.\n");
							else
								sock_printf(cmd_sock, "File not found\n");
						} else
							sock_printf(cmd_sock, "No file specified\n");
					break;
/*				case SHRTSTAT:
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
					break;*/
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
					for(i=0; i<num_tracks; i++) {
						sprintr(&buf, strlen(val) ? val : DEFAULT_SHOWLIB,
								  "#i", i, "##", num_tracks, "$p", library[i]->path ? library[i]->path : "",
								  "$g", library[i]->genre ? library[i]->genre : "",
								  "$a", library[i]->artist ? library[i]->artist : "",
								  "$l", library[i]->album ? library[i]->album : "",
								  "#n", library[i]->number,
								  "$t", library[i]->title ? library[i]->title : "",
								  NULL);
						sock_printf(cmd_sock, buf);
						sock_printf(cmd_sock, "\n");
						free(buf);
					}
					break;
/*				case SHRTLIB:
					for(i=0; i<num_tracks; i++)
						sock_printf(cmd_sock, "%d\t%s\t%s\t%s\t%s\t%d\t%s\t\n",
							i, library[i]->path,
							library[i]->genre,
							library[i]->artist,
							library[i]->album,
							library[i]->number,
							library[i]->title);
					break;*/
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
									call_hook(H_SETLIST_FAIL);
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
				case SETHOOK:
					name = ex = val;
					while(*ex!=' ' && *ex!=0)
						ex++;
					if(*ex==0) {
						sock_printf(cmd_sock, "Invalid hook definition\n");
						break;
					}
					*ex = 0;
					ex++;
					n = strlen(name);
					for(i=0; i<HookLast; i++)
						if(n==strlen(hook_names[i]) && strncmp(name, hook_names[i], n)==0) break;
					if(i==HookLast) {
						for(i=0; i<CommandLast; i++)
							if(n==strlen(ctrl_cmds[i]) && strncmp(name, ctrl_cmds[i], n)==0) break;
						if(i==CommandLast) {
							sock_printf(cmd_sock, "Unknown hook\n");
							break;
						}
						i += HookLast;
					}
					free(hooks[i]);
					n = strlen(ex);
                    printf("HOOK: %d, %s\n", i, ex);
					XALLOC(hooks[i], char, n);
					strncpy(hooks[i], ex, n);
					sock_printf(cmd_sock, "OK\n");
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

	if(num_tracks<1) {
		printf("fuuuh keine tracks\n");
		return NULL;
		}

	call_hook(H_PLAY_BEFORE);
	pthread_mutex_lock(&lock);
	player_pid = play(library[cur_track], &infp, &outfp);
	pthread_mutex_unlock(&lock);

	waitpid(player_pid, &stat, 0);
	if(WIFEXITED(stat)!=0) {
		scout(INC, 1);
		call_hook(H_PLAY_AFTER);
	} else {
		call_hook(H_PLAY_FAIL);
	}

	player_pid = -1;

	close(infp);
	close(outfp);

	return NULL;
}

void run() {
	pthread_t p_player, p_listener;

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
}

int main(int argc, char *argv[]) {
#ifndef debug
    pid_t pid;
#endif
	char *listfile = NULL, /**handlerfile = NULL,*/ *listpath = "%s/madasul/list"/*, *handlerpath = "%s/madasul/handler"*/;
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
		}
	}

    init_handlers();

	if(listfile==NULL) {
		l = strlen(getenv("XDG_CONFIG_HOME")) + strlen(listpath);
		XALLOC(listfile, char, l);
		snprintf(listfile, l, listpath, getenv("XDG_CONFIG_HOME"));
		num_tracks = load_lib(listfile);
		free(listfile);
	} else {
		if((num_tracks=load_lib(listfile))<0)
			die(OPEN_FILE_ERR, listfile);
	}
	if(num_tracks>=0) printf("loaded %d files from list\n", num_tracks);

	srandom((unsigned int)time(NULL));

	opensock();
	printf("Listening on Socket %d.\n", socket_port);

	for(i=0; i<CommandLast + HookLast; i++)
		hooks[i] = NULL;

#ifndef debug
    pid = fork();
    if(pid < 0) {
        die("Error: failed to fork\n");
    } else if(pid != 0) {
        printf("forked to pid %d\n", pid);
        return 0;
    }
#endif

	run();

    return 0;
}
