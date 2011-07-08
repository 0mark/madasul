#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <signal.h>
//#include <string.h>
//#include <sys/socket.h>
//#include <sys/types.h>
#include <sys/un.h>
//#include <netinet/in.h>
#include <fcntl.h>
#include <pthread.h>
//#define __USE_GNU
#include <netdb.h>
//#include <errno.h>
#include <sys/wait.h>
#include <stdarg.h>

/* macros */
#define LENGTH(X)          (sizeof X / sizeof X[0])

#define BUF_SIZE           1024
#define TRACKS_BUF_SIZE	   128
#define SOCKET_ADDRESS     "/home/mark/.madasul_sock"
#define READ               0
#define WRITE              1
#define ERR                2


/* enums */
enum { MPG, OGG, WAV, FLACC, TypeLast };
enum { STOP, PAUSE, PLAY, NEXT, PREV, DIE, STATUS, RND, LIST, CommandLast };
enum { INC, DEC, SET };


/* structs */
typedef struct track {
    int type;
    char* path;
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
// types
char* typenames[] = { "mp3", "mp2", "ogg", /*"wav", "wave", "flacc", */};
int types[]       = {  MPG,   MPG,   OGG,  /* WAV,   WAV,    FLACC, */};
// player
char* play_cmds[][3]    = {
	{ "/usr/bin/mpg123", "mpg123", "" },
	{ "/usr/bin/ogg123", "ogg123", "" },
};
// commands
char* ctrl_cmds[] = { "stop", "pause", "play", "next", "prev", "die", "status", "random", "tracklist"};

track** tracks;
int ctrl_sock     = -1;
int cur_track, num_tracks;
int running = 0;
pthread_mutex_t lock;
pid_t player_pid = -1;
int rnd = 1;
static const int debug =0;


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

	if(debug)
		printf("to socket: %s\n", msg);

	send(cmd_sock, msg, strlen(msg), MSG_NOSIGNAL);
	va_end(ap);
}

int munchIn() {
    char type[10], buffer[BUF_SIZE];
    int i = 0, j = 0, k, len;

    if((tracks = calloc(sizeof(track*), TRACKS_BUF_SIZE)) == NULL)
        die("Ex memoria on Mars Error: failed to allocate some memory\n");

    while(!feof(stdin)) {
        if(fscanf(stdin, "%[^\t]\t%[^\n]\n", type, buffer)!=2) {
			printf("Runaway Indian in the prairie of Spain Error: failed to read track line %d\n", i);
			continue;
		}
        len = strlen(buffer);

        if((tracks[i] = calloc(sizeof(track), len + 1)) == NULL)
	        die("Ex memoria on Mars Error: failed to allocate some memory\n");
        if((tracks[i]->path = calloc(sizeof(char), len + 1)) == NULL)
	        die("Ex memoria on Mars Error: failed to allocate some memory\n");

        for(k=0; k<LENGTH(typenames); k++)
            if(strncmp(type, typenames[k], LENGTH(typenames[k]))==0)
                tracks[i]->type = types[k];
        strncpy(tracks[i]->path, buffer, len);
        i++;
        j++;
        if(j>=TRACKS_BUF_SIZE) {
            track **tmp = tracks;
            if((tracks = calloc(sizeof(track*), i + TRACKS_BUF_SIZE - 1)) == NULL)
		        die("Ex memoria on Mars Error: failed to allocate some memory\n");
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
    int len, flags;
    struct sockaddr_un saun;

    if((ctrl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        die("Bare feet in Nakatomi Tower Error: failed to open socket.\n");

	flags = fcntl(ctrl_sock, F_GETFL, 0);
    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, SOCKET_ADDRESS);
    len = sizeof(saun.sun_family) + strlen(saun.sun_path);

    if(bind(ctrl_sock, (struct sockaddr *)&saun, len)<0)
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
	n = read(*cmd_sock, buf, BUF_SIZE);
	buf[n] = 0;
	val[0] = 0;

	if(n>0) {
		for(i=0; i<LENGTH(ctrl_cmds); i++) {
			len = strlen(ctrl_cmds[i]);
			if(strncmp(ctrl_cmds[i], buf, len)==0) {
				cmd = i;
				break;
			}
		}
		if(n>len)
			strncpy(val, buf+len+1, n);
	}

	return cmd;
}

// you know, like track selection...
void scout(unsigned int whatdo, unsigned int val) {
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
				cur_track = random(); // valid because % num_tracks later. Haha!
			} else
				cur_track -= val;
			break;
		case SET:
			cur_track = val;
			break;
	}
	if(cur_track<0) cur_track = num_tracks - cur_track;
	if(cur_track>=num_tracks) cur_track = cur_track % num_tracks;
	pthread_mutex_unlock(&lock);
}

void* listener() {
	char val[BUF_SIZE];
	int cmd, cmd_sock, i;

	while(1) {
		if((cmd=get_cmd(&cmd_sock, val))>=0) {
			switch(cmd) {
				case STOP:
					kill(player_pid, SIGKILL);
					sock_printf(cmd_sock, "OK\n");
					break;
				case PAUSE:
					sock_printf(cmd_sock, "not implemented\n");
					break;
				case PLAY:
					if(strlen(val)) i = atoi(val);
					else i = 1;
					if(i>=0) {
						kill(player_pid, SIGKILL);
						scout(SET, i);
						sock_printf(cmd_sock, "OK\n");
					} else
						sock_printf(cmd_sock, "bad value\n");
					break;
				case NEXT:
					if(strlen(val)) i = atoi(val);
					else i = 1;
					if(i>0) {
						kill(player_pid, SIGKILL);
						scout(INC, i);
						sock_printf(cmd_sock, "OK\n");
					} else
						sock_printf(cmd_sock, "bad value\n");
					break;
				case PREV:
					if(strlen(val)) i = atoi(val);
					else i = 1;
					if(i>0) {
						kill(player_pid, SIGKILL);
						scout(DEC, i);
						sock_printf(cmd_sock, "OK\n");
					} else
						sock_printf(cmd_sock, "bad value\n");
					break;
				case DIE:
					kill(player_pid, SIGKILL);
					running = 0;
					sock_printf(cmd_sock, "OK\n");
					break;
				case STATUS:
					sock_printf(cmd_sock, "Track (%d): %s\n", cur_track, tracks[cur_track]->path);
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
						sock_printf(cmd_sock, "Track (%d): %s\n", i, tracks[i]->path);
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
}

void* player() {
	FILE* fp;
    int num, infp, w, outfp, cmd, stat;
    char line[BUF_SIZE];

	if(player_pid>=0)
		die("Heile Welt at Sehbuehl Error: pid is already in use\n");

	pthread_mutex_lock(&lock);
	player_pid = play(tracks[cur_track], &infp, &outfp);
	pthread_mutex_unlock(&lock);

	w = waitpid(player_pid, &stat, 0);
	if(WIFEXITED(stat)!=0) // WEXITSTATUS(stat_val)
		scout(INC, 1);

	player_pid = -1;

	close(infp);
	close(outfp);
}

int main(/*int argc, char *argv[]*/) {
	pthread_t p_player, p_listener;

	srandom((unsigned int)time(NULL));

	num_tracks = munchIn();
	printf("munched %d lines\n", num_tracks);
	opensock();
	printf("opened socket\n");

	pthread_mutex_init(&lock, NULL);

	pthread_create(&p_listener, NULL, listener, NULL);
	running = 1;
	while(running==1) {
		pthread_create(&p_player, NULL, player, NULL);
		pthread_join(p_player, NULL);
		sleep(1);
	}

    return 0;
}
