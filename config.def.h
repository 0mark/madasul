//#define SOCKET_PORT          6666
static int socket_port = 6666;
//#define SOCKET_ADRESS      "127.0.0.1"
static char *socket_adress = "127.0.0.1";
#define BUF_SIZE           1024
#define TRACKS_BUF_SIZE	   128

enum { MPG, OGG,  };

char* play_cmds[][3] = {
	{ "/usr/bin/mpg321", "mpg321", "",  },
	{ "/usr/bin/ogg123", "ogg123", "",  },
};

char* typenames[]    = { "mp3", "mp2", "ogg",  };
int types[]          = {  MPG,   MPG,   OGG,   };

