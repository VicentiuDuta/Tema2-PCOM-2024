#include <netinet/in.h>

#define UDP_MAX_SIZE 1552
#define TCP_MAX_SIZE 1500
#define MAX_ID_CLIENT_LEN 10
#define MAX_CLIENTS 128
#define MAX_TOPIC_LEN 50
#define MAX_UDP_DATA_SIZE 1500


struct udp_msg {
    char topic[MAX_TOPIC_LEN];
    unsigned char data_type;
    char data[MAX_UDP_DATA_SIZE];
};

struct recv_udp_data {
    struct udp_msg msg;
    struct sockaddr_in udp_client_addr;
};

struct tcp_msg {
    char type; // 1 - subscribe, 2 - unsubscribe, 3 - exit, 0 - connect
    char data[TCP_MAX_SIZE]; // topic in cazul subscribe/unsubscribe, id_client in cazul connect/exit
};

int server_confirmation = 0;






