#define _POSIX_C_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define __USE_GNU
#include <netdb.h>
#include <stdarg.h>
//#include <termios.h>
#include <regex.h>
#include <limits.h>
#include <strings.h>
#include "ansi.h"
#include "config.h"


/* macros */
#define aprintf(STR, ...)   snprintf(STR+strlen(STR), BUF_SIZE-strlen(STR), __VA_ARGS__)
#define LAST(E, L)          for(E=L; E && E->next; E = E->next);


/* structs */
typedef struct Filter Filter;
struct Filter {
	int ar, al, ti, ge, pa;
	char *ex;
	Filter *next;
};

typedef struct Filterlist Filterlist;
struct Filterlist {
	char *name;
	Filter *filter;
	Filterlist *next;
};


/* function declarations */
// helper
static void die(const char *errstr, ...);
// socket
static void opensock();
static int talk2sock(char *cmd);
// output
static void head();
static int selfields(Filter *flt);
// lists
static void listFromFilters(Filter *flt);


/* variables */
int madasul_sockfd;
char errorstring[256];
Filterlist *filterlists = NULL;
Filterlist *curlist = NULL;


/* function definitions */
void die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void opensock() {
	struct sockaddr_in serv_addr;
	struct hostent *server;

	if(madasul_sockfd>0) close(madasul_sockfd);

	if((madasul_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("Bare feet in Nakatomi Tower Error: failed to open socket.\n");

	if((server = gethostbyname(SOCKET_ADRESS)) == NULL)
        die("Bare feet in Nakatomi Tower Error: failed to get hostname for socket.\n");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(SOCKET_PORT);

	if(connect(madasul_sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        die("Bare feet in Nakatomi Tower Error: failed to connect to socket.\n");
}

int talk2sock(char *cmd) {
    int n;
	opensock();
	n = write(madasul_sockfd, cmd, strlen(cmd));
    //if(n<0)
    //     error("ERROR writing to socket");
    //n = read(madasul_sockfd, buffer, 255);
    //if (n < 0)
    //     error("ERROR reading from socket");
	return n;
}

void head() {
	static char buf[BUF_SIZE], path[BUF_SIZE], artist[BUF_SIZE], album[BUF_SIZE], title[BUF_SIZE], genre[BUF_SIZE]/*, date[BUF_SIZE]*/;
	static char cmd[] = "shrtstat", *statxt[5] = {"XX", "--", "||", ">>", "?"};
	unsigned int track, tracknum, atrack, status, rnd;

	opensock();

	if(talk2sock(cmd)<0) {
		printf("ERROR sending command\n");
		return;
	}

    read(madasul_sockfd, buf, BUF_SIZE);

	if(sscanf(buf, "%u\t%u\t%u\t%u\t%[^\t]\t%[^\t]\t%[^\t]\t%[^\t]\t%u\t%[^\n]\n", &track, &tracknum, &status, &rnd, path, genre, artist, album, &atrack, title/*, date*/)!=10) {
		printf("ERROR parsing response\n");
		return;
	} else {
		if(status<0 || status>2) status = 3;
		printf(
			CLS
			HMAG"M"CYA"adasul "                               HMAG"C"CYA"ontrol "  HMAG"P"CYA"rogram "  HCYA VERSION CLR"\n"
			                                                 GTH(14)    "%srnd    "MAG"<<  []  |>  >>"CLR"\n"
			MAG"["YEL"%s"MAG"]"GTH(6) HGRN"%d"MAG"/"GRN"%d"  GTH(14)HBLK"r      b   s   p   f\n"
			GRN"%s"MAG" - "GRN"%s"MAG" - ["CYA"%d"MAG"] "GRN"%s\n"
			GTH(20) "%s" CLR "\n",
			rnd ? MAG : HBLK, statxt[status], track, tracknum,
			artist, album, atrack, title, errorstring);
	}

	close(madasul_sockfd);
}

int selfields(Filter *flt) {
	char c, cmd[256];

	while(1) {
		snprintf(cmd, 256, ESC"1G"MAG"a%srtist"CYA", %sa"MAG"l%sbum"CYA", "MAG"t%sitle"CYA", "MAG"g%senre, "MAG"p%sath "MAG"<┘", flt->ar?GRN:HBLK, flt->al?GRN:HBLK, flt->al?GRN:HBLK, flt->ti?GRN:HBLK, flt->ge?GRN:HBLK, flt->pa?GRN:HBLK);
		putstr(cmd);

		c = getch(ECHO_OFF);
		switch(c) {
			case 'a': flt->ar = !flt->ar; break;
			case 'l': flt->al = !flt->al; break;
			case 't': flt->ti = !flt->ti; break;
			case 'g': flt->ge = !flt->ge; break;
			case 'p': flt->pa = !flt->pa; break;
			case '\x1B':
				return 0;
			case '\n':
				return 1;
				break;
		}
	}
}

void listlists() {
	Filterlist *l;
	static char buf[64];
	int i=0;

	putstr("\n"CYA"Filterlists: "GRN);

	for(l=filterlists; l; l=l->next) {
		snprintf(buf, 64, HGRN"%d"GRN" %s%s", i++, l->name, l==curlist?"*":"");
		putstr(buf);
		if(l->next) putstr(MAG", "GRN);
	}
}

void listFromFilters(Filter *flt) {
	FILE *fp;
    regex_t preg;
	char path[BUF_SIZE], artist[BUF_SIZE], album[BUF_SIZE], title[BUF_SIZE], genre[BUF_SIZE], *list;
	unsigned int tracknum, atrack, rm, i = 0;
	int maxnumlen = (sizeof(int) * 8) / 3 + 1; // a bit higher than the real value...

	if((list=calloc(sizeof(char), BUF_SIZE))==NULL) {
		strcpy(errorstring, "Failed to allocate buffer");
		return;
	}
	list[0] = 0;
	aprintf(list, "setlist ");

	while(flt) {
		if(regcomp(&preg, flt->ex, REG_EXTENDED | REG_NOSUB)!=0) {
			snprintf(errorstring, 256, "Invalid regex: %s", flt->ex);
			return;
		}

		if(talk2sock("shorttracklist")<0) {
			strcpy(errorstring, "ERROR sending command");
			return;
		}

		fp = fdopen(madasul_sockfd, "r");
		while(fp && !feof(fp)) {
			if(fscanf(fp, "%u\t%[^\t]\t%[^\t]\t%[^\t]\t%[^\t]\t%u\t%[^\n]\n", &tracknum, path, genre, artist, album, &atrack, title/*, date*/)!=7)
				break;
			rm = 0;
			if(flt->ar)
				rm += regexec(&preg, artist, 0, NULL, 0) ? 0 : 1;
			if(flt->al)
				rm += regexec(&preg, album, 0, NULL, 0) ? 0 : 1;
			if(flt->ti)
				rm += regexec(&preg, title, 0, NULL, 0) ? 0 : 1;
			if(flt->ge)
				rm += regexec(&preg, genre, 0, NULL, 0) ? 0 : 1;
			if(flt->pa)
				rm += regexec(&preg, path, 0, NULL, 0) ? 0 : 1;
			if(rm>0) {
				// TODO: this is very ineffizient..., strlen is called way to often!
				aprintf(list, "%d,", tracknum);
				if(strlen(list)+maxnumlen+1>=BUF_SIZE) {
					list = realloc(list, sizeof(char) * i * BUF_SIZE);
					i++;
				}
			}
		}
		fclose(fp);
		flt = flt->next;
	}

	if(strlen(list)) {
		list[strlen(list)-1] = 0;
		talk2sock(list);
	}

	free(list);
	regfree(&preg);
}


int main(int argc, char *argv[]) {
    Filter *flt=NULL, *f;
	Filterlist *flst=NULL, *fl;
	char buffer[256], cmd[256], c, buf[64];
	int i=0, reinit = 1;
	*errorstring=0;

	opensock();
	head();

	while(1) {
		if(reinit){
			reinit = 0;
			buffer[0] = 0;
			i = 0;
			head();
			putstr(": ");
		}
		c = getch(ECHO_OFF);
		if(*errorstring) {
			*errorstring = 0;
			putstr(SCP GTO(5,1) CLL RCP);
		}
		switch(c) {
			case 'b':
				snprintf(cmd, 256, "prev %d\n", strlen(buffer) ? atoi(buffer) : 1);
				talk2sock(cmd);
				reinit = 1;
				break;
			case 's':
				talk2sock("stop\n");
				reinit = 1;
				break;
			case 'p':
				if(strlen(buffer))
					snprintf(cmd, 256, "playpause %d\n", strlen(buffer) ? atoi(buffer) : 1);
				else
					snprintf(cmd, 256, "playpause");
				talk2sock(cmd);
				reinit = 1;
				break;
			case 'f':
				snprintf(cmd, 256, "next %d\n", strlen(buffer) ? atoi(buffer) : 1);
				talk2sock(cmd);
				reinit = 1;
				break;
			case 'r':
				talk2sock("random");
				reinit = 1;
				break;
			case 'q':
				if(madasul_sockfd>0)
					close(madasul_sockfd);
				exit(0);
				break;
			case 'a': // add filter to current list
				if(!curlist) {
					strcpy(errorstring, HYEL"No Filterlist!");
					reinit = 1;
					break;
				}
				flt = calloc(sizeof(Filter), 1);
				flt->al = flt->ar = flt->ti = flt->ge = flt->pa = 0;
				flt->next = NULL;
				if(selfields(flt)) {
					putstr("\n"CYA"regex"MAG": "RED);
					if((i=readln(buffer, 256))>0) {
						flt->ex = calloc(sizeof(char), i);
						memcpy(flt->ex, buffer, i);
						LAST(f, curlist->filter);
						if(f) f->next = flt;
						else curlist->filter = flt;
					} else free(flt);
				} else free(flt);
				reinit = 1;
				break;
			case 'd': // delete filter from current list
				break;
			case 'u': // use current filter list
				if(!curlist) {
					strcpy(errorstring, HYEL"No Filterlist!");
					reinit = 1;
					break;
				}
				listFromFilters(curlist->filter);
				reinit = 1;
				break;
			case 'l': // select filter list
				listlists();
				putstr(MAG": "CYA);
				if(readln(buf, 64)>0) {
					i = atoi(buf);
					for(curlist=filterlists; curlist && (i--)>0; curlist=curlist->next);
				}
				reinit = 1;
				break;
			case 'A': // add filter list
			    putstr("\n"CYA"list name"MAG": "RED);
				if((i=readln(buffer, 256))>0) {
					flst = calloc(sizeof(Filterlist), 1);
					flst->name = calloc(sizeof(char), i);
					memcpy(flst->name, buffer, i);
					LAST(fl, filterlists);
					if(fl) fl->next = flst;
					else filterlists = flst;
					curlist = flst;
				} else free(flst);
				reinit = 1;
				break;
			case 'D': // remove filter list
				break;
			case 'w': // save all filter lists
				break;
			case 127: // DEL
				if(i) {
					buffer[--i] = 0;
					putstr(CLE(1)CLL);
				}
				break;
			case '\x1B': // ESC or special
				if(wasESC())
					reinit = 1;
				break;
			case '\n': // ENTER
				reinit = 1;
				break;
			default:
				if(i<254 && c>'0' && c<'9') {
					buffer[i++] = c;
					buffer[i] = 0;
					putstr(buffer + i - 1);
				}
		}
	}
}
