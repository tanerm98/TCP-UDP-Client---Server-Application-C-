***   Taner Mustafa - 325 CB - Tema 2 Protocoale de Comunicatii - 05.2019    ***
	
	Pe langa comentariile din cod, care sunt destul de dese si clarificatoare, 
voi explica in linii mari cum functioneaza protocolul, ce cazuri speciale sau 
erori trateaza si ce nu trateaza.
	Nu voi explica cum se realizeaza conexiunea clientilor cu serverul sau 
multiplexarea pentru ca este standard si banal.

	Mesajele afisate:
		> engleza: cele cerute in enunt;
		> romana: erori si date suplimentare.
		
	Tema a fost facuta in C++ 11.

*** ------------------------------------------------------------------------ ***
>	Clientul TCP (subscriber.cpp):
	[Folosire: ./subscriber client_id server_address server_port];
	[Input: exit / subscribe topic SF / unsubscribe topic];
	
	> primeste date ori de la tastatura, ori de la server:
		> tastatura: comanda <exit> (care mai poate fi primita de la server);
		> server: primeste mesajul pe care trebuie doar sa-l afiseze, in forma 
			sa finala, parsat exact cum trebuie, fara sa mai necesite modificari.
			
	> cazuri speciale sau erori tratate:
		> erorile ce pot aparea la conexiunea cu serverul;
		> argumente incorecte in apelarea din CLI (se verifica client_id-ul, 
			IP-ul si portul serverului);
		> clientul se inchide odata cu serverul sau cu comanda de <exit> data 
			de server sau de utilizator;
(1)		> corectitudinea comenzilor date din clientul TCP este verificata in
			server in functia <parseTCPmessage> si este notificat clientul daca 
			a dat o comanda gresita, precum:
				> primul cuvant diferit de "subscribe" sau "unsubscribe";
				> al doilea cuvant nefiind un topic existent;
				> in cazul "subscribe", SF diferit de 0 si 1.


*** ------------------------------------------------------------------------ ***
>	Serverul (server.cpp):
	[Folosire: ./server server_port];
	[Input: exit];
	
	> primeste date ori de la tastatura, ori de la un client UDP sau TCP:
		> tastatura: comanda <exit> inchide serverul si clientii lui;
	(2)	> socket UDP: primeste un mesaj, il parseaza corect in topic, tip_topic 
			si continut si trimite la fiecare abonat al topicului mesajul care 
			contine toate datele cerute, inclusiv informatii despre clientul 
			UDP care a trimis mesajul;
	(3)	> socket TCP pasiv: primeste o cerere de conexiune si se conecteaza cu 
			noul client, primindu-i imediat si numele si asociind acest nume cu 
			portul clientului, intr-un map.
	(4)	> socket TCP: primeste un mesaj si il verifica sa fie asa cum a fost
			descris la (1) si adauga clientul in lista de abonati a unui topic 
			sau il dezaboneaza de la acel topic.
			
	> cazuri speciale sau erori tratate (pe langa cele pe care le enunt mai jos):
		> erorile ce pot aparea la conexiunea cu clientii;
		> argumente incorecte in apelarea din CLI (se verifica portul);
		> serverul se inchide la comanda <exit> data de utilizator;
		> comenzi diferite de <exit> date de utilizator;
		> mesaj primit gresit de la clientul UDP (topic, tip_topic, continut);
		> <client_id> deja existent printre clientii conectati, la cererea unui
			client TCP de a se conecta;
		> client deconectat.
			
------>>>    Voi explica cum functioneaza mai exact (2), (3) si (4)    <<<------

	> La conexiunea cu un client nou TCP, serverul primeste imediat si ID-ul sau.

	> Exista un map cu cheia <client_id> si valoarea <socketul> corespondent al 
		cheii. In map se insereaza atunci cand se realizeaza conexiunea cu un 
		client TCP nou. La terminarea conexiunii unui client, se sterge din map 
		perechea sa corespunzatoare. Daca se conecteaza din nou, acesta va avea 
		acelasi ID, dar un <socket> posibil diferit. Astfel, va putea inca primi 
		mesajele la care era abonat cat timp a fost inactiv.

	> Exista o lista de topice.
	> Fiecare element al listei contine numele topicului si o lista de 
		subscriberi.
	> Fiecare element al unei liste de subscriberi contine:
		> client_id-ul abonatului;
		> SF-ul sau (0 sau 1);
		> o lista de string-uri unde sunt retinute mesajele legate de topicul 
		la care este abonat clientul, care au fost primite cat timp clientul nu 
		era conectat (daca SF == 1).
	!!! Un client poate face parte din listele de subscriberi ale mai multor 
		topice, avand in fiecare instanta a sa stocate mesajele neprimite pentru 
		topicul respectiv.
		
	(2): Pentru topicul legat de care s-a primit mesaj de la UDP, se parcurg toti 
	abonatii, conform functiei <sendNews>:
		> Daca un abonat este inactiv si SF-ul lui este 1, se stocheaza 
			mesajele in lista sa de mesaje si se trimit cand acesta se conecteaza
			din nou.
		> Daca un abonat este activ, se trece mesajul nou in lista de mesaje, 
			se trimite catre abonat tot ce este in lista de mesaje (inclusiv 
			cele pe care le-a primit cat timp era inactiv) si se goleste lista 
			de mesaje.
		!!! Astfel, se trimit mereu toate mesajele pentru care un client era 
			abonat.
		!!! Daca topicul pentru care s-au primit date nu exista, acesta este 
			creat si mesajul curent se pierde, in lipsa de abonati.

	(3): Daca un client care doreste conectarea are <client_id> identic cu unul 
		deja conectat, ii este refuzata conectarea si acesta este notificat.
		
	(4): Se verifica sa fie comanda data corect, asa cum a fost descris la (1).
		Daca nu este data corect, se ignora si este notificat clientul.
		!!! Daca se doreste <subscribe> la un topic inexistent, acesta se creeaza 
			si asteapta notificari. Pentru <unsubscribe>, se ignora comanda, la 
			fel si daca clientul nu este abonat la topic.
			In toate cazurile, clientul este notificat.
		
		
*** ------------------------------------------------------------------------ ***
>	Cazuri netratate (exceptandu-le pe cele de care s-a spus ca nu sunt posibile,
	cum ar fi mesaje corupte sau topic format din mai multe cuvinte spatiate):
	
	> mesaje trunchiate / concatenate;
		> solutie: dupa fiecare mesaj trimis de clientul TCP sau de server catre 
			un client TCP, se pot introduce byti de control si mesajele, la 
			primire, se pot separa si trata fiecare in parte. Mesajele trunchiate 
			se pot stoca temporar intr-un buffer si trata abia cand se primeste 
			restul de mesaj.
	> mai mult de 100 de clienti TCP.

