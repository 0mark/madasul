#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include "ansi.h"

/* variables */
char eschar[3] = { 0, 0, 0 };


/* function definitions */
char getch(int echo) {
	static char buf[4];// = 0;
	int l;
	*buf = *eschar = 0;
	struct termios old = {0};
	if(tcgetattr(0, &old) < 0)
		perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	if(!echo)
		old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if(tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if((l=read(0, buf, 4)) < 0)
		perror ("read()");
	if(l>1) {
		memcpy(eschar, buf+2, l - 1);
		//eschar = buf[2];
		eschar[l] = 0;
	}
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
		perror ("tcsetattr ~ICANON");
	//memcpy(bla, buf, 4);
	//bla[4]=0;
	return (*buf);
}

void putstr(char *str) {
	//char buf = 0;
	struct termios old = {0};
	if (tcgetattr(0, &old) < 0)
	perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(1, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	//if (read(0, &buf, 1) < 0)
	//	perror ("read()");
	write(1, str, strlen(str));
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(1, TCSADRAIN, &old) < 0)
		perror ("tcsetattr ~ICANON");
	//return (buf);
}

int readln(char *ln, int len) {
	int p = 0, l = 0, i;
	static char c[2] = { 0, 0 }, buf[10];

	putstr(SCP);
	while(1) {
		*c = getch(ECHO_OFF);
		switch(*c) {
			case '\x1B':
				if(*eschar) {
					//*c = eschar;
					//snprintf(buf, 10,"%s",bla+1);
					//putstr(eschar);
					switch(*eschar) {
						case 'A': // up
							//TODO: implement history
							break;
						case 'B': // down
							//TODO: implement history
							break;
						case 'D': // left
							if(p>0) {
								putstr(CLE(1));
								p--;
							}
							break;
						case 'C': // right
							if(p<l) {
								putstr(CRE(1));
								p++;
							}
							break;
						case '1': // home
							snprintf(buf, 10, ESC"%dD", p);
							putstr(buf);
							p = 0;
							break;
						case '4': // end
							snprintf(buf, 10, ESC"%dC", l-p);
							putstr(buf);
							p = l;
							break;
					}
				} else {
					ln[0] = 0;
					return 0;
				}
				break;
			case '\n':
				putstr(RCP CLL);
				return l;
			case 127:
				if(p>0) {
					ln[--p] = 0;
					putstr(CLE(1)CLL);
					l--;
				}
			default:
				if(l<len-1) {
					if(l==p) {
						ln[p++] = *c;
						ln[p] = 0;
					} else {
						for(i=l; i>=p; i--)
							ln[i+1] = ln[i];
						ln[p++] = *c;
					}
					l++;
				}
				putstr(RCP);
				putstr(ln);
				if(l>p) {
					snprintf(buf, 10, ESC"%dD", l-p);
					putstr(buf);
				}
		}
	}
}

int wasESC() {
	return (!*eschar?1:0);
}