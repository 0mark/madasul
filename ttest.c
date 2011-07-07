#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t mutex;
char ch;

void *print_stern (void *dummy)
{
  pthread_mutex_lock (&mutex);
  ch = '*';
  sleep (1);
  printf ("%c\n", ch);
  pthread_mutex_unlock (&mutex);
  return NULL;
}

void *print_minus (void *dummy)
{
  pthread_mutex_lock (&mutex);
  ch = '-';
  sleep (1);
  printf ("%c\n", ch);
  pthread_mutex_unlock (&mutex);
  return NULL;
}

int main ()
{
  pthread_t p1, p2;

  pthread_mutex_init (&mutex, NULL);

  pthread_create (&p1, NULL, print_minus, NULL);
  pthread_create (&p2, NULL, print_stern, NULL);

  pthread_join (p1, NULL);
  pthread_join (p2, NULL);

  return 0;
}
