cc -D_REENTRANT -lpthread madasul.c -o madasul && { rm ~/.madasul_sock; cat list | ./madasul; }
