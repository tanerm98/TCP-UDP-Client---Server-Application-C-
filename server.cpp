#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

#include <map>
#include <string>
#include <list>
#include <math.h>
#include <iostream>
using namespace std;

typedef struct client {
	string id;
	bool SF;
	std::list<string> messages;
} SClient;

typedef struct topics {
	string name;
	std::list<client> subscribers;
} STopics;


//pentru fiecare topic la care este abonat <client_id>, se trimit pe socketul
//<socket> mesajele primite cat timp acesta era inactiv.
void update (std::list<topics> &TOPS, string client_id, int socket) {

	for (std::list<topics>::iterator it = TOPS.begin(); it != TOPS.end(); ++it) {
				
		for (std::list<client>::iterator jt = it->subscribers.begin();
			jt != it->subscribers.end(); ++jt) {
			
			if (client_id.compare(jt->id) == 0) {
			
				std::list<string>::iterator kt;
				while (!jt->messages.empty()) {
					kt = jt->messages.begin();
					
					char temp[2000];
					memset (temp, 0, 2000);
					strcpy (temp, (*kt).c_str());
					strcat (temp, "\n");
					strcat (temp, "\0");
					send (socket, temp, strlen(temp), 0); 
					
					jt->messages.erase(kt);
				}
			}
		
		}
	}
}


//pentru fiecare subscriber abonat la <topic>, se trimite mesajul <message> sau
//se adauga in storage, daca SF == 1
void sendNews (std::map<string, int> TCPclients, std::list<topics> &TOPS, string topic, string message) {

	bool gasitTopic = false;
	
	for (std::list<topics>::iterator it = TOPS.begin(); it != TOPS.end(); ++it) {
		if (topic.compare(it->name) == 0) {
			gasitTopic = true;
				
			for (std::list<client>::iterator jt = it->subscribers.begin();
				jt != it->subscribers.end(); ++jt) {
				
				std::map<string,int>::iterator lt = TCPclients.find(jt->id);
				if (lt == TCPclients.end()) {
					if (jt->SF == 1) {
						jt->messages.push_back(message);
					}
				} else {
					jt->messages.push_back(message);
					for (std::list<string>::iterator kt = jt->messages.begin(); kt != jt->messages.end(); ++kt) {
						char temp[2000];
						strcpy (temp, (*kt).c_str());
						send (lt->second, temp, 2000, 0);
					}
					jt->messages.clear();
				}
			
			}
				
			break;
		}
	}
	
	//daca nu exista topicul pentru care s-a primit mesaj, acesta se creeaza
	if (!gasitTopic) {
		//topic nou
		topics nou;
		nou.name = topic;
		
		//se baga topicul in lista de topice
		TOPS.push_back(nou);
	}
}


void parseUDP (char UDP_IP[20], int port, char buffer[BUFLEN], std::map<string, int> TCPclients, std::list<topics> &TOPS) {

	char topic[51], tip[10], continut[1501];
	char tip_date;
	
	char sign;
	uint8_t x;
	uint16_t y;
	uint32_t z;
	
	int xx = 0;
	char b[4];
	b[3] = b[2] = b[1] = 0;
	
	memset (topic, 0, 51);
	memset (continut, 0, 1501);
	
	//aflam tipul de date si continutul
	memcpy (&tip_date, buffer + 50, 1);
	switch (tip_date) {
	
		case 0: 
			strcpy (tip, "INT");
			memcpy (&sign, buffer + 51, 1);
			memcpy (&z, buffer + 52, 4);
			
			if (sign == 1) {
				sprintf (continut, "-%d", ntohl(z));
			} else {
				sprintf (continut, "%d", ntohl(z));
			}
			break;
			
		case 1: 
			strcpy (tip, "SHORT_REAL");
			memcpy (&y, buffer + 51, 2);
			sprintf (continut, "%.4f", ntohs(y) / 100.00);
			break;
			
		case 2: 
			strcpy (tip, "FLOAT");
			memcpy (&sign, buffer + 51, 1);
			memcpy (&z, buffer + 52, 4);
			memcpy (&x, buffer + 56, 1);
			
			b[0] = x;
			xx = *((int*)(b));
			
			if (sign == 1) {
				sprintf (continut, "-%.4f", ((double)(ntohl(z))) * (pow(10.00, -xx)));
			} else {
				sprintf (continut, "%.4f", ((double)(ntohl(z))) * (pow(10.00, -xx)));
			}
			break;
			
		case 3: 
			strcpy (tip, "STRING");
			buffer[strlen(buffer)] = 0;
			strcpy (continut, buffer + 51);
			break;
			
		default: 
			printf ("Primit tip de date gresit de la clientul UDP %s:%d!\n",
				UDP_IP, port);
			break;
	}

	
	//aflam topicul
	buffer[50] = 0;
	strcpy (topic, buffer);
	
	char mesaj[2000];
	sprintf (mesaj, "%s:%d - %s - %s - %s", UDP_IP, port, topic, tip, continut);
	
	string stopic (topic);
	string smesaj (mesaj);
	
	sendNews (TCPclients, TOPS, stopic, smesaj);
	
}


//parseaza comanda unui client TCP
void parseTCPmessage (string client_id, char buffer[BUFLEN], std::list<topics> &TOPS, int socket) {

	char *p, err[256];
	bool gasitTopic = false, gasitSubscr = false;
	
	p = strtok (buffer, " ");
	//se verifica daca s-a dat subscribe sau unsubscribe
	if ((strcmp (p, "subscribe") != 0) && (strcmp (p, "unsubscribe") != 0)) {
		printf ("Comanda invalida (<subscribe topic SF> / <unsubscribe topic>)!\n");
		strcpy (err, "Comanda invalida (<subscribe topic SF> / <unsubscribe topic>)!");
	    send (socket, err, strlen(err), 0);
		return;
	}
	
	
	//cazul in care s-a dat comanda "unsubscribe"
	if (strcmp (p, "unsubscribe") == 0) {
		p = strtok (NULL, " ");
		//se verifica daca exista si al doilea argument (topicul)
		if (p == NULL) {
			printf ("Comanda invalida (<unsubscribe topic>)!\n");
			strcpy (err, "Comanda invalida (<unsubscribe topic>)!");
	    	send (socket, err, strlen(err), 0);
			return;
		}
		
		string topicName (p);
		//se gaseste topicul si se sterge din el subscriberul
		for (std::list<topics>::iterator it = TOPS.begin(); it != TOPS.end(); ++it) {
			
			//daca s-a gasit numele topicului cautat
			if (topicName.compare(it->name) == 0) {
				gasitTopic = true;
				
				//se cauta subscriberul
				for (std::list<client>::iterator jt = it->subscribers.begin();
					jt != it->subscribers.end(); ++jt) {
					
					//daca s-a gasit, se sterge
					if (client_id.compare(jt->id) == 0) {
						gasitSubscr = true;
						//frees memory
						jt->messages.clear();
						it->subscribers.erase(jt);
						
						send (socket, "unsubscribed topic", 25, 0);
						return;
					}
				}
				break;
			}
		}
		
		if (!gasitTopic) {
			cout << "Topicul " << topicName << " inexistent momentan.\n";
			strcpy (err, "Topicul cerut inexistent momentan!");
	    	send (socket, err, strlen(err), 0);
			return;
		}
		if (!gasitSubscr) {
			cout << "Clientul " << client_id << " nu este abonat la " << topicName << "!\n";
			strcpy (err, "Nu sunteti abonat la topicul specificat!");
	    	send (socket, err, strlen(err), 0);
			return;
		}
	}
	
	
	//cazul in care s-a dat comanda "subscribe"
	else if (strcmp (p, "subscribe") == 0) {
		
		//se verifica daca exista si al doilea argument (topicul)
		p = strtok (NULL, " ");
		if (p == NULL) {
			printf ("Comanda invalida (<subscribe topic SF>)!\n");
			strcpy (err, "Comanda invalida (<subscribe topic SF>)!");
	    	send (socket, err, strlen(err), 0);
			return;
		}
		string topicName (p);
		
		//se verifica daca exista si al 3lea argument
		p = strtok (NULL, " ");
		if (p == NULL) {
			printf ("Comanda invalida (<subscribe topic SF>)!\n");
			strcpy (err, "Comanda invalida (<subscribe topic SF>)!");
	    	send (socket, err, strlen(err), 0);
			return;
		}
		int SF = *p - '0';
		if ((SF != 0) && (SF != 1)) {
			printf ("SF trebuie sa fie 0 sau 1!\n");
			strcpy (err, "SF trebuie sa fie 0 sau 1!");
	    	send (socket, err, strlen(err), 0);
			return;
		}
		
		//se gaseste topicul si se sterge din el subscriberul
		for (std::list<topics>::iterator it = TOPS.begin(); it != TOPS.end(); ++it) {
			
			//daca s-a gasit numele topicului cautat
			if (topicName.compare(it->name) == 0) {
				gasitTopic = true;
				
				//se cauta subscriberul daca este deja abonat
				for (std::list<client>::iterator jt = it->subscribers.begin();
					jt != it->subscribers.end(); ++jt) {
					
					//daca s-a gasit, se afiseaza aceasta informatie
					if (client_id.compare(jt->id) == 0) {
						cout << "Clientul " << client_id << " este deja abonat la " << topicName << "!\n";
						strcpy (err, "Sunteti deja abonat la topicul specificat!");
	    				send (socket, err, strlen(err), 0);
						return;
					}
				}
				
				//altfel, se aboneaza
				client nou;
				nou.id = client_id;
				nou.SF = SF;
				it->subscribers.push_back(nou);
				
				send (socket, "subscribed topic", 25, 0);
				return;
			}
		}
		
		//daca topicul nu exista, acesta se creeaza
		if (!gasitTopic) {
			cout << "Topicul " << topicName << " inexistent! Se creeaza!\n";
			strcpy (err, "Topicul specificat nu exista! S-a creat!");
	    	send (socket, err, strlen(err), 0);
	    	
	    	//topic nou
	    	topics nou;
	    	nou.name = topicName;
	    	
	    	//abonat nou la topic
	    	client nouu;
	    	nouu.id = client_id;
	    	nouu.SF = SF;
	    	
	    	//se baga abonatul in topic si topicul in lista de topice
	    	nou.subscribers.push_back(nouu);
	    	TOPS.push_back(nou);
			return;
		}
	}
	
}


int main(int argc, char *argv[])
{
	int sockfdTCP, sockfdUDP, newsockfd, portno;
	char buffer[BUFLEN], err[256];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;
	
	//mapa in care retinem numele si socketul clientilor TCP
    std::map<string, int> TCPclients;
    std::map<string, int> history;
    std::pair<std::map<string,int>::iterator,bool> res;
    std::map<string,int>::iterator it;
    
    std::list<topics> TOPS;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc != 2) {
		printf("Folosire: ./server server_port\n");
		return -1;
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);



    // se realizeaza conexiunea pentru clientii TCP
	sockfdTCP = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfdTCP < 0) {
	    printf("Deschidere socket TCP esuata!\n");
	    return -1;
	}

	portno = atoi(argv[1]); //portul dat ca argument
	if (portno == 0) {
	    printf("Port introdus gresit!\n");
	    return -1;
	}

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfdTCP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	if (ret < 0) {
	    printf("Bind cu TCP esuat! Incercati alt port!\n");
	    return -1;
	}

	ret = listen(sockfdTCP, MAX_CLIENTS);
	if (ret < 0) {
	    printf("Ascultare esuata!\n");
	    return -1;
	}

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfdTCP, &read_fds);
	
	
	// se realizeaza conexiunea pentru clientii UDP
    sockfdUDP = socket(AF_INET,SOCK_DGRAM, 0);
    if (sockfdUDP < 0) {
	    printf("Deschidere socket UDP esuata!\n");
	    return -1;
	}
	
	ret = bind(sockfdUDP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	if (ret < 0) {
	    printf("Bind cu UDP esuat!\n");
	    return -1;
	}
	
	FD_SET(sockfdUDP, &read_fds);
	fdmax = sockfdTCP > sockfdUDP ? sockfdTCP : sockfdUDP;
	
	FD_SET(STDIN_FILENO, &read_fds);
	

	while (1) {
		tmp_fds = read_fds;
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		if (ret < 0) {
	        printf("Selectare esuata!\n");
	        return -1;
	    }

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
			
				//citire de la tastatura
			    if (i == STDIN_FILENO) {
			    
			        memset(buffer, 0, BUFLEN);
			        fgets(buffer, BUFLEN - 1, stdin);
			        
			        if (strncmp(buffer, "exit", 4) == 0) {
			            printf ("Server closing!\n");
			            close (sockfdTCP);
						close (sockfdUDP);
			            return 0;
			        } else {
			            printf ("Server only accepts <exit> command!\n");
			        }
			    }
			    
			    //comunicare cu client UDP
			    else if (i == sockfdUDP) {
			    
			        struct sockaddr_in source;
			        socklen_t socklen;
			        
	                n = recvfrom(sockfdUDP, (char *)buffer, BUFLEN, 0, (struct sockaddr *)&source, &socklen);
	                if (n < 0) {
	                    printf("Primire mesaj de la UDP esuata!\n");
	                    continue;
	                }

					// conexiunea s-a inchis
					if (n == 0) {
						close(i);
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					}
	                
	                char UDP_IP[20];
	                strcpy (UDP_IP, inet_ntoa(cli_addr.sin_addr));
	                int port = ntohs(cli_addr.sin_port);
	                
	                parseUDP (UDP_IP, port, buffer, TCPclients, TOPS);
			    }
			    
			    //conexiune noua TCP
				else if (i == sockfdTCP) {
				
					// a venit o cerere de conexiune TCP pe socketul inactiv 
					//(cel cu listen), pe care serverul o accepta;
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfdTCP, (struct sockaddr *) &cli_addr, &clilen);
					if (newsockfd < 0) {
	                    printf("Acceptare esuata!\n");
	                    return -1;
	                }

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) {
						fdmax = newsockfd;
					}
                    
                    //se primeste si client_id-ul
                    char client_id[11];
                    memset (client_id, 0, 11);
                    n = recv(newsockfd, client_id, 10, 0);
                    if (n < 0) {
	                    printf("Primire ID client esuata!\n");
	                    return -1;
	                }
	                
	                string value (client_id);
	                
	                //verific daca deja exista un client cu acest ID
	                it = TCPclients.find(value);
	                if (it != TCPclients.end()) {
	                	printf ("Clientul cu ID-ul %s deja este conectat!\n", client_id);
	                	
	                	strcpy (err, "Un client cu ID-ul dvs este deja conectat!");
	                	send (newsockfd, err, strlen(err), 0);
	                	
	                	FD_CLR(newsockfd, &read_fds);
	                	close (newsockfd);
	                	continue;
	                } else {
	                	//introduc clientul in map
	                	TCPclients.insert (std::pair<string, int>(value, newsockfd));
	                	
	                	//verific daca acest client a mai fost conectat inainte;
	                	//daca da, i se trimit mesajele la care era abonat cat
	                	//timp a fost inactiv. Altfe, se adauga in history.
	                	it = history.find(value);
	                	if (it != history.end()) {
	                		printf ("Acest client a mai fost conectat!\n");
	                		update(TOPS, value, newsockfd);
	                	} else {
	                		history.insert (std::pair<string, int>(value, newsockfd));
	                	}
	                }
	                
                    printf("New client (%s) connected from %s:%d.\n", 
                            client_id, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                    
                    //daca a mai fost conectat inainte, se trimit mesajele la 
                    //care a fost abonat cat timp a fost inactiv;
						
				// mesaj de la un client TCP existent			
				} else {
				
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, BUFLEN, 0);
					if (n < 0) {
	                    printf("Primire mesaj de la TCP esuata!\n");
	                    continue;
	                }

					// conexiunea s-a inchis
					if (n == 0) {
						close(i);
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
						
						//se sterge din map-ul conexiunilor active
						for (it = TCPclients.begin(); it != TCPclients.end(); ++it) {
							if (it->second == i) {
								cout << "Client " << it->first << " disconnected.\n";
								TCPclients.erase(it);
								break;
							}
						}
					
					//se primeste comanda de la un client TCP
					} else {
						if (buffer[strlen(buffer) - 1] == 10) {
							buffer[strlen(buffer) - 1] = 0;
						}
                        //se vede daca e subscribe sau unsubscribe si se seteaza
                        //topicul unde trebuie
                        string id;
                        for (it = TCPclients.begin(); it != TCPclients.end(); ++it) {
							if (it->second == i) {
								id = it->first;
								break;
							}
						}
						
						// se parseaza comanda si se executa
                        parseTCPmessage (id, buffer, TOPS, i);
					}
				}
				
			}
		}
	}

	close (sockfdTCP);
	close (sockfdUDP);

	return 0;
}
