#ifndef BUF_MANAGER_H
#define BUF_MANAGER_H
#define MAXBUF 128

typedef struct fd_sock_manager fd_sock_m;
typedef struct buf_manager{
	int fd;
	int idx;
	int remain;
	char start;
	char buf[MAXBUF];
	char isfitbuffer;
}mybuf;

void buf_add_idx(mybuf*, int add);
int buf_is_close_with_add(mybuf *,int add);
int buf_strncmp(mybuf*, char *, int buf_len);
void buf_cpy_len(char *des, mybuf *, int len);
int buf_get_number(mybuf *);
#endif
