#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];


	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if (argc != 4) {
		printf("Folosire: ./subscriber client_id server_address server_port\n");
		return -1;
	}
	
	if (strlen(argv[1]) > 10) {
	    printf("ID client prea lung (maxim 10 caractere)!\n");
	    return -1;
	}

    //se creeaza socketul de comunicare cu serverul
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
	    printf("Eroare in crearea socketului!\n");
	    return 0;
	}

    //se introduc datele serverului
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));  //portul e dat ca argument
	ret = inet_aton(argv[2], &serv_addr.sin_addr);  //IP-ul e dat ca argument
	if (ret < 0) {
	    printf("Introduceti IP-ul corect al serverului!\n");
	    return 0;
	}

    //se conecteaza cu serverul
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	if (ret < 0) {
	    printf("Eroare la conexiunea cu serverul (verificati portul introdus)!\n");
	    return 0;
	}
	
	ret = send(sockfd, argv[1], strlen(argv[1]), 0);
	if (ret < 0) {
        printf("Eroare la trimiterea ID-ului de client!\n");
        return 0;
    }

    // se seteaza socketul catre server si inputul de la tastatura
	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds;
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if (ret < 0) {
	        printf("Eroare la selectarea socketului!\n");
	        return 0;
	    }

        //daca se citeste de la tastatura
		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);
			
			if (strncmp(buffer, "exit", 4) == 0) {
			    printf ("Clientul %s se inchide!\n", argv[1]);
			    return 0;
			}

			// se trimite mesaj catre server
			ret = send(sockfd, buffer, strlen(buffer), 0);
			if (ret < 0) {
	            printf("Eroare la trimiterea mesajului!\n");
	            return -1;
	        }
	        //corectitudinea comenzii e verificata in server, nu in client
		}

        //daca se realizeaza o conexiune cu serverul
		if (FD_ISSET(sockfd, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			ret = recv(sockfd, buffer, BUFLEN, 0);
			
			// cazul in care serverul se inchide sau nu se primeste corect
			if (ret <= 0) {
	            printf("Transmisiune incheiata!\n");
	            return -1;
	        }
	        
	        //cazul in care se primeste comanda de exit
	        if (strcmp(buffer, "exit") == 0) {
	            printf("Clientul %s se inchide!\n", argv[1]);
	            return -1;
	        }
	        
	        printf ("%s\n", buffer);
	        
		}

	}

	close(sockfd);

	return 0;
}
