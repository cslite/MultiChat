#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include<netinet/in.h>

#define SA      struct sockaddr
#define true 1
#define false 0
#define MAX_LEN 2047

const int port = 6000;
const char baseIP[] = "238.101";
int allow_loopback = 0;
int cpid;   //variable to store child process id

struct course{
    int comcode;
    char code[12];
    char *cname;
};

typedef struct course course;

course *courseList;
int numCourses;

void ps(char *str){
    if(str)
        fprintf(stderr,"%s\n",str);
    else
        fprintf(stderr,"null\n");
}

char *allocString(int size){
    char *cstr = (char *)(calloc(size+1,sizeof(char)));
    return cstr;
}


/*
 * Tests whether strings s1 and s2 are equal
 */
int equals(char *s1, char *s2){
    if(s1 == NULL && s2 == NULL)
        return true;
    else if(s1 == NULL || s2 == NULL)
        return false;
    else
        return (strcmp(s1,s2) == 0);
}

void sighandlr(int signo){
    kill(cpid,SIGKILL);
    printf("Exiting...\n");
    exit(0);
}

int parseCourseInfo(char *buf,course *cptr){
    const char delim = ',';
    int i=0;
    while(buf[i] != '\0' && buf[i] != delim)
        i++;
    if(!buf[i])
        return false;
    buf[i] = 0;
    sscanf(buf,"%d",&(cptr->comcode));
    buf = buf+i+1;
    i = 0;
    while(buf[i] != '\0' && buf[i] != delim)
        i++;
    if(!buf[i])
        return false;
    buf[i] = 0;
    strcpy((cptr->code),buf);
    buf = buf+i+1;
    i = 0;
    while(buf[i] != '\n' && buf[i] != '\0')
        i++;
    buf[i] = '\0';
    (cptr->cname) = allocString(i);
    strcpy((cptr->cname),buf);
    return true;
}

/*
 * Format will be
 * 2
 * 1094,CS F211,DATA STRUCTURES & ALGORITHMS
 * 1092,CS F213,OBJECT ORIENTED PROGRAMMING
 */

int readCourseData(char *cpath){
    if(cpath == NULL)
        return false;
    int i;
    FILE *fp = fopen(cpath,"r");
    if(fp == NULL)
        return false;
    char buf[201];
    if(fgets(buf,10,fp) == NULL)
        return false;
    sscanf(buf,"%d",&numCourses);
    courseList = (course *)(calloc(numCourses,sizeof(course)));
    for(i=0;i<numCourses;i++){
        if(fgets(buf,200,fp) == NULL)
            return false;
        if(!parseCourseInfo(buf,courseList+i))
            return false;
    }
    return true;
}

/*
 * It gets a courseCode like "CS F211" and it returns index in courseList
 */

int getCourseIdx(char *courseCode){
    int i;
    //linear search (can be done by binary search if number of courses are binary)
    for(i=0;i<numCourses;i++){
        course *c = &(courseList[i]);
        if(equals(courseCode,c->code)){
            return i;
        }
    }
    return -1;
}

char *genIpFromComCode(int comCode){
    char *multicastIP = allocString(16);
    sprintf(multicastIP,"%s.1%d.1%d",baseIP,comCode/100,comCode%100);
    return multicastIP;
}


int joinGroup(int grpIdx, int sockfd){
    course *c = &(courseList[grpIdx]);
    char *gip = genIpFromComCode(c->comcode);
    struct ip_mreq  mreq;
    struct sockaddr_in gstr;
    if((gstr.sin_addr.s_addr = inet_addr(gip)) == -1){
        free(gip);
        return false;
    }
    mreq.imr_multiaddr = gstr.sin_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if(setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&mreq,sizeof(mreq)) == -1){
        ps("[ERROR]: Unable to join group.");
        free(gip);
        return false;
    }
    free(gip);
    return true;
}

int leaveGroup(int grpIdx, int sockfd){
    course *c = &(courseList[grpIdx]);
    char *gip = genIpFromComCode(c->comcode);
    struct ip_mreq  mreq;
    struct sockaddr_in gstr;
    if((gstr.sin_addr.s_addr = inet_addr(gip)) == -1) {
        free(gip);
        return false;
    }
    mreq.imr_multiaddr = gstr.sin_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(sockfd,IPPROTO_IP,IP_DROP_MEMBERSHIP,(char *)&mreq,sizeof(mreq)) == -1){
        perror("setsockopt(): IP_DROP_MEMBERSHIP:");
        ps("[ERROR]: Unable to leave group.");
        free(gip);
        return false;
    }
    free(gip);
    return true;
}

/*
 * Create a socket, set various properties and return fd
 */
int createSocket(){

    struct sockaddr_in addr;
    int sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock < 0)
        return -1;
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int val = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        perror("Reusing ADDR failed");
        exit(1);
    }

    if(bind(sock,(struct sockaddr*)&addr,sizeof(addr)) < 0){
        ps("[ERROR]: Unable to bind.");
        return -1;
    }

    if(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &allow_loopback,sizeof(allow_loopback)) == -1){
        perror("[ERROR]: Unable to disable loopback");
    }

    int mc_all = 0;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_ALL, (void*) &mc_all, sizeof(mc_all));

    return sock;

}

void generateMessage(char *buf, char *msg, char *courseCode){
    strcpy(buf,"\n\n- - - - - - - - - - - - - - - - - - - -\n");
    char headr[100];
    sprintf(headr,"MESSAGE FROM %s",courseCode);
    strcat(buf,headr);
    strcat(buf,"\n- - - - - - - - - - - - - - - - - - - -\n");
    int mlen = strlen(msg);
    if(mlen > 0 && msg[mlen-1] == '\n')
        msg[mlen-1] = '\0';
    strcat(buf,msg);
    strcat(buf,"\n- - - - - - - - - - - - - - - - - - - -\n");
}

int sendMessage(int grpIdx, int sockfd){
    struct sockaddr_in addr;
    char buf[MAX_LEN], msg[MAX_LEN];
    course *c = &(courseList[grpIdx]);
    char *destIp = genIpFromComCode(c->comcode);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(destIp);

    printf("Type the message to be sent to %s\n",c->code);
    if(fgets(msg,MAX_LEN - 200,stdin) == NULL){
        printf("[ERROR]: Unable to read stdin\n");
        free(destIp);
        return false;
    }
    generateMessage(buf,msg,c->code);
    int len = strlen(buf);
    if(sendto(sockfd,buf,len,0,(SA *) &addr,sizeof(addr)) < 0){
        ps("[ERROR]: Unable to send.");
        free(destIp);
        return false;
    }
    free(destIp);
    return true;
}

void getMessage(int sockfd){
    int nr = 0;
    char recvBuf[MAX_LEN] = {0};

    for(;;){
        nr = recv(sockfd,recvBuf,MAX_LEN,0);
        if(nr < 0){
            ps("[ERROR]: Difficulty in reading from the multicast socket.");
        }
        write(1,recvBuf,nr);
    }


}

void displayMenu(){
    printf("1. Send Message\n");
    printf("2. Join new course\n");
    printf("3. Leave a course\n");
    printf("4. Exit\n: ");
}

void displayCourseMenu(int rcl[],int n){
    int i;
    for(i=0;i<n;i++){
        course *c = &(courseList[rcl[i]]);
        printf("%d. %s - %s\n",i,c->code,c->cname);
    }
    printf(": ");
}

void startApp(){
    int sockfd =createSocket();
    int rcl[10];    //registered course list
    int rcli = 0;
    memset(rcl,-1,sizeof rcl);
    if(sockfd == -1)
        exit(-1);
    if((cpid=fork()) == 0){
        getMessage(sockfd);
    }
    else{
        signal(SIGINT,sighandlr);
        for(;;){
            displayMenu();
            int ch;
            scanf("%d",&ch);
            if(ch == 1){
                //send message
                if(rcli == 0){
                    printf("[ERROR]: You don't have any courses.\n");
                }
                else{
                    displayCourseMenu(rcl,rcli);
                    scanf("%d",&ch);
                    getchar();
                    if(ch >= rcli){
                        printf("[ERROR]: Invalid Choice\n");
                    }
                    else{
                        if(!sendMessage(rcl[ch],sockfd)){
                            printf("[ERROR]: Failed sending message.\n");
                        }
                        printf("[INFO]: Message Sent.\n");
                    }
                }
            }
            else if(ch == 2){
                printf("Enter course code (eg. IS F462): ");
                char buf[15] = {0};
                getchar();
                fgets(buf,15,stdin);

                int len = strlen(buf);
                if((len -1) >= 0 && buf[len-1] == '\n')
                    buf[--len] = '\0';
                int idx = getCourseIdx(buf);
                if(idx == -1){
                    printf("[ERROR]: No such course exists.\n");
                }
                else{
                    rcl[rcli++] = idx;
                    course *c = &(courseList[idx]);
                    if(!joinGroup(idx,sockfd)){
                        printf("[ERROR]: Unable to join multicast group.\n");
                        rcli--;
                    }
                    else{
                        printf("Successfully added %s - %s\n",c->code,c->cname);
                    }
                }
            }
            else if(ch == 3){
                if(rcli == 0){
                    printf("[ERROR]: You have no courses.\n");
                }
                else{
                    displayCourseMenu(rcl,rcli);
                    scanf("%d",&ch);
                    if(ch >= rcli){
                        printf("[ERROR]: Invalid Choice\n");
                    }
                    else{
                        if(leaveGroup(rcl[ch],sockfd)){
                            rcli--;
                            course *c = &(courseList[rcl[ch]]);
                            rcl[ch] = rcl[rcli];
                            printf("Successfully left the course %s - %s\n",c->code,c->cname);
                        }
                    }
                }
            }
            else{
                kill(cpid,SIGKILL);
                exit(0);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2){
        printf("[ERROR]: This program accepts one argument. Path to the course data file.\n");
        return -1;
    }
    if(argc == 3){
        //optional argument to allow/disallow loopback of messages
        sscanf(argv[2],"%d",&allow_loopback);
    }
    if(!readCourseData(argv[1]))
        return -1;

    startApp();
    return 0;
}
