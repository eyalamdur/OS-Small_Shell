#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
void seg_fault_catcher(int signum)
{
 printf("%d\n", signum);
 exit(0);
}

// void seg_fault_catcher(int signum)
// {
//     printf("%d\n", signum);
//     signal(SIGFPE, seg_fault_catcher);
//     exit(3/(11-signum));
// } 

int main()
{
    signal(SIGSEGV, SIG_IGN);
    int* x = (int*)malloc(4*sizeof(int));
    for(int i=0; i<4; i++) {
        x[i] = i;
    }
    int j = 4;
    while(1) {
        j--;
        printf("%d,", x[j]);
    }
    printf("Hi\n");
    return 0;
} 
