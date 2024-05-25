#include <iostream>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iomanip>
#include <vector>

#include "structs.h"

using namespace std;

vector<string> splitCommandIntoTokens(string cmd, string delim) {
    vector<string> tokens;
    size_t pos = 0;
    while((pos = cmd.find(delim)) != string::npos) {
        tokens.push_back(cmd.substr(0, pos));
        cmd.erase(0, pos + delim.length());
    }
    tokens.push_back(cmd);
    return tokens;
}

int main(int argc, char* argv[]) {
    //verific numarul de argumente
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << "<CLIENT_ID> <IP_SERVER> <PORT>" << endl;
        exit(1);
    }

    //dezactivez buffering-ul la afisare
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    //extrag id-ul clientului si portul
    auto *id_client = argv[1];
    int port = atoi(argv[3]);
    if(port == 0) {
        cerr << "Invalid port" << endl;
        exit(1);
    }

    //configurez socket-ul tcp
    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_sockfd < 0) {
        cerr << "Error creating tcp socket" << endl;
        exit(1);
    }

    //configurez adresa serverului tcp
    struct sockaddr_in tcp_server_addr;
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = htons(port);
    // convertesc adresa IP a serverului
    int rc = inet_aton(argv[2], &tcp_server_addr.sin_addr);
    if(rc == 0) {
        cerr << "Invalid IP address" << endl;
        exit(1);
    }

    // conectez socket-ul la server
    rc = connect(tcp_sockfd, (struct sockaddr*)&tcp_server_addr, sizeof(tcp_server_addr));
    if(rc < 0) {
        cerr << "Error connecting to server" << endl;
        exit(1);
    }

    //dezactivez algoritmul lui Nagle
    int flag = 1;
    rc = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
    if(rc < 0) {
        cerr << "Error setting TCP_NODELAY" << endl;
        exit(1);
    }

    // trimit mesaj de conectare
    struct tcp_msg msg;
    msg.type = 0;
    strcpy(msg.data, id_client);
    rc = send(tcp_sockfd, &msg, sizeof(msg), 0);
    if(rc < 0) {
        cerr << "Error sending id_client" << endl;
        exit(1);
    }

    // Setez file descriptorii pentru select
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);

    fd_set tmp_fds;
    FD_ZERO(&tmp_fds);

    FD_SET(tcp_sockfd, &read_fds);

    // Aflu maximul dintre descriptorii de citire
    int fdmax = tcp_sockfd;
    bool ok = true;
    int i;

    while(ok) {
        tmp_fds = read_fds;
        rc = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        if(rc < 0) {
            cerr << "Error in select" << endl;
            exit(1);
        }

        for(i = 0; i <= fdmax; i++) {
            if(FD_ISSET(i, &tmp_fds)) {
                if(i == 0) {
                    // citesc de la tastatura
                    string line;
                    getline(cin, line);
                    // impart comanda in tokeni
                    vector<string> tokens = splitCommandIntoTokens(line, " ");
                    // exit
                    if(tokens[0] == "exit") {
                        struct tcp_msg msg;
                        msg.type = 3;
                        memset(msg.data, 0, sizeof(msg.data));
                        rc = send(tcp_sockfd, &msg, sizeof(msg), 0);
                        if(rc < 0) {
                            cerr << "Error sending exit message" << endl;
                            exit(1);
                        }
                        ok = false;
                        break;
                    }

                    // subscribe
                    else if(tokens[0] == "subscribe") {
                        struct tcp_msg msg;
                        msg.type = 1;

                        if(tokens.size() != 2) {
                            cerr << "Invalid command" << endl;
                            continue;
                        }

                        // copiez topicul in mesaj
                        strcpy(msg.data, tokens[1].c_str());
                        // trimit mesajul de subscribe
                        rc = send(tcp_sockfd, &msg, sizeof(msg), 0);
                        if(rc < 0) {
                            cerr << "Error sending subscribe message" << endl;
                            exit(1);
                        }

                        // astep raspuns de la server
                        rc = (int) recv(tcp_sockfd, &server_confirmation, sizeof(server_confirmation), 0);
                        if(rc < 0) {
                            cerr << "Error receiving confirmation" << endl;
                            exit(1);
                        }
                        if(server_confirmation == 1) {
                            cout << "Subscribed to topic " << tokens[1] << endl;
                        }
                        server_confirmation = 0;
                        continue;
                    }

                    // unsubscribe
                    else if(tokens[0] == "unsubscribe") {
                        struct tcp_msg msg;
                        msg.type = 2;

                        if(tokens.size() != 2) {
                            cerr << "Invalid command" << endl;
                            continue;
                        }

                        // copiez topicul in mesaj
                        strcpy(msg.data, tokens[1].c_str());
                        // trimit mesajul de unsubscribe
                        rc = send(tcp_sockfd, &msg, sizeof(msg), 0);
                        if(rc < 0) {
                            cerr << "Error sending unsubscribe message" << endl;
                            exit(1);
                        }

                        // astep raspuns de la server
                        rc = (int) recv(tcp_sockfd, &server_confirmation, sizeof(server_confirmation), 0);
                        if(rc < 0) {
                            cerr << "Error receiving confirmation" << endl;
                            exit(1);
                        }

                        if(server_confirmation == 1) {
                            cout << "Unsubscribed from topic " << tokens[1] << endl;
                        }
                        server_confirmation = 0;
                        continue;
                    }
                    
                }
                // daca am primit date de la server
                else if (i == tcp_sockfd) {
                    char buffer[sizeof(struct recv_udp_data) + 1];
                    memset(buffer, 0, sizeof(buffer));
                    rc = (int) recv(tcp_sockfd, buffer, sizeof(struct recv_udp_data), 0);
                    if(rc < 0) {
                        cerr << "Error receiving message" << endl;
                        exit(1);
                    }

                    if(rc == 0) {
                        cout << "Server disconnected" << endl;
                        close(tcp_sockfd);
                        return 0;
                    }
                    struct recv_udp_data udp_msg = *(struct recv_udp_data *) buffer;
                    struct udp_msg *msg = &udp_msg.msg;
                    unsigned char data_type = msg->data_type;     
                      switch (data_type)
                      {
                          case 0: // INT
                          {
                             int value;
                             memcpy(&value, msg->data + 1, sizeof(uint32_t));
                             value = ntohl(value);
                             if(msg->data[0] == 1){
                                 value = -value;
                             }
                             printf ("%s:%d - %s - INT - %d\n", inet_ntoa(udp_msg.udp_client_addr.sin_addr), ntohs(udp_msg.udp_client_addr.sin_port), msg->topic, value);
                             break;
                          }
                          case 1: // SHORT_REAL
                          {
                             uint16_t value;
                             memcpy(&value, msg->data, sizeof(uint16_t));
                             float real_value = ntohs(value) / 100.0;
                            printf ("%s:%d - %s - SHORT_REAL - %.2f\n", inet_ntoa(udp_msg.udp_client_addr.sin_addr), ntohs(udp_msg.udp_client_addr.sin_port), msg->topic, real_value);
                             break;                             
                          }

                             case 2: // FLOAT
                          {
                                 float value;
                                 uint32_t raw_value;
                                 memcpy(&raw_value, msg->data + 1, sizeof(uint32_t));
                                 raw_value = ntohl(raw_value);
                                 int power = msg->data[5];
                                 value = raw_value * pow(10, -power);
                                 if(msg->data[0] == 1) {
                                     value = -value;
                                 }
                                printf ("%s:%d - %s - FLOAT - %.4f\n", inet_ntoa(udp_msg.udp_client_addr.sin_addr), ntohs(udp_msg.udp_client_addr.sin_port), msg->topic, value);
                                 break;
                          }

                          case 3: // STRING
                          {
                            printf("%s:%d - %s - STRING - %s\n", inet_ntoa(udp_msg.udp_client_addr.sin_addr), ntohs(udp_msg.udp_client_addr.sin_port), msg->topic, msg->data);
                            break;
                          }
                        
                      }

                }
                
            }
        }
    }

    // inchid socket-ul
    close(tcp_sockfd);

    return 0;
}