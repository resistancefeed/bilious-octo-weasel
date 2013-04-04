#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/wait.h>



//~ ///****************************included following library to use the chroot and setuid function***///
//~ #include <unistd.h>
//~ 
//~ ///***********************************included following library to use setuid***///
//~ #include <sys/types.h>



#define TFTP_SRV_PORT 69

#define OPC_WRQ 2
#define OPC_RRQ 1
#define OPC_DATA 3
#define OPC_ACK 4
#define OPC_ERR 5

#define MODE_OCTET 0
#define MODE_NETASCII 1

#define PKT_BUF_SIZE 1024

#define BLOCK_SIZE 512
#define DATA_HDR_LEN 4
#define MAX_MODE_LENGTH 8

#define DATA_PKT_LEN BLOCK_SIZE+DATA_HDR_LEN

#define STARTING_BLOCK_NUM 1

#define MAX_RETRIES 5
#define TIMEOUT 10
#define MAX_FILENAME_SIZE 128

/* Macros for accessing fields in the request packet*/
#define PKT_OPCODE(p) ((short*)p)[0]

#define PKT_FILENAME(p) p+2
#define PKT_MODE(p)     (p+2+strlen(p+2)+1)

#define PKT_BLOCK(p) ((short*)(p+2))
#define PKT_DATA(p) p+4



void exitWithError() {
  perror("mytftp");
  exit(1);
}

/* Creates a datagram socket for the server. If port is 0, it uses an ephemeral port */

int createServerSocket(int port) {
  int srv_fd;
  if((srv_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    exitWithError();

  if(port > 0) { // REUSE_ADDR not needed for ephemeral ports
    int one=1;
    if(setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0)
      exitWithError();
  }

  struct sockaddr_in srv_addr;
  bzero(&srv_addr, sizeof(struct sockaddr_in));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(port);
  srv_addr.sin_addr.s_addr = INADDR_ANY;

  if(bind(srv_fd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0)
    exitWithError();
  return srv_fd;
}

int create_connected_socket(struct sockaddr_in* addr, int addr_len) {
  int fd = createServerSocket(0);
  if(connect(fd, (struct sockaddr*) addr, addr_len) < 0)
    exitWithError();

  return fd;
}

// netascii mode
int read_next_block(FILE* file, char* buf, int mode) {
  if(mode == MODE_NETASCII)
    exitWithError(); // netascii mode not implemented
  int bytesRead;
  if((bytesRead = fread(buf, 1, BLOCK_SIZE, file)) < 0)
    exitWithError();
  return bytesRead;
}

int main() {

  int srv_fd = createServerSocket(TFTP_SRV_PORT);


///*********chroot function is utilized to limit access to the clients***///
chroot(./tftpdir);

///*******Permanently remove root access from the user**///
setuid(getuid());


  // wait for connection attempt

  int packet_len;
  struct sockaddr_in client_addr;
  char packet[PKT_BUF_SIZE];
  socklen_t client_addr_len = sizeof(client_addr);
  char command[MAX_FILENAME_SIZE + 8];

  while(1) { // endless loop to handle multiple connections

    if((packet_len = recvfrom(srv_fd, packet, sizeof(packet), 0, (struct sockaddr*) &client_addr, &client_addr_len)) < 0)
      exitWithError();

    // check for valid request

    if(ntohs(PKT_OPCODE(packet)) != OPC_RRQ) {
      fprintf(stderr, "Invalid or Unsupported Opcode in Request Packet\n");
      continue; // wait for a new request packet ...
    }

	

	///*****I commented the below part out TO REMOVE IT because it is a potential vulnerability that***// 
	///******doesn't even provide a necessary utility to the program since the**********************//
	///******strerror() is used to return any error to opening a file with fopen below***************///
	///*****The vulnerability.txt file covers this in more detail.*************///
	/********************************************************************************
   // test if file exists
  sprintf(command, "[ -e %s ]", PKT_FILENAME(packet));
    int status;
    if((status = system(command)) < 0)
      exitWithError;
    if(WEXITSTATUS(status) != 0) // file does not exist
      continue;
	///*******************************************************************************////




    // check mode
    int mode = 0;
	char modeString[MAX_MODE_LENGTH];


	///*********I changed strcpy to strncpy to limit the amount of bytes that are copied to the buffer to ****///
	///****protect from overflowing the modestring buffer*****************///
	strncpy(modeString, PKT_MODE(packet),8);



    if(strcasecmp(modeString, "octet") == 0)
      mode = MODE_OCTET;
    else if(strcasecmp(modeString, "netascii") == 0) {
      mode = MODE_NETASCII;
	  continue; // incomplete -- netascii not supported
	}
    else
      continue;

    FILE* file;  
    if ((file = fopen(PKT_FILENAME(packet), "r")) == NULL) {
      fprintf(stderr, "Error opening file %s: %s", PKT_FILENAME(packet), strerror(errno));
      continue;
    }

    // create Connection-specific Socket on random port
    int conn_fd;
    conn_fd = create_connected_socket(&client_addr,client_addr_len);

    handle_read_request(conn_fd, file, mode);
  }
}

int handle_read_request(int conn_fd, FILE* file, int mode) {
  char send_buf[DATA_PKT_LEN];
  char recv_buf[PKT_BUF_SIZE];
  int block_num=STARTING_BLOCK_NUM;


  PKT_OPCODE(send_buf) = htons(OPC_DATA);
  int data_len;
  do {
    int retry_cnt = 0;
    // read 512 byte block from file (optional netascii conversion)
    *(PKT_BLOCK(send_buf)) = htons(block_num);
    data_len = read_next_block(file, PKT_DATA(send_buf), mode);

  retry:
    if(write(conn_fd, send_buf, DATA_HDR_LEN + data_len) < 0)
      exitWithError();

    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(cli_addr);

    // Call select first to create a timeout on the read

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(conn_fd, &fds);
    int maxfd=conn_fd;

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    int num_socks;
    if((num_socks = select(maxfd+1, &fds, (fd_set*) 0, (fd_set*) 0, &timeout)) < 0)
      exitWithError();
    if(!num_socks) {// timeout expired
      if(++retry_cnt < MAX_RETRIES)
	goto retry;
      else {
	    // incomplete - may need to send ERROR packet here
        goto closeConn;
      }
    }

    // if the socket is readable according to select , recvfrom should never block

    if(recvfrom(conn_fd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*) &cli_addr, &cli_addr_len) < 0) {
      if(errno==ECONNREFUSED)
	goto closeConn;
      else
	exitWithError();
    }

    // Check the received packet (check that it is from correct sender not
    // needed since we use a connected socket)

	// incomplete -- should also check for error packet from client
    if(PKT_OPCODE(recv_buf) != htons(OPC_ACK)) {
      if(++retry_cnt < MAX_RETRIES)
		goto retry;
      goto closeConn;
    }
    if((*PKT_BLOCK(recv_buf)) < htons(block_num)) {
      if(++retry_cnt < MAX_RETRIES)
		goto retry;
      goto closeConn;
    }
    block_num++;
  }
  while(data_len == BLOCK_SIZE);

closeConn:
  close(conn_fd);

  return 0;
}

