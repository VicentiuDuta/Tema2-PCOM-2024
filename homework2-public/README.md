                                                        Tema 2 - Protocoale de Comunicatie - 2024
                                                           - Duta Vicentiu-Alecsandru - 321CC - 
1. structs.h:
    - struct udp_msg : folosit pentru mesajele primite de la clientii UDP, fiind format din:
                i) char topic[50] -> numele topicului
                ii) unsigned char data_type -> tipul datelor
                iii) char data[1500] -> datele mesajului

    - struct recv_udp_data : folosit pentru transmiterea mesajelor primite de server de la clientii UDP catre clientii TCP abonati la topicul respectiv. Acesta contine:
                i) struct udp_msg msg -> mesajul efectiv
                ii) struct sockaddr_in udp_client_addr -> folosit pentru a retine ip-ul si portul pe care a trimis clientul UDP mesajul catre server
    
    - struct tcp_msg : utilizat pentru a trimite mesaje la server de catre clienti. Contine:
                i) char type -> 0 pentru connect, 1 pentru subscribe, 2 pentru unsubscribe, 3 pentru exit
                ii) char data[1500] -> contine topic-ul in cazul subscribe/unsubscribe

2. server.cpp: 
    - Dezactivez buffering-ul la afisare

    - Configurez socketii udp si tcp

    - Dezactivez algoritmul lui Nagle

    - Pregatesc file descriptorii pentru citire

    - Am folosit 5 map-uri:
        i) clientID_sock -> leaga id-ul clientului de socket-ul acestuia
        ii) sock_clientID -> leaga socket-ul clientului de id-ul acestuia
        iii) clientID_topics -> leaga id-ul clientului de topic-urile la care e abonat
        iiii) topic_clientsID -> leaga topic-ul de id-urile clientilor care sunt abonati la el
        iiiii) client_connected -> leaga id-ul clientului de status-ul acestuia (connected - true, not connected - false)

    - Pornesc un infinite loop in care multiplexez file descriptorii.

    - Pentru fiecare file descriptor, selectez socketul pe care au venit date:
        i) primesc date pe socket-ul UDP:
            -> incadrez datele intr-o structura udp_msg
            -> trimit datele tuturor clientilor care sunt conectati si abonati la topicul respectiv 

        ii) primesc date pe socket-ul TCP:
            -> incadrez mesajul intr-o structura tcp_msg
            -> parsez mesajul in functie de type-ul acestuia. Daca type-ul e 0, iar clientul nu e deja conectat, permit conectarea acestuia.
        
        iii) primesc date de la tastatura:
            -> daca comanda este exit, inchid conexiunile cu toti clientii, opresc bucla infinita si inchid serverul.
        
        iiii) altfel, inseamna ca am primit date de la un client TCP:
            -> incadrez datele intr-o structura tcp_msg
            -> parsez mesajul in functie de type-ul acestuia:
                * [subscribe] - in cazul in care client-ul nu este abonat la topicul respectiv, trimit mesaj de confirmare client-ului ca este posibila abonarea la topicul respectiv. ALtfel, nu permit abonarea.
                * [unsubscribe] - similar cu subscribe

3. subscriber.cpp:
    - Dezactivez buffering-ul la afisare

    - configurez socket-ul TCP si il conectez la server

    - Dezactivez algoritmul lui Nagle

    - Pregatesc file descriptorii pentru citire

    - Pornesc un infinite loop in care multiplexez file descriptorii.

    - Pentru fiecare file descriptor, selectez socketul pe care au venit date:
        i) primesc date de la tastatura:
            -> citesc comanda si o sparg in tokeni, folosind functia splitCommandIntoTokens
                * [exit] - trimit un mesaj tcp_msg catre server, cu type-ul 3, opresc bucla infinita si inchid conexiunea cu serverul.
                * [subscribe] - trimit un mesaj tcp_msg catre server, cu type-ul 1, avand ca date numele topicului la care client-ul doreste sa se aboneze si astept confirmarea de la server.
                * [unsubscribe] - trimit un mesaj tcp_msg catre server, cu type-ul 2. avand ca date numele topicului de la care client-ul doreste sa se dezaboneze si astept confirmarea de la server.

        ii) primesc date de la server:
            -> incadrez mesajul intr-o structura recv_udp_data.
            -> afisez datele in functie de tipul acestora in modul specificat in enunt.
            


