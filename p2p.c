#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

static int mcast_socket(struct sockaddr_in *taddr, unsigned short tport, 
                        struct sockaddr_in *saddr, unsigned short sport, unsigned int ttl) {
   int                msock, on = 1;
   unsigned int       loop = 1;

   if ((msock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("socket");
      return -1;
   }

   assert(taddr != NULL);
   
   taddr->sin_family     = AF_INET;
   taddr->sin_port       = htons(tport);

   saddr->sin_family     = AF_INET;
   saddr->sin_port       = htons(sport);
   
   if (bind(msock, (struct sockaddr *)saddr, sizeof(struct sockaddr)) < 0) {
      perror("bind");
      return -1;
   }

   if (setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, 
                  &on, sizeof(on)) < 0) {
      perror("setsockopt");
      goto error;
   }
   
   if (!IN_MULTICAST(htonl(taddr->sin_addr.s_addr))) {
      fprintf(stderr, "Warning: %s is not a multicast address\n",
            inet_ntoa(taddr->sin_addr));
   } else {
      if ((setsockopt(msock, IPPROTO_IP,
                             IP_MULTICAST_TTL,
                             &ttl, sizeof(ttl)) < 0) ||
          (setsockopt(msock, IPPROTO_IP,
                             IP_MULTICAST_LOOP, 
                             &loop, sizeof(loop)) < 0)) {
         perror("setsockopt");
         goto error;
      }
   }

   return msock;

error:
   if (close(msock) < 0)
      perror("close");
   return -1;
}

/**
 * open a sending socket
 *
 * @param hostname multicast address to bind to 
 * @param port port to send on
 * @return -1 on error, file descriptor on success
 */
int mcast_send_socket(char *targetaddr, unsigned short tport, 
                      char *sourceaddr, unsigned short sport, int ttl) {
   struct sockaddr_in taddr, saddr;
   struct hostent     *lookup;
   int                msock;

   memset(&taddr, 0, sizeof(struct sockaddr_in));
   memset(&saddr, 0, sizeof(struct sockaddr_in));

   if (NULL == (lookup = gethostbyname(targetaddr))) {
      perror("gethostbyname for targetaddr");
      return -1;
   }
   memcpy(&taddr.sin_addr, lookup->h_addr_list[0], lookup->h_length);

   
   if (NULL == (lookup = gethostbyname(sourceaddr))) {
      perror("gethostbyname for sourceaddr");
      return -1;
   }
   memcpy(&saddr.sin_addr, lookup->h_addr_list[0], lookup->h_length);


   if ((msock = mcast_socket(&taddr, tport, &saddr, sport, ttl)) < 0)
      return -1;

   if (connect(msock, (struct sockaddr *)&taddr, sizeof(taddr)) < 0) {
      perror("connect");
      goto error;
   }

   return msock;

error:
   if (close(msock) < 0)
      perror("close");
   return -1;
}

#define MAXMESSAGES 100
int main(int argc, char *argv[]) {
   int   tport, sport;  
   int   msock;
   int   nummessages = 0;
   char *messages[MAXMESSAGES];
   int   buflen;
   char  buf[10000]; 
   char *bufptr;
   FILE *msgfile;   

   if (argc != 5) {
      fprintf(stderr, "%s <sourceaddr> <sourceport> <targetaddr> <targetport>\n", argv[0]);
      exit(1);
   }


   if (!(msgfile = fopen("config/prison/p2p/messages", "r"))) {
      perror("fopen");
      exit(1);
   }

   if ((buflen = fread(buf, 1, sizeof(buf) - 1, msgfile)) < 0) {
      fprintf(stderr, "Cannot read message file\n");
      exit(0);
   }

   buf[buflen] = '\0';

   for (bufptr = strtok(buf, "\n"); nummessages < MAXMESSAGES && bufptr; bufptr = strtok(NULL, "\n"))
      messages[nummessages++] = bufptr;

   if (fclose(msgfile) != 0) {
      perror("fclose");
      exit(1);
   }
    
   sport = atoi(argv[2]);
   tport = atoi(argv[4]);

   fprintf(stdout, "Sende von %s:%d nach %s:%d\n", argv[1], sport, argv[3], tport);
       
   if (-1 == (msock = mcast_send_socket(argv[3], tport, argv[1], sport, 1))) {
      fprintf(stderr, "Could not open mcast socket\n");
      exit(1);
   }

   srand(time(0));

   while (1) {
      char *message = messages[rand() % nummessages];
      
      if (send(msock, message, strlen(message), 0) < 0) {
         fprintf(stderr, "send() on multicast socket failed\n");
         exit(1);
      }

      sleep(1);
   }
   
   if (close(msock) < 0)
      perror("close");

   exit(0);
}


