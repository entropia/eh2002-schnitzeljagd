#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

/* Für polsyshandlecmd() */
#include "polsys.h"

void sigchild(int foo) {
    int status;
    int diedchild = wait(&status);
    printf("%d died\n", diedchild);
}

int handleclient(int fd) {
    char   *welcome="> Willkommen im neuen Polizeisystem\n"
                    "> -------------------------------------------------\n"
                    "> Achtung: Dies ist eine Alphaversion von Polsys II\n"
                    "> -------------------------------------------------\n"
                    "> HELP zeigt Hilfe an.\n";
    char    out[256]= {0};
    char    in[256] = {0};
    char   *inptr = in;
    int     len;

    write(fd, welcome, strlen(welcome));

    while((len = read(fd, inptr, 1000000)) > 0) { 
        in[len]=0;
        
        // Zu lange Befehle anfangen
        if (len > sizeof(in)) {  
            char *warn = "> Befehl zu lang\n";
            write(fd, warn, strlen(warn));
            return 0;
        }

        // Hilfe
        else if (!strncasecmp(in, "help", 4)) {
            char *help = "> Das Hilfesystem ist noch nicht implementiert.\n"
                         "> Bitte schlagen sie die vorhandenen Polsysbefehle\n"
                         "> in ihrem Polsys II Bedienungshandbuch nach.\n";
            write(fd, help, strlen(help));
        }
            
        // Beenden 
        else if (!strncasecmp(in, "quit", 4)) {
            char *quit= "> Polsys II beendet\n";
            write(fd, quit, strlen(quit));
            return 0;
        }
        
        // Polsys Befehle verarbeiten
        else if (polsyshandlecmd(in)) { 
            char *cmdok = "> Success\n";
            write(fd, cmdok, strlen(cmdok));
        }

        // Fehlermeldung bei unbekannten Befehlen ausgeben 
        else {
            char *unkn = "> Unknown command: ";
            write(fd, unkn, strlen(unkn));
            sprintf(out, in);
            write(fd, out, strlen(out));
        }
    }
    
    return 0;
}

int mainloop(int fd) {
    struct sockaddr_in client;
    int len = sizeof(client);
    int csock, running = 1;
    pid_t cpid;
    
    while (running) {
        csock = accept(fd, (struct sockaddr *)&client, &len);

        if (csock == -1) {
            sleep(10);
            continue;
        }

        switch (cpid = fork()) {
            case -1: // error 
                perror("fork");
                break;
            case 0:  // child 
                {
                    char *disconmsg = "> Verbindung beendet\n";
                    handleclient(csock);
                    write(csock, disconmsg, strlen(disconmsg));
                    shutdown (csock, SHUT_RDWR);
                    close(csock);
                    exit(0);
                }
                break;
            default: // parent
                fprintf(stdout, "new child: pid %d auf %d for %s:%d\n", cpid, csock, inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                close(csock);
                sleep(1);
                break;
        }
    }

    return 0;
}


int main(int argc, char *argv[]) {
    struct sockaddr_in  server;
    int                 lsock, port, on = 1;

    if (argc != 3) {
        fprintf(stderr, "%s <addr> <port>\n", argv[0]);
        exit(1);
    }

    if (signal(SIGCHLD, sigchild) == SIG_ERR) {
        fprintf(stderr, "cannot set signalhandler\n");
        exit(1);
    }


    if((lsock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    
    if (setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt");
        exit(1);
    }
 
    port = atoi(argv[2]);
    
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family     = AF_INET;
    server.sin_port       = htons(port);
    server.sin_addr.s_addr= inet_addr(argv[1]);

    if (bind(lsock, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(lsock, 1) != 0) {
        perror("listen");
        exit(1);
    }

    if (getgid() == 0 && setgid(1000) != 0) {
        perror("setgid");
        exit(1);
    }

    if (getuid() == 0 && setuid(1000) != 0) {
        perror("setuid");
        exit(1);
    }

    mainloop(lsock);

    if (close(lsock) == -1) 
        perror("close");
    
    return 0;
}
