#include <stdio.h>
#include <stdlib.h>

int main(void) {
   printf("Dies ist ein IRC Server. Sie wurden gewarnt!\n");
   return system("/usr/bin/nc localhost 6667");
}
