#include <unistd.h>
#include <stdio.h>
#include <wait.h>

int main()
{
 int i = 0;
 printf("%d\n", getpid());
FORK:
 pid_t p = fork();
 i++;
 //if (i<3) {
 goto FORK;
 //}
 //while (wait(NULL) != -1);
 while (waitpid(p, NULL, 0) != -1);
 printf("(PID: %d, fork: %d)\n", getpid(), i);
 return 0;
}