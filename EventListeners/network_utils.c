/*
 * =====================================================================================
 *
 *       Filename:  network_utils.c
 *
 *    Description: This file contains routines to work with Network sockets programs 
 *
 
 * =====================================================================================
 */

#include "network_utils.h"

typedef struct thread_arg_pkg_{

	char ip_addr[16];
	uint32_t port_no;
	recv_fn_cb recv_fn;
} thread_arg_pkg_t;


/* UDP Server code*/

static void *
_udp_server_create_and_start(void *arg){

	thread_arg_pkg_t *thread_arg_pkg = 
		(thread_arg_pkg_t *)arg;

	char ip_addr[16];
	strncpy(ip_addr, thread_arg_pkg->ip_addr, 16);
	uint32_t port_no   = thread_arg_pkg->port_no;
	recv_fn_cb recv_fn = thread_arg_pkg->recv_fn;
	
	free(thread_arg_pkg);
	thread_arg_pkg = NULL;
	
	int udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP );

	if(udp_sock_fd == -1){
		printf("Socket Creation Failed\n");
		return 0;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family      = AF_INET;
	server_addr.sin_port        = port_no;
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(udp_sock_fd, (struct sockaddr *)&server_addr,
				sizeof(struct sockaddr)) == -1) {
		printf("Error : UDP socket bind failed\n");
		return 0;
	}

	char *recv_buffer = calloc(1, MAX_PACKET_BUFFER_SIZE);

    struct sockaddr_in client_addr;
    int bytes_recvd = 0,
       	addr_len = sizeof(client_addr);

	while(1){

		memset(recv_buffer, 0, MAX_PACKET_BUFFER_SIZE);

		/* block here to recv data, recvfrom is blocking system call by default*/
		bytes_recvd = recvfrom(udp_sock_fd, recv_buffer,
				MAX_PACKET_BUFFER_SIZE, 0, 
				(struct sockaddr *)&client_addr, &addr_len);

		recv_fn(recv_buffer, bytes_recvd, 
				network_covert_ip_n_to_p(
					(uint32_t)htonl(client_addr.sin_addr.s_addr), 0),
				client_addr.sin_port);	
	}
    return 0;
}


pthread_t *
udp_server_create_and_start(
        char *ip_addr,
        uint32_t udp_port_no,
		recv_fn_cb recv_fn){

    pthread_attr_t attr;
    pthread_t *recv_pkt_thread;
	thread_arg_pkg_t *thread_arg_pkg;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	thread_arg_pkg = calloc(1, sizeof(thread_arg_pkg_t));

	/*  pack all data in a single object which needs to be passed
 	 *  as argument to thread fn */
	strncpy(thread_arg_pkg->ip_addr, ip_addr, 16);
	thread_arg_pkg->port_no = udp_port_no;
	thread_arg_pkg->recv_fn = recv_fn;

	recv_pkt_thread = calloc(1, sizeof(pthread_t));

    pthread_create(recv_pkt_thread, &attr,
			_udp_server_create_and_start,
            (void *)thread_arg_pkg);

	return recv_pkt_thread;
}

int
send_udp_msg(char *dest_ip_addr,
             uint32_t dest_port_no,
             char *msg,
             uint32_t msg_size) {
 
	struct sockaddr_in dest;

    dest.sin_family = AF_INET;
    dest.sin_port = dest_port_no;
    struct hostent *host = (struct hostent *)gethostbyname(dest_ip_addr);
    dest.sin_addr = *((struct in_addr *)host->h_addr);
    int addr_len = sizeof(struct sockaddr);
	int sock_fd;

	sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if(sock_fd < 0){
		printf("socket creation failed, errno = %d\n", errno);
		return -1;
	}

    int rc = sendto(sock_fd, msg, msg_size,
            0, (struct sockaddr *)&dest,
            sizeof(struct sockaddr));
    close(sock_fd);
	return rc;
}


char *
network_covert_ip_n_to_p(uint32_t ip_addr,
                    char *output_buffer){

    char *out = NULL;
    static char str_ip[16];
    out = !output_buffer ? str_ip : output_buffer;
    memset(out, 0, 16);
    ip_addr = htonl(ip_addr);
    inet_ntop(AF_INET, &ip_addr, out, 16);
    out[15] = '\0';
    return out;
}

uint32_t
network_covert_ip_p_to_n(char *ip_addr){

    uint32_t binary_prefix = 0;
    inet_pton(AF_INET, ip_addr, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    return binary_prefix;
}
