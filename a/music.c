#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
#define __USE_GNU
#include <netdb.h>
#include <errno.h>
#include <sys/wait.h>

#define LENGTH(X)          (sizeof X / sizeof X[0])

#define BUF_SIZE           1024
#define TRACKS_BUF_SIZE	   128
#define SOCKET_ADDRESS     "/home/mark/musicsocket"
#define READ               0
#define WRITE              1
#define ERR                2

enum { MPG, OGG, WAV, FLACC, TypeLast };
enum { STOP, PAUSE, PLAY, NEXT, PREV, DIE, STATUS, CommandLast };
enum { INC, DEC, SET }

typedef struct track {
    int type;
    char* path;
} track;

track** tracks;
char* typenames[] = { "mp3", "mp2", "ogg", /*"wav", "wave", "flacc", */};
int types[]       = {  MPG,   MPG,   OGG,  /* WAV,   WAV,    FLACC, */};
char* ctrl_cmds[] = { "stop", "pause", "play", "next", "prev", "die", "status", };
int ctrl_sock     = -1, tracknum = 0;
char* play_cmds[][3]    = {
	{ "/usr/bin/mpg123", "mpg123", "" },
	{ "/usr/bin/ogg123", "ogg123", "" },
};

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
		return;
    }

	flags = fcntl(ctrl_sock, F_GETFL, 0);
	fcntl(ctrl_sock, F_SETFL, flags | O_NONBLOCK);

    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, SOCKET_ADDRESS);

    len = sizeof(saun.sun_family) + strlen(saun.sun_path);

    if(bind(ctrl_sock, (struct sockaddr *)&saun, len)<0) {
        printf("Error connecting socket.\n");
        return;
    }
	listen(ctrl_sock, 5);

}

int get_cmd() {
	FILE *ctrl_fp;
	struct sockaddr_un cli_addr;
	socklen_t clilen;
	char buf[BUF_SIZE];
	int i, cmd_sock, cmd = -1, n, stat;

	clilen = sizeof(cli_addr);
	cmd_sock = accept(ctrl_sock, (struct sockaddr *)&cli_addr, &clilen);
	if(cmd_sock < 0)
		return -1;

	n = read(cmd_sock, buf, BUF_SIZE);

	if(n>0) {
		buf[n] = 0;
		for(i=0; i<LENGTH(ctrl_cmds); i++)
			if(strncmp(ctrl_cmds[i], buf, strlen(ctrl_cmds[i]))==0) {
				cmd = i;
				break;
			}
	}

	if(cmd<0) {
		char msg[] = "INVALID\n";
		send(cmd_sock, msg, strlen(msg), MSG_NOSIGNAL);
	} else if(cmd==STATUS) {
		char msg[BUF_SIZE];
		int n = tracknum-1<0 ? 0: tracknum-1;
		snprintf(msg, BUF_SIZE, "Track (%d): %s\n", n, tracks[n]->path);
		send(cmd_sock, msg, strlen(msg), MSG_NOSIGNAL);
	} else {
		char msg[] = "OK\n";
		send(cmd_sock, msg, strlen(msg), MSG_NOSIGNAL);
	}
	close(cmd_sock);

	return cmd;
}

int next_track, cur_track, num_tracks;

// you know, like track selection...
void scout(unsigned int whatdow, unsigned int val) {
	switch(whatdo) {
		case INC:
			next_track += val;
			break;
		case DEC:
			next_track -= val;
			break;
		case SET:
			next_track = val;
			break;
	}
	if(next_track<=0) next_track = num_tracks-1;
	if(next_track>=num_tracks) next_track = 0;
}

int main(/*int argc, char *argv[]*/) {
    FILE* fp;
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