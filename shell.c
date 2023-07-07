#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

/*
 * 기호를 매크로로 정의. 아래는 각 기호에 따른 매직넘버.
 * \n 1
 * cmd 2
 * & 3
 * ; 4
 * < 5
 * > 6
 * >> 7
 * >| 8
 * | 9
 * >| 10
*/
#define EOL 1
#define ARG 2
#define AMPERSAND 3
#define SEMICOLON 4
#define REDIRECTION_L 5
#define REDIRECTION_R 6
#define REDIRECTION_RR 7
#define REDIRECTION_RN 8
#define PIPE 9
#define REDIRECTION_RE 10

#define ERROR -1
#define TRUE 1
#define FALSE 0

#define FOREGROUND 0 // 전면처리
#define BACKGROUND 1 // 후면 처리

#define PERM 0644 // 파일 열고 생성할 때의 권한

#define MAXLINE 1024 // 명령어 최대 길이
#define MAXPIPE 10 // 파이프의 최대 크기 

static char cur[MAXLINE];
char dir[MAXLINE];

char *init_args[MAXLINE]; 

char input[MAXLINE + 1];
char *arg[MAXLINE + 1];
char *argp[MAXPIPE][MAXLINE + 1];

/*
 * < 일때 왼쪽 명렁어를 leftname에 저장
 * > 일때 오른쪽 명령어를 rightnam에 저장.
 */
static char leftname[MAXLINE];
static char rightname[MAXLINE];

// index의 해당하는 파이프의 명령어의 크기
static int argpn[MAXPIPE];
//  < 일때,       > 일떄
int checkleft, checkright;
//  파이프 갯수,  명령어 배열 갯수
int pnum,      count;

// 에러처리
void fatal(char *msg);
/*
 * 명령어 입력받고 분석하고 실행
 */
void input_cmd();
int analyze_cmd();
int check_sign(int *idx);
int runcommand(char **cmd, int type);
// history 관련 함수
void create_history();
void insert_history(char **cmd);
/*
 * 명령어 함수
 */
int cmd_cd(char **path);
int cmd_history();
void run_history(char *history);

/*
 * 에러 메시지 출력
 */
void fatal(char *msg) {
    printf("%s", msg);
    exit(-1);
}

int main(){

    // history 파일 생성
    create_history();

    // 명령 프롬포트를 무한루프로 받기
    while(TRUE) {
        printf("youngwoong:");
        
        // 명령어 입력받기
        input_cmd();

        //history라는 파일에 명령어 넣기
        insert_history(arg);
        
        if(**arg == '!'){ // !일때 다르게 처리
            run_history(*arg);
        }
        
        // 구문 분석 및 실행
        analyze_cmd();
    }
    unlink(".history"); // history 파일 제거
    return 0;
}

/*
 * history 파일 생성
 */
void create_history(){
    int idx = 0;
    FILE *fp;

    getcwd(dir, MAXLINE);
    while (dir[idx] != '\0')
        idx++;// sysproj/proj/.history
    dir[idx] = '/';
    strcpy(dir + idx + 1, ".history");
    
    if ((fp = fopen(dir, "w")) == NULL){
        fatal("history open error\n");
    }
    fclose(fp);
}

/*
 * 명령어 파싱
 * stdin에서 받아와서 arg에 넣어주기
 */
void input_cmd(){

    int i = 0;
    
    // 배열 값 초기화
    memset(input, 0, MAXLINE);
    memset(arg, 0, MAXLINE);
    memset(init_args, 0, MAXLINE);

    fflush(stdout);
    
    /* prompt의 요구사항 프롬프트는 자신의 이름:현재디렉토리$ 형태로 출력 */
    getcwd(cur, MAXLINE);
    printf("%s$ ", cur);

    fgets(input, sizeof(input), stdin);
    count = strlen(input)-1;
    input[count] = '\0';
    
    /* delimeter를 " "로 설정 후 문자열 파싱 */
    char *ptr = strtok(input, " "); 
    
    /* delimeter별로 args에 넣어주기 */
    while(ptr != NULL) { 
        arg[i++] = ptr;
        ptr = strtok(NULL, " ");
    }
    arg[i] = "\0"; /*argc의 execvp의 파라미터로 전달할 수 없기 때문에 NULL로 파라미터의 끝을 표시함 */
    count = i;

    // 구문 분석할 배열로 복사
    for(int i = 0; i < count; i++) {
        init_args[i] = arg[i];
    }
}

/*
 * !1 실행
 */
void run_history(char *history){

    int num = -1, fidx = 0;
    int n, fd, i, j = 0, temp_idx = 0;
    pid_t pid, status;

    char buf[MAXLINE];
    char temp[1][MAXLINE];
    char *cmd[MAXLINE];
    
    memset(buf, 0, MAXLINE);
    memset(cmd, 0, MAXLINE);
    memset(init_args, 0, MAXLINE);

    num = atoi(history + 1);

    if((fd = open(dir, O_RDONLY, PERM)) < 0) {
        fatal("history open error");
    }
    if((n = read(fd, buf, MAXLINE)) < 0){
        fatal("history file read error");
    }

    if(num <= 0){
        fatal("integer value");
    }else{
        for(i = 0; i < n; i++){
            if(fidx == num-1){
                if(buf[i] == '\n') {
                    break;
                }
                temp[0][temp_idx++] = buf[i];
            }
            
            if(buf[i] == '\n'){
                fidx++;
            }

            if(fidx == num) break;
        }
    }
    temp_idx--;
    temp[0][temp_idx] = '\0';

    char *ptr = strtok(temp[0], " "); 
    
    /* delimeter별로 args에 넣어주기 */
    while(ptr != NULL) { 
        cmd[j++] = ptr;
        ptr = strtok(NULL, " ");
    }
    cmd[j] = NULL; /*argc의 execvp의 파라미터로 전달할 수 없기 때문에 NULL로 파라미터의 끝을 표시함 */
    count = j;

    //history에 넣어주기
    insert_history(cmd);

    // 구문 분석할 배열로 복사
    for(i = 0; *(cmd + i) != NULL; i++) {
        init_args[i] = cmd[i];
    }

}

/*
 * history 파일에 명령어 넣기.
 */
void insert_history(char **cmd){

    int check = 0, idx = 0;
    FILE *fp;
    
    if ((fp = fopen(dir, "aw")) == NULL){
        fatal("history open error\n");
    }else{
        if(**cmd == '!' || **cmd == '\0'){
            fclose(fp);
            return;
        }
        for(int i = 0; *(cmd + i) != NULL; i++) {
            fprintf(fp, "%s ", *(cmd + i));
        }
        fprintf(fp, "%s", "\n");
        fclose(fp);   
    }
}

/*
 * change directory 구현.
 * 성공시 TRUE(1) FALSE(0) 리턴
 */
int cmd_cd(char **path){

    if(chdir(*(path + 1)) == -1) {
        return FALSE;
    }
    return TRUE;
}

/*
 * history 구현
 * 성공시 TRUE(1) FALSE(0) 리턴
 */
int cmd_history(){

    int fd, n;
    char buf[MAXLINE];

    if((fd = open(dir, O_RDONLY, PERM)) < 0) {
        return FALSE;
    }

    while(1){
        if((n = read(fd, buf, MAXLINE)) < 0){
            return FALSE;
        }
        if (n == 0) {
            printf("src End Of File\n");
            break;
        }else{
            printf("%s\n", buf);
        }
    }

    return TRUE;
}



/*
 * 구문 분석하는 함수
*/
int analyze_cmd(){
    // input값인 arg에 따라 처리.
    char *argu[MAXLINE + 1];
    int nargu = 0;
    
    // redirection에 따라 실행시킬 배열
    char *arg2[MAXLINE + 1];
    int narg2;
    
    int toktype; // 기호
    int btoktype; // 이전 기호
    
    int type; // 전면처리, 후면처리
    int redir = 0; // 리다이렉션 기호 존재 

    int pointer = 0; // 명령어 인덱스 표시

    checkleft = 0; // < 기호 존재
    checkright = 0; // >, >>, >| 파악
    pnum = 0; // 파이프의 갯수

    memset(argu, 0, MAXLINE);
    memset(arg2, 0, MAXLINE);
    
    for (;;){
        // 명령어 구문 분석
        toktype = check_sign(&pointer);

        if(toktype == -1) { // 기호를 인식 못할 때, 정해진 기호가 아닐 때
            fatal("check sign error");
        }

        // 이전 기호가 리다이랙션이였을 시 해당 문자열을 배열에 넣어주기
        switch(btoktype){ 
            case REDIRECTION_L:
                strcpy(leftname, *(arg + pointer));
                checkleft = 1;
            break;

            case REDIRECTION_R:
                strcpy(rightname, *(arg + pointer));
                checkright = 1;
            break;

            case REDIRECTION_RR:
                strcpy(rightname, *(arg + pointer));
                checkright = 2;
            break;

            case REDIRECTION_RN:
                strcpy(rightname, *(arg + pointer));
                checkright = 3;
            break;

            case REDIRECTION_RE:
                strcpy(rightname, *(arg + pointer));
                checkright = 4;
            break;

        }
        // 현재 기호에 대한 구문 분석
        switch (toktype){
            case ARG:
                if (nargu < MAXLINE){ // argu에 명령어 넣어주기
                    argu[nargu++] = init_args[pointer];
                }
            break;

            // redirection 시 여태까지 명령어르 arg2에 넣어주고 arg 초기화
            case REDIRECTION_L:
            case REDIRECTION_R:
            case REDIRECTION_RR:
            case REDIRECTION_RN:
            case REDIRECTION_RE:
                if (redir != 1){
                    for(int i = 0; i < nargu; i++){
                        arg2[i] = argu[i];
                    }
                    redir = 1;
                    narg2 = nargu;
                }
                if (nargu != 0)
                    arg[nargu] = NULL;
                nargu = 0;
            break;
            // pipe일 시 argp에 명령어를 넣어주고 argu 초기화
            case PIPE:                
                for(int i = 0; i < nargu; i++) {
                    argp[pnum][i] = argu[i];
                }
                argpn[pnum] = nargu;
                pnum++;
                if (nargu != 0)
                    argu[nargu] = NULL;
                nargu = 0;
            break;
            
            case EOL:
            case SEMICOLON:
            case AMPERSAND:

                if (toktype == AMPERSAND){
                    type = BACKGROUND;
                }else{
                    type = FOREGROUND;
                }
                if (nargu != 0){
                    argu[nargu] = NULL;
                    if (redir == 1){
                        arg2[narg2] = NULL;
                        runcommand(arg2, type);
                        redir = 0;
                        checkright = 0;
                        checkleft = 0;
                    }else{
                        runcommand(argu, type);
                    }
                }
                
                if (toktype == EOL)
                    return 0;
                nargu = 0;
            break;
        }
        btoktype = toktype;
        pointer++;
    }    
    
}

/*
 * 연산자 확인
 */
int check_sign(int *idx){

    int type = -1;

    char *tok = *(init_args + *idx);

    if(*idx == count || strcmp(tok, "\n") == 0){
        type = EOL;
    }else if(strcmp(tok, "&") == 0){
        type = AMPERSAND;
    }else if(strcmp(tok, ";") == 0){
        type = SEMICOLON;
    }else if(strcmp(tok, "<") == 0){
        type = REDIRECTION_L;
    }else if(strcmp(tok, ">") == 0){
        type = REDIRECTION_R;
    }else if(strcmp(tok, ">>") == 0){
        type = REDIRECTION_RR;
    }else if(strcmp(tok, ">|") == 0){
        type = REDIRECTION_RN;
    }else if(strcmp(tok, "2>") == 0){
        type = REDIRECTION_RE;
    }else if(strcmp(tok, "|") == 0){
        type = PIPE;
    }else{
        type = ARG;
    }

    return type;
}

/*
 * 명령어 실행
 */
int runcommand(char **cmd, int type){
    pid_t pid;
    int fd, status;
    int n, i, j;
    int pipe_fd[MAXPIPE][2];

    if (strcmp(*cmd, "cd") == 0 && type == FOREGROUND){
        if(cmd_cd(cmd)){
            return 0;
        }
        fatal("change directory error");
    }else if (strcmp(*cmd, "history") == 0){
        if(cmd_history()){
            return 0;
        }
        fatal("history error");
    }else if (strcmp(*cmd, "exit") == 0 || strcmp(*cmd, "quit") == 0){
        fatal("Bye");
    }

    // pnum의 갯수만큼 파이프 열기
    for (i = 0; i < pnum; i++){
        if (pipe(pipe_fd[i]) == -1){
            fatal("pipe error");
        }
    }

    for (i = 0; i < pnum + 1; i++){
        switch (pid = fork()){
            case -1:
                fatal("fork error");
            return -1;

            case 0:
                // ==========리다이렉션 파이프 처리===============
                if (checkright){
                    switch(checkright){
                        case 1:
                            fd = open(rightname, O_WRONLY | O_CREAT | O_TRUNC, PERM);
                        break;

                        case 2:
                            fd = open(rightname, O_WRONLY | O_CREAT | O_APPEND, PERM);
                        break;

                        case 3:
                            fd = open(rightname, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, PERM);
                        break;

                        case 4:
                            fd = open(rightname, O_WRONLY | O_CREAT | O_TRUNC, PERM);
                            dup2(fd, STDERR_FILENO);
                        break;
                        
                        default:
                            printf("default error");
                        break;
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }else if (i < pnum){ // 파이프 존재
                    dup2(pipe_fd[i][1], STDOUT_FILENO);
                }
                
                if (checkleft){
                    fd = open(leftname, O_RDONLY | O_CREAT, PERM);
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }else if (i != 0){ // 파이프 시
                    dup2(pipe_fd[i - 1][0], STDIN_FILENO);
                }
                
                // 파일 디스크립터 절약
                for (j = 0; j < pnum; j++){
                    close(pipe_fd[j][0]);
                    close(pipe_fd[j][1]);
                }
                // ==========리다이렉션 파이프 처리===============

                // // ============명령어 실행================
                if (i < pnum){
                    argp[i][argpn[i]] = NULL;
                    execvp(*argp[i], argp[i]);        
                }
                else{
                    execvp(*cmd, cmd);
                }
                fatal(*cmd);

                exit(1);
            
        }
    }
    // 파일 디스크립터 절약
    for (j = 0; j < pnum; j++){
        close(pipe_fd[j][0]);
        close(pipe_fd[j][1]);
    }

    if (type == BACKGROUND){
        printf("[ %d ]\n", pid);
        return 0;
    }
    if (waitpid(pid, &status, 0) == -1)
        return -1;
    else
        return status;
}