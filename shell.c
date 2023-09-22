#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

#define MAX_LEN 1000

typedef struct Command{ //lista encadeada de comandos que será usada na execução das threads
    char comando[MAX_LEN];
    struct Command *next;
}Command;

void prinft_commmands(FILE *file);
void verify_user_input(Command **head, Command **tail, char *user_in);
void insert_cmd(Command **head, Command **tail, char *cmd);
void parallel_exec(Command **head, Command **tail);
void *read_cmd(void *command);
void sequential_exec(Command **head, Command **tail);
void pipe_commands_par(char *cmd1, char *cmd2);
void pipe_commands_seq(char *cmd1, char *cmd2);
void exec_command(char *cmd);
void *pipe_out_thread(void *cmd);
void *pipe_in_thread(void *cmd);
void output_append_seq(char *command, char *filename);
void output_write_seq(char *command, char *filename);
void input_write_seq(char *program, char *input_f) ;
void *output_append_par(void *cmd, void *input_f);
void *output_write_par(void *cmd, void *input_f);
void *input_write_par(void *program, void *input_f) ;
void remove_spaces(char *str);
void print_prompt();

char last_cmd[MAX_LEN]; //variavel pra armazenar o ultimo comando armazenado
int style = 0; //0 = seq, 1 = par
int type = 0; //0 = int //1 = batch_file
int line_count = 0;
int exit_flag = 0;
int pipe_file[2];

int main(int argc, char *argv[]){
    char *user_input = NULL;
    size_t length = 0;
    ssize_t read;
    FILE *file;

    Command *head = NULL;
    Command *tail = NULL;
    if(argc == 2){ //se for um arquivo ele vai abrir, ler, printar, fechar, e abrir denovo
        type = 1;
        char *file_name = argv[1];
        file = fopen(file_name, "r");
        //prinft_commmands(file); 
        //printf("%d", line_count);
        //fseek(file, 0, SEEK_SET);
        //file = fopen(file_name, "r");
    }
    while(1){

        if(type == 0){
            print_prompt();
        }

        if(type == 1 && file == NULL){
            perror("Arquivo nao encontrado");
            return 1;
        }
        
        if(type == 1){//lendo a linha de um arquivo
            read = getline(&user_input, &length, file);
            
        }
        else{//lendo a linha no modo interativo
            read = getline(&user_input, &length, stdin);
        }
        
        if((read == (ssize_t)-1)){ //saindo caso o "ctrl+d"  ou EOF
            break;
        }
        
        if((read > 0 && user_input[read - 1] == '\n')){//tirando a quebra de linha
            user_input[read - 1] = '\0';
        }        

        if (strcmp(user_input,"style parallel") == 0 || strstr(user_input, "style parallel") != NULL){ //definindo se o estilo será paralelo ou sequencial na hora da execução
            style = 1;
            if(type == 1){             
                printf("%s\n", user_input);
                sleep(1);
            }

        }
        else if(strcmp(user_input,"style sequential") == 0 || strstr(user_input, "style sequential") != NULL){
            style = 0;
            if(type == 1){             
                printf("%s\n", user_input);
                sleep(1);
            }
        }
        else{
            if(type == 1){             
                printf("%s\n", user_input);
                sleep(1);
            }

            verify_user_input(&head, &tail, user_input);
            if(style == 0){
                sequential_exec(&head, &tail); //execucao sequencial
                
            }
            else if(style == 1){
                parallel_exec(&head, &tail); //executando paralelamente
                if (exit_flag == 1){
                    sleep(1);
                    return 0;
                }
            }
        }
        if(type == 1){
            free(user_input);
        }
    }
    if (type == 1){
       fclose(file);
    }
    return 0;
}


void verify_user_input(Command **head, Command **tail, char *user_in){
    char *cmd;
    cmd = strtok(user_in, ";");//separando os comandos pelo caractere ";"
    while(cmd != NULL){
        remove_spaces(cmd);
        insert_cmd(head, tail, cmd);
        cmd = strtok(NULL, ";");
    }

}


void insert_cmd(Command **head, Command **tail, char *cmd){//inserir comando na fila
    Command *new = (Command *)malloc(sizeof(Command));

    if(new != NULL){
        if(cmd[0] != 0 && strcmp(cmd, "!!") != 0){
            strcpy(last_cmd, cmd);//determinando o ultimo comando
        }
        if(strcmp(cmd, "!!") == 0){
            strcpy(new->comando, last_cmd);
        }
        else{
            strcpy(new->comando, cmd);
        }
        new->next = NULL;
        if(*head == NULL){
            *head = new;
            *tail = new;
        }
        else{
            (*tail)->next = new;
            *tail = new;
        }
        
    } 
}

void exec_command(char *cmd){
    char *args[256]; // Array de argumentos (incluindo NULL no final)
    int arg_count = 0;

    if(strcmp(cmd, "exit") == 0){
        exit(0);
    }
    
    // Divide a entrada em tokens usando espaço como delimitador
    char *token = strtok(cmd, " ");
    while (token != NULL) {
        args[arg_count] = token;
        arg_count++;
        token = strtok(NULL, " ");
    }

    args[arg_count] = NULL; // Coloca NULL no final do array de argumentos

    // Use execvp para executar o comando
    execvp(args[0], args);  
    perror("Comando invalido - erro");
 
    
}

void sequential_exec(Command **head, Command **tail){
    Command *temp, *aux = *head;
    pid_t child_pid;
    int child_status;

    while(aux != NULL){
        child_pid = fork(); //criando processo filho
     
        if(child_pid == 0){ //processo filho  
            if(type == 1){
                printf("%s\n", aux->comando);
                sleep(1);
            }
            if(strchr(aux->comando, '|') != NULL) { //executar os comandos em pipe
                char *cmd1 = strtok(aux->comando, "|");
                char *cmd2 = strtok(NULL, "|");

                remove_spaces(cmd1);
                remove_spaces(cmd2);
                                       
                pipe_commands_seq(cmd1, cmd2);
            }
            else if(strstr(aux->comando, ">>") != NULL){//Output com Apend em um arquivo (>>) 
          
                char *cmd = strtok(aux->comando, ">>");
                char *file_name = strtok(NULL, ">>");

                remove_spaces(cmd);
                remove_spaces(file_name);
                
                output_append_seq(cmd, file_name);
                exit(0);
            }
            else if(strchr(aux->comando, '>') != NULL){ //output em um arquivo (>)
                char *cmd = strtok(aux->comando, ">");
                char *file_name = strtok(NULL, ">");

                remove_spaces(cmd);
                remove_spaces(file_name);

                output_write_par(cmd, file_name);
                exit(0);
            }
            else if (strchr(aux->comando, '<') != NULL){  //input vindo de um arquivo (<)
                char *prog = strtok(aux->comando, "<");
                char *file_name = strtok(NULL, "<");

                remove_spaces(prog);
                remove_spaces(file_name);

                input_write_seq(prog, file_name);
                exit(0);
            }
            else{
                remove_spaces(aux->comando);
                exec_command(aux->comando);
                exit(0);
            }
        } 
        else if(child_pid < 0){ 
            perror("Criação do processo filho - erro");
        }
        else{
            if(strstr(aux->comando, "exit") != NULL){
                sleep(1);
                exit(0);
            }
            else{
                waitpid(child_pid, &child_status, 0);
            }
            aux = aux->next; 
        }
        
    }

    while(*head != NULL){ //limpando os comandos depois de executar todas as threads
        temp = *head;
        *head = (*head)->next;
        free(temp);
    }
    *tail = NULL;
    
}


void *read_cmd(void *command){ //tratando o ponteiro void e executando o comando no sistema 
    char *read = (char *)command;
    if(strcmp(read, "exit") == 0){
        exit_flag = 1;
        pthread_exit(NULL);
    }
    else if(strchr(read, '|') != NULL) { 
    char *cmd1 = strtok(read, "|");
    char *cmd2 = strtok(NULL, "|");

    remove_spaces(cmd1);
    remove_spaces(cmd2);

    pipe_commands_par(cmd1, cmd2);
    }
    else if(strstr(read, ">>") != NULL){
        char *cmd = strtok(read, ">>");
        char *file_name = strtok(NULL, ">>");

        remove_spaces(cmd);
        remove_spaces(file_name);

        output_append_par(cmd, file_name);
    }
    else if(strstr(read, ">") != NULL){
        char *cmd = strtok(read, ">");
        char *file_name = strtok(NULL, ">");

        remove_spaces(cmd);
        remove_spaces(file_name);

        output_write_par(cmd, file_name);
    }
    else if(strstr(read, "<") != NULL){
        char *cmd = strtok(read, "<");
        char *file_name = strtok(NULL, "<");

        remove_spaces(cmd);
        remove_spaces(file_name);

        input_write_par(cmd, file_name);
    }
    else{
        if(system(read) != 0){ //executando o comando
            perror("Comando invalido - erro");
        }
        pthread_exit(NULL);
    }
    return NULL;
}

void parallel_exec(Command **head, Command **tail){//executar em paralelo os comandos, em forma de threads
    
    pid_t child_pid;
    int child_status;
    Command *aux = *head;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t));
    int count = 0;

    child_pid = fork();
    if(child_pid == 0){
        while(aux != NULL){
            if (threads == NULL){
                perror("Alocacao de memoria na criacao de thread - erro");
                break;
            }
            else{
                if(pthread_create(&threads[count], NULL, read_cmd, (void*)aux->comando) != 0){
                    perror("Criacao da thread - erro");
                    break;
                }
                count++; 
                
                threads = (pthread_t *)realloc(threads, (count + 1) * sizeof(pthread_t));//realocando o espaço de memoria das threads 

                if(threads == NULL){
                    perror("Realoc de memoria para as threads - erro");    
                }
            }
            if (exit_flag == 1){
                break;
            }
            aux = aux->next;
        }
        
        for(int i = 0; i < count; i++){
            if(pthread_join(threads[i], NULL) != 0){ 
                perror("Espera por threads - erro");  
            }
            
        }

        exit(0);
    }
    else if(child_pid < 0){ //o processo pai deve esperar o processo filho finalizar
        perror("Criação do processo filho - erro");
    }
    else{     
        waitpid(child_pid, &child_status, 0);
    }

    Command* temp;
    while(*head != NULL){ //limpando os comandos depois de executar todas as threads
        temp = *head;
        if(strcmp(temp->comando, "exit") == 0){
            exit_flag = 1;
        }
        
        *head = (*head)->next;
        free(temp);
    }
    *tail = NULL;

    free(threads); //limpando o array de threads    
    
}

void pipe_commands_seq(char *cmd1, char *cmd2){
    int pipefd[2]; 
    pid_t pipe_pid;

    if(pipe(pipefd) != 0){ //criando um pipe
        perror("Criacao pipe - erro");
        //exit(EXIT_FAILURE);
    }

    pipe_pid = fork();
    
    if(pipe_pid < 0){
        perror("Criacao processo pipe - erro");
        //exit(EXIT_FAILURE);
    }
    else if(pipe_pid == 0){  //processo neto
        close(pipefd[1]); //fecha a extremidade de escrita do pipe

        //redireciona a entrada do cmd1 pra a leitura do pipe 
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]); 

        //char *exec_cmd[] = {"/bin/sh", "-c",cmd2, NULL};
        exec_command(cmd2);
        perror("Pipe cmd 2 -erro");
        exit(1);
    } 
    else{ //processo filho
        close(pipefd[0]); 
       
        dup2(pipefd[1], STDOUT_FILENO); //lê a entrada do cmd1 pra executa no cmd2
        close(pipefd[1]);  // Fecha a extremidade de escrita do pipe
 
        exec_command(cmd1);
        perror("Pipe cmd 1 -erro");
        exit(1);
    }
}

void *pipe_out_thread(void *cmd) {
    char *cmd1 = (char *)cmd;
    close(pipe_file[0]); // Fecha a extremidade de leitura do pipe

    // Redireciona a saída padrão para a extremidade de escrita do pipe
    dup2(pipe_file[1], STDOUT_FILENO);
    close(pipe_file[1]); // Fecha a extremidade de escrita do pipe

    system(cmd1);
    sleep(1);
    pthread_exit(NULL);
}

void *pipe_in_thread(void *cmd) {
    char *cmd2 = (char *)cmd;
    close(pipe_file[1]); // Fecha a extremidade de escrita do pipe

    // Redireciona a entrada padrão para a extremidade de leitura do pipe
    dup2(pipe_file[0], STDIN_FILENO);
    close(pipe_file[0]); // Fecha a extremidade de leitura do pipe

    system(cmd2);
    sleep(1);
    pthread_exit(NULL);
}

void pipe_commands_par(char *cmd1, char *cmd2){

    if (pipe(pipe_file) == -1) {
        perror("Criacao do pipe - erro");
        exit(1);
    }
    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, pipe_in_thread, (void*) cmd2);
    pthread_create(&thread2, NULL, pipe_out_thread, (void*) cmd1);

    sleep(1);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
}

//Output com Apend em um arquivo (>>)
void output_append_seq(char *cmd, char *filename) { 
    FILE *file = fopen(filename, "a"); // Abre o arquivo em modo de anexação (append).

    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        
    }

    // Redireciona a saída padrão para o arquivo
    if (dup2(fileno(file), STDOUT_FILENO) == -1) {
        perror("dup2");
        fclose(file);
        
    }

    // Executa o comando e redireciona sua saída para o arquivo
    if (system(cmd) == -1) {
        perror("system");
        fclose(file);  
    }

    // Fecha o arquivo após a execução
    fclose(file);
}

void output_write_seq(char *cmd, char *filename) { 
    FILE *file = fopen(filename, "w+");
    if (file == NULL) {
        perror("open");
    }

    // Redirecionar a saída padrão para o arquivo
    if (dup2(fileno(file), STDOUT_FILENO) == -1) {
        perror("dup2");
    }

    // Executar o comando especificado e redirecionar sua saída para o arquivo
    if (system(cmd) == -1) {
        perror("system");
    }

    // Fechar o arquivo após o redirecionamento
    fclose(file);
}

void input_write_seq(char *program, char *input_f) { 
    int pid;
    int status;

    pid = fork(); // Cria um processo filho

    if (pid == -1) {
        perror("fork");
    }

    if (pid == 0) { // Processo filho
        FILE *file = fopen(input_f, "r");
        if (file == NULL) {
            perror("open");
            exit(1);
        }

        // Redireciona a entrada padrão (stdin) para o arquivo
        if (dup2(fileno(file), STDIN_FILENO) == -1) {
            perror("dup2");
            fclose(file);
            exit(1);
        }

        fclose(file);

        // Executa o programa especificado
        exec_command(program);
        perror("execlp"); // Se o execlp() falhar
        exit(1);
    } else { // Processo pai
        // Aguarda o processo filho terminar
        wait(&status);
    }
}

void *output_append_par(void *cmd, void *input_f) { 

    char *cmd1 = (char *)cmd;
    char *filename = (char *)input_f;

    FILE *file = fopen(filename, "a"); // Abre o arquivo em modo de anexação (append).

    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        pthread_exit((void *)NULL);
    }

    // Redireciona a saída padrão para o arquivo
    if (dup2(fileno(file), STDOUT_FILENO) == -1) {
        perror("dup2");
        fclose(file);
        
    }

    // Executa o comando e redireciona sua saída para o arquivo
    if (system(cmd1) == -1) {
        perror("system");
        fclose(file);  
    }

    // Fecha o arquivo após a execução
    fclose(file);
    pthread_exit((void *)NULL);
}

void *output_write_par(void *cmd, void *input_f) { 
    
    char *cmd1 = (char *)cmd;
    char *filename = (char *)input_f;

    FILE *file = fopen(filename, "w+");
    if (file == NULL) {
        perror("open");
    }

    // Redirecionar a saída padrão para o arquivo
    if (dup2(fileno(file), STDOUT_FILENO) == -1) {
        perror("dup2");
    }

    // Executar o comando especificado e redirecionar sua saída para o arquivo
    if (system(cmd1) == -1) {
        perror("system");
    }

    // Fechar o arquivo após o redirecionamento
    fclose(file);

    pthread_exit((void *)NULL);
}

void *input_write_par(void *program, void *input_f) { 
    char *prog = (char *)program;
    char *file_name = (char *)input_f;
    int pid;
    int status;

    pid = fork(); // Cria um processo filho

    if (pid == -1) {
        perror("fork");
    }

    if (pid == 0) { // Processo filho
        FILE *file = fopen(file_name, "r");
        if (file == NULL) {
            perror("open");
            pthread_exit((void *)NULL);
        }

        // Redireciona a entrada padrão (stdin) para o arquivo
        if (dup2(fileno(file), STDIN_FILENO) == -1) {
            perror("dup2");
            fclose(file);
            pthread_exit((void *)NULL);
        }

        fclose(file);

        // Executa o programa especificado
        exec_command(prog);
        perror("execlp"); // Se o execlp() falhar
        pthread_exit((void *)NULL);
    } else { // Processo pai
        // Aguarda o processo filho terminar
        wait(&status);
        pthread_exit((void *)NULL);
    }
}

void print_prompt(){
    if(style == 1 && type == 0){
        printf("cepc par> ");
    }
    else if(style == 0 && type == 0){
        printf("cepc seq> ");
        style = 0;
    }
}

void remove_spaces(char *str) {
    int len = strlen(str);
    int start = 0;
    int end = len - 1;

    // Encontre a posição do primeiro caractere não espaço
    while (str[start] == ' ' || str[start] == '\t') {
        start++;
    }

    // Encontre a posição do último caractere não espaço
    while (end > start && (str[end] == ' ' || str[end] == '\t')) {
        end--;
    }

    // Mova os caracteres não espaço para o início da string
    int i, j = 0;
    for (i = start; i <= end; i++) {
        str[j++] = str[i];
    }

    str[j] = '\0'; // Termina a string resultante
}




