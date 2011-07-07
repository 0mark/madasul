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

#define LENGTH(X)          (sizeof X / sizeof X[0])

#define BUF_SIZE           1024
#define TRACKS_BUF_SIZE	   128
#define SOCKET_ADDRESS     "/home/mark/.madasul_sock"
#define READ               0
#define WRITE              1
#define ERR                2

enum { MPG, OGG, WAV, FLACC, TypeLast };
enum { STOP, PAUSE, PLAY, NEXT, PREV, DIE, STATUS, CommandLast };
enum { INC, DEC, SET };

typedef struct track {
    int type;
    char* path;
} track;

track** tracks;
char* typenames[] = { "mp3", "mp2", "ogg", /*"wav", "wave", "flacc", */};
int types[]       = {  MPG,   MPG,   OGG,  /* WAV,   WAV,    FLACC, */};
char* ctrl_cmds[] = { "stop", "pause", "play", "next", "prev", "die", "status", };
char* play_cmds[][3]    = {
	{ "/usr/bin/mpg123", "mpg123", "" },
	{ "/usr/bin/ogg123", "ogg123", "" },
};

int ctrl_sock     = -1;
int cur_track, num_tracks;
//int stop = 0;
int running = 0;
pthread_mutex_t lock;
pid_t player_pid = -1;

pid_t play(track* track, int* infp, int* outfp) {
    int p_stdin[2], p_stdout[2], type = track->type;
    pid_t pid;
	char path[BUF_SIZE+2];

    if(pipe(p_stdin) != 0 || pipe(p_stdout) != 0)
        return -1;

    pid = fork();

    if(pid < 0)
        return pid;
    else if (pid == 0) {
        close(p_stdin[WRITE]);
        dup2(p_stdin[READ], READ);
        close(p_stdout[READ]);
        dup2(p_stdout[WRITE], ERR);
        snprintf(path, BUF_SIZE+2, "\"%s\"", track->path);
		execl(play_cmds[type][0], play_cmds[type][1], track->path, play_cmds[type][2], NULL);
        perror("execl");
        exit(1);
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

int munchIn() {
    char type[10], buffer[BUF_SIZE];
    int i = 0, j = 0, k, len;

    tracks = calloc(sizeof(track*), TRACKS_BUF_SIZE);

    if(tracks == NULL) {
        perror("Failed to allocate content");
        return -1;
    }

    while(!feof(stdin)) {
        if(fscanf(stdin, "%[^\t]\t%[^\n]\n", type, buffer)!=2)
	    continue;
        len = strlen(buffer);
        tracks[i] = calloc(sizeof(track), len + 1);
        tracks[i]->path = calloc(sizeof(char), len + 1);
        for(k=0; k<LENGTH(typenames); k++)
            if(strncmp(type, typenames[k], LENGTH(typenames[k]))==0)
                tracks[i]->type = types[k];
        strncpy(tracks[i]->path, buffer, len);
        i++;
        j++;
        if(j>=TRACKS_BUF_SIZE) {
            track **tmp = tracks;
            tracks = calloc(sizeof(track*), i + TRACKS_BUF_SIZE - 1);
            memcpy(tracks, tmp, i - 1);
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

    if((ctrl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        ctrl_sock = -1;
        printf("Error opening socket.\n");
	return(0);
    }

	flags = fcntl(ctrl_sock, F_GETFL, 0);
	//fcntl(ctrl_sock, F_SETFL, flags | O_NONBLOCK);

    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, SOCKET_ADDRESS);

    len = sizeof(saun.sun_family) + strlen(saun.sun_path);

    if(bind(ctrl_sock, (struct sockaddr *)&saun, len)<0) {
        printf("Error connecting socket.\n");
        return(0);
    }
	listen(ctrl_sock, 5);
    return(1);
}

void die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

int get_cmd(int *cmd_sock, char *val) {
	struct sockaddr_un cli_addr;
	socklen_t clilen;
	char buf[BUF_SIZE];
	int i, n, len, cmd = -1;

	clilen = sizeof(cli_addr);
	*cmd_sock = accept(ctrl_sock, (struct sockaddr *)&cli_addr, &clilen);
	if(cmd_sock < 0)
		die("cant open listen socket\n");
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
			cur_track += val;
			break;
		case DEC:
			cur_track -= val;
			break;
		case SET:
			cur_track = val;
			break;
	}
	if(cur_track<=0) cur_track = num_tracks-1;
	if(cur_track>=num_tracks) cur_track = 0;
	pthread_mutex_unlock(&lock);
}

void* player() {
	FILE* fp;
    int num, infp, w, outfp, cmd, stat;
    char line[BUF_SIZE];

printf("P a\n");
	if(player_pid>=0)
		die("pid is already in use\n");
printf("P b\n");

	pthread_mutex_lock(&lock);
printf("P c\n");
	if((player_pid = play(tracks[cur_track], &infp, &outfp))<=0) {
printf("P d\n");
		die("cant exec player\n");
	}
printf("P e\n");
	pthread_mutex_unlock(&lock);
printf("P f\n");

	w = waitpid(player_pid, &stat, 0);
printf("P g\n");
	if(WIFEXITED(stat)!=0) // WEXITSTATUS(stat_val)
		scout(INC, 1);
printf("P h\n");

	player_pid = -1;
printf("P i\n");

	close(infp);
printf("P j\n");
	close(outfp);
printf("P k\n");
}

void sock_printf(int cmd_sock, const char *format, ...) {
	char msg[BUF_SIZE];
	va_list ap;

	va_start(ap, format);
	vsnprintf(msg, BUF_SIZE, format, ap);
	//printf("%s\n", msg);
	send(cmd_sock, msg, strlen(msg), MSG_NOSIGNAL);
	va_end(ap);
}

void* listener() {
	char val[BUF_SIZE];
	int cmd, cmd_sock, i;
printf("L a\n");
	while(1) {
printf("L b\n");
		if((cmd=get_cmd(&cmd_sock, val))>=0) {
printf("L c %d\n", cmd);
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
				printf("---%s---\n", val);
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
				default:
					sock_printf(cmd_sock, "unknown command");
					break;
			}
		} else {
printf("L d\n");
			sock_printf(cmd_sock, "invalid command");
		}
printf("L e\n");
		close(cmd_sock);
printf("L f\n");
	}
printf("L g\n");
}

int main(/*int argc, char *argv[]*/) {
	pthread_t p_player, p_listener;
	//pthread_attr_t attr;

	num_tracks = munchIn();
	printf("munched %d lines\n", num_tracks);
	if(!opensock())
	    die("bare feet in nakatomi tower!\n");
	printf("opened socket\n");

printf("M a\n");
	pthread_mutex_init(&lock, NULL);
printf("M b\n");
	//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
printf("M c\n");

	pthread_create(&p_listener, NULL, listener, NULL);
printf("M c1\n");
		//pthread_join(p_listener, NULL);
printf("M d\n");
	running = 1;
	while(running==1) {
printf("M e\n");
		pthread_create(&p_player, NULL, player, NULL);
printf("M f\n");
		pthread_join(p_player, NULL);
printf("M g\n");
		sleep(1);
	}

    return 0;
}









/*
		if(cmd==STOP) {
			printf("*puff*\n");
			close(ctrl_sock);
			if(pid>0) {
				close(infp);
				close(outfp);
				kill(pid, SIGKILL);
				x=wait(&stat);
			}
			return 0;
		} else if(cmd==NEXT) {
			close(infp);
			close(outfp);
			kill(pid, SIGKILL);
			x=wait(&stat);
			pid = -1;
		} else if(cmd==PREV) {
			tracknum-=2;
			if(tracknum<0) tracknum = num - 1;
			close(infp);
			close(outfp);
			kill(pid, SIGKILL);
			x=wait(&stat);
			pid = -1;
		}
*/


	//pthread_join (p2, NULL);

    /*FILE* fp;
    int num, infp, x, outfp, cmd, stat;
    char line[BUF_SIZE];
    pid_t pid;

    num = munchIn();
    printf("munched %d lines\n", num);

	opensock();
	if((pid=play(tracks[tracknum++], &infp, &outfp)) <= 0) {
		printf("popen2 failed\n");
		return 0;
	}

	fp = fdopen(outfp, "r");

	while(1) {
		if(pid<1) {
			//printf("playing (%d):\n", tracknum); printf("%s\n", tracks[tracknum]->path);
			if((pid = play(tracks[tracknum++], &infp, &outfp))<=0) {
				printf("something failed");
				return 1;
			}
		}

		x = waitpid(pid, &stat, WNOHANG);
		//printf("(%d): %d, %d\n", pid, x, WIFEXITED(stat));
		if(x>0 && WIFEXITED(stat)>0) {
			scout(INC, 1);
		} else {
			get_cmd();
		}

		if(cur_track!=next_track) {
			close(infp);
			close(outfp);
			pid = -1;
		}


		sleep(1);
	}*/
