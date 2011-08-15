/* constants */
#define ESC       "\x1B["
#define CLS       "\x1B[1J\x1B[1;1H\x1B[0m"
#define CLL       "\x1B[K"
#define CLR       "\x1B[0m"

#define BLK       "\x1B[0;30m"
#define RED       "\x1B[0;31m"
#define GRN       "\x1B[0;32m"
#define YEL       "\x1B[0;33m"
#define BLU       "\x1B[0;34m"
#define MAG       "\x1B[0;35m"
#define CYA       "\x1B[0;36m"
#define WHT       "\x1B[0;37m"
#define HBLK      "\x1B[1;30m"
#define HRED      "\x1B[1;31m"
#define HGRN      "\x1B[1;32m"
#define HYEL      "\x1B[1;33m"
#define HBLU      "\x1B[1;34m"
#define HMAG      "\x1B[1;35m"
#define HCYA      "\x1B[1;36m"
#define HWHT      "\x1B[1;37m"

#define SCP       "\x1B[s"
#define RCP       "\x1B[u"


/* macros */
#define GTH(C)    "\x1B[" #C "G"
#define GTO(X, Y) "\x1B[" #X ";" #Y "f"
#define CUP(C)    "\x1B[" #C "A"
#define CDN(C)    "\x1B[" #C "B"
#define CLE(C)    "\x1B[" #C "D"
#define CRE(C)    "\x1B[" #C "C"


/* enums */
enum { ECHO_OFF, ECHO_ON };


/* function declarations */
char getch(int echo);
void putstr(char *str);
int readln(char *ln, int len);
int wasESC();

					// ioctl(1, TIOCGWINSZ, &ws);
					// char *tmp=getenv("COLUMNS");
