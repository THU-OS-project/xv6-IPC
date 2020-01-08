struct stat;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(char*, int);
int mknod(char*, short, short);
int unlink(char*);
int fstat(int fd, struct stat*);
int link(char*, char*);
int mkdir(char*);
int chdir(char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int send(int sender_pid, int rec_pid, void *msg);
int recv(void *msg);
int sig_set(int sig_num, sighandler_t handler);
int sig_send(int dest_pid, int sig_num, void *sig_arg);
int sig_pause(void);
int sig_ret(void);
int send_multi(int sender_pid, int rec_pids[], void *msg, int rec_length);
int sem_init(int semaphore_pointer,int value);
int sem_P(int semaphore_pointer,void* chan);
int sem_V(int semaphore_pointer);

// ulib.c
int stat(char*, struct stat*);
char* strcpy(char*, char*);
void *memmove(void*, void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, char*, ...);
char* gets(char*, int max);
uint strlen(char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
