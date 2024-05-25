#include <iostream>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "structs.h"

using namespace std;

int main(int argc, char *argv[]) {
    //dezactivez buffering-ul la afisare
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    //verific numarul de argumente
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <PORT>" << endl;
        exit(1);
    }

    //extrag portul
    int port = atoi(argv[1]);
    if (port == 0) {
        cerr << "Invalid port" << endl;
        exit(1);
    }

    //configurez socket-ul udp
    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        cerr << "Error creating udp socket" << endl;
        exit(1);
    }

    //configurez adresa serverului udp
    auto *udp_server_addr = new sockaddr_in;
    udp_server_addr->sin_family = AF_INET;
    udp_server_addr->sin_port = htons(port);
    udp_server_addr->sin_addr.s_addr = INADDR_ANY;

    //leg socket-ul de adresa serverului
    int rc = bind(udp_sockfd, (struct sockaddr *) udp_server_addr, sizeof(*udp_server_addr));
    if (rc < 0) {
        cerr << "Error binding udp socket" << endl;
        exit(1);
    }

    //configurez socket-ul tcp
    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sockfd < 0) {
        cerr << "Error creating tcp socket" << endl;
        exit(1);
    }

    //configurez adresa serverului tcp
    auto *tcp_server_addr = new sockaddr_in;
    tcp_server_addr->sin_family = AF_INET;
    tcp_server_addr->sin_port = htons(port);
    tcp_server_addr->sin_addr.s_addr = INADDR_ANY;

    //leg socket-ul de adresa serverului
    rc = bind(tcp_sockfd, (struct sockaddr *) tcp_server_addr, sizeof(*tcp_server_addr));
    if (rc < 0) {
        cerr << "Error binding tcp socket" << endl;
        exit(1);
    }

    //ascult pentru conexiuni tcp
    rc = listen(tcp_sockfd, MAX_CLIENTS);
    if (rc < 0) {
        cerr << "Error listening for tcp connections" << endl;
        exit(1);
    }

    // dezactivez algoritmul lui Nagle
    int flag = 1;
    rc = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    if (rc < 0) {
        cerr << "Error setting TCP_NODELAY" << endl;
        exit(1);
    }

    //setez descriptorii de citire
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);

    //folosim un file descriptor temporar pentru a nu pierde date
    fd_set tmp_fds;
    FD_ZERO(&tmp_fds);

    //adaugam socketii udp si tcp in multimea de descriptori
    FD_SET(udp_sockfd, &read_fds);
    FD_SET(tcp_sockfd, &read_fds);

    // retin descriptorul maxim
    int fdmax = max(udp_sockfd, tcp_sockfd);

    //map client_id -> socket
    unordered_map<string, int> clientID_sock;
    //map socket -> client_id
    unordered_map<int, string> sock_clientID;
    //map client_id -> topics
    unordered_map<string, vector<string>> clientID_topics;
    //map topic -> client_id
    unordered_map<string, vector<string>> topic_clientsID;
    // map client -> connection_status
    unordered_map<string, bool> client_connected;

    bool ok = true;
    while (ok) {
        tmp_fds = read_fds;
        //asteptam date de la clienti
        rc = select(fdmax + 1, &tmp_fds, nullptr, nullptr, nullptr);
        if (rc < 0) {
            cerr << "Error in select" << endl;
            exit(1);
        }

        for(int i = 0; i <= fdmax; i++) {
            // verific daca descriptorul este in multimea de descriptori de citire
            if(FD_ISSET(i, &tmp_fds) == true) {
                if(i == udp_sockfd) {
                    char buffer[UDP_MAX_SIZE];
                    memset(buffer, 0, UDP_MAX_SIZE);
                    auto *udp_client_addr = new sockaddr_in;
                    socklen_t udp_client_addr_len = sizeof(*udp_client_addr);
                    //primesc mesaj de la client udp
                    rc = (int) recvfrom(udp_sockfd, buffer, UDP_MAX_SIZE, 0, (struct sockaddr *) udp_client_addr, &udp_client_addr_len);
                    if(rc < 0) {
                        cerr << "Error receiving udp message" << endl;
                        exit(1);
                    }
                    if(rc == 0) {
                        continue;
                    }
                    // incadram mesajul primit
                    struct udp_msg *msg = (struct udp_msg *) buffer;
                    if(topic_clientsID[msg->topic].empty()) {
                        continue;
                    }

                    // trimit mesajul la toti clientii abonati la topic
                    for(auto client : topic_clientsID[msg->topic]) {
                        // daca clientul nu e conectat, sarim peste
                        if(client_connected[client] == false) {
                            continue;
                        }
                        int client_fd = clientID_sock[client];
                        struct recv_udp_data data;
                        data.msg = *msg;
                        data.udp_client_addr = *udp_client_addr;
                        rc = sendto(client_fd, &data, sizeof(struct recv_udp_data), 0, (struct sockaddr *) udp_client_addr, udp_client_addr_len);
                        if(rc < 0) {
                            cerr << "Error sending udp message to client" << endl;
                            exit(1);
                        }
                    }

                    continue;
                }
                // daca am primit o conexiune noua pe socketul tcp
                if(i == tcp_sockfd) {
                    // dezactivez algoritmul lui Nagle
                    int flag = 1;
                    rc = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                    if(rc < 0) {
                        cerr << "Error setting TCP_NODELAY" << endl;
                        exit(1);
                    }
                    struct sockaddr_in tcp_client_addr;
                    socklen_t tcp_client_addr_len = sizeof(tcp_client_addr);

                    // accept conexiunea
                    int new_tcp_sockfd = accept(tcp_sockfd, (struct sockaddr *) &tcp_client_addr, &tcp_client_addr_len);
                    if(new_tcp_sockfd < 0) {
                        cerr << "Error accepting tcp connection" << endl;
                        exit(1);
                    }
                    struct tcp_msg msg;
                    // primesc mesajul de la noul client
                    rc = recv(new_tcp_sockfd, &msg, sizeof(struct tcp_msg), 0);
                    if(rc < 0) {
                        cerr << "Error receiving tcp message" << endl;
                        exit(1);
                    }

                    // daca clientul incearca sa se conecteze
                    if(msg.type == 0) {
                        string client_id = (char*) msg.data;
                        // verificam daca clientul e deja conectat
                        if(client_connected[client_id] == true) {
                            cout << "Client " << client_id << " already connected." << endl;
                            close(new_tcp_sockfd);
                            continue;
                        }

                        // verificam daca clientul a mai fost conectat
                        if (clientID_sock.find(client_id) != clientID_sock.end()) {
                            // actualizam socketul clientului
                            clientID_sock[client_id] = new_tcp_sockfd;
                            sock_clientID[new_tcp_sockfd] = client_id;
                            client_connected[client_id] = true;
                        }
                        // daca clientul nu a mai fost conectat
                        else {
                            // adaugam clientul in map-uri
                            clientID_sock[client_id] = new_tcp_sockfd;
                            sock_clientID[new_tcp_sockfd] = client_id;
                            client_connected[client_id] = true;
                        }

                        cout << "New client " << client_id << " connected from " << inet_ntoa(tcp_client_addr.sin_addr) << ":" << ntohs(tcp_client_addr.sin_port) << "." << endl;
                    }

                    // updatez descriptorii de citire
                    FD_SET(new_tcp_sockfd, &read_fds);
                    fdmax = max(fdmax, new_tcp_sockfd);
                    continue;
                }

                if(i == STDIN_FILENO) {
                    string comanda;
                    cin >> comanda;

                    if(comanda == "exit") {
                        // inchid conexiunile cu toti clientii
                        for(int j = 1; j <= fdmax; j++) {
                            if(FD_ISSET(j, &read_fds))
                                if(j != tcp_sockfd && j != udp_sockfd && j != STDIN_FILENO) {
                                    close(j);
                                    FD_CLR(j, &read_fds);
                                }
                        }
                        ok = false;
                        break;
                    }

                    continue;
                }
                // daca am primit date de la un client tcp
                else {
                    struct tcp_msg msg;
                    rc = (int) recv(i, &msg, sizeof(struct tcp_msg), 0);
                    if(rc < 0) {
                        cerr << "Error receiving tcp message" << endl;
                        exit(1);
                    }

                    string client_id = sock_clientID[i];
                    // daca clientul vrea sa se deconecteze
                    if(msg.type == 3) {
                        cout << "Client " << client_id << " disconnected." << endl;
                        // marcam clientul ca deconectat
                        client_connected[client_id] = false;
                        // inchidem conexiunea
                        close(i);
                        FD_CLR(i, &read_fds);
                        FD_CLR(i, &tmp_fds);

                        sock_clientID.erase(i);
                        clientID_sock.erase(client_id);

                        continue;
                    }

                    // daca clientul vrea sa se aboneze la un topic
                    if(msg.type == 1) {
                        string topic = (char*) msg.data;
                        // verificam daca clientul nu e deja abonat la topic
                        if(find(clientID_topics[client_id].begin(), clientID_topics[client_id].end(), topic) == clientID_topics[client_id].end()) {
                            // adaugam topicul in map-urile corespunzatoare
                            clientID_topics[client_id].push_back(topic);
                            topic_clientsID[topic].push_back(client_id);

                            // trimitem confirmarea clientului
                            server_confirmation = 1;
                            rc = send(i, &server_confirmation, sizeof(server_confirmation), 0);
                            if(rc < 0) {
                                cerr << "Error sending confirmation" << endl;
                                exit(1);
                            }
                            server_confirmation = 0;
                        }
                        else {
                            server_confirmation = 0;
                            rc = send(i, &server_confirmation, sizeof(server_confirmation), 0);
                            if(rc < 0) {
                                cerr << "Error sending confirmation" << endl;
                                exit(1);
                            }
                        }

                        continue;
                    }

                    // daca clientul vrea sa se dezaboneze de la un topic
                    if(msg.type == 2) {
                        string topic = (char*) msg.data;
                        // stergem topicul din map-urile corespunzatoare daca clientul e abonat la el
                        if(find(clientID_topics[client_id].begin(), clientID_topics[client_id].end(), topic) != clientID_topics[client_id].end()) {
                            clientID_topics[client_id].erase(find(clientID_topics[client_id].begin(), clientID_topics[client_id].end(), topic));
                            topic_clientsID[topic].erase(find(topic_clientsID[topic].begin(), topic_clientsID[topic].end(), client_id));

                            // trimitem confirmarea clientului
                            server_confirmation = 1;
                            rc = send(i, &server_confirmation, sizeof(server_confirmation), 0);
                            if(rc < 0) {
                                cerr << "Error sending confirmation" << endl;
                                exit(1);
                            }
                            server_confirmation = 0;
                        }
                        else {
                            server_confirmation = 0;
                            rc = send(i, &server_confirmation, sizeof(server_confirmation), 0);
                            if(rc < 0) {
                                cerr << "Error sending confirmation" << endl;
                                exit(1);
                            }
                        }

                        continue;
                    }

                    continue;
                }
            } 
        }
    }

    //inchid socketii
    close(udp_sockfd);
    close(tcp_sockfd);
    return 0;
}

