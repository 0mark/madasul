enum { MPG, OGG, ALL,  };

char* typenames[]    = { "mp3", "mp2", "ogg", "*",   };
int types[]          = {  MPG,   MPG,   OGG,   ALL,  };

handler handlers[] = {
	{ "mpg123 %s", ERR },
	{ "ogg123 %s", ERR },
	{ "mplayer %s", 1 },
};
