#include "fd_sock.h"

int main(){
	fd_sock_m *m=fd_sock_init("127.0.0.1",9999,REDIS);
	netdata temp;
	while(1){
		fd_sock_read_redis(m,&temp);
		fd_sock_write_redis(m,&temp);
	}
	return 1;
}
