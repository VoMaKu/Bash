#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ARGS 16

char *get_word(char *end){	//читает слово, в *end кладёт разделитель, на котором остановились
	int c = getchar();
	while (c == ' ' || c == '\t'){	//пропускаем отступы перед словом
		c = getchar();
	}
	char *array = NULL;
	int array_size = 0;
	while (c != EOF && c != '\n' && c != ' ' && c != '\t' && c != '|'){
		char *temp_array = realloc(array, (array_size + 1) * sizeof(char));
		if (temp_array == NULL){
			perror("realloc error");
			free(array);
			*end = '\0';
			return NULL;
		}
		array = temp_array;
		array[array_size] = c;
		array_size++;
		c = getchar();
	}
	*end = (c == EOF) ? '\n' : c;
	if (array == NULL){	//слова не было: сразу разделитель или конец ввода
		return NULL;
	}
	char *temp_array = realloc(array, (array_size + 1) * sizeof(char));
	if (temp_array == NULL){
		perror("realloc error");
		free(array);
		*end = '\0';
		return NULL;
	}
	array = temp_array;
	array[array_size] = '\0';
	return array;
}

char read_cmd(char **argv){	//собирает слова одной команды, список заканчивает NULL
	int argc = 0;
	char end = '\n';
	while (argc < MAX_ARGS - 1){
		char *word = get_word(&end);
		if (word == NULL){
			break;
		}
		argv[argc] = word;
		argc++;
		if (end == '\n' || end == '|' || end == '\0'){
			break;
		}
	}
	argv[argc] = NULL;
	return end;
}

void clear_cmd(char **argv){
	for (int i = 0; argv[i] != NULL; i++){
		free(argv[i]);
	}
}

int main(){
	char *cmd_A[MAX_ARGS], *cmd_B[MAX_ARGS];
	char end = read_cmd(cmd_A);
	if (cmd_A[0] == NULL || end != '|'){
		fprintf(stderr, "usage: type a line of the form \"command | command\"\n");
		clear_cmd(cmd_A);
		return 1;
	}
	read_cmd(cmd_B);
	if (cmd_B[0] == NULL){
		fprintf(stderr, "usage: type a line of the form \"command | command\"\n");
		clear_cmd(cmd_A);
		return 1;
	}
	int s1[2];
	if (pipe(s1) == -1){
		perror("s1 error");
		clear_cmd(cmd_A);
		clear_cmd(cmd_B);
		return 100;
	}
	pid_t pid1 = fork();
	if (pid1 < 0){
		perror("pid1");
		return 90;
	}
	if (pid1 == 0){
		dup2(s1[1], 1);
		close(s1[0]);
		close(s1[1]);
		execvp(cmd_A[0], cmd_A);
		perror(cmd_A[0]);
		_exit(80);
	}
	pid_t pid2 = fork();
	if (pid2 < 0){
		perror("pid2");
		return 60;
	}
	if (pid2 == 0){
		dup2(s1[0], 0);	//без этого второй потомок не подключён к каналу и читает с клавиатуры
		close(s1[0]);
		close(s1[1]);
		execvp(cmd_B[0], cmd_B);
		perror(cmd_B[0]);
		_exit(50);
	}
	close(s1[0]);
	close(s1[1]);
	int flag = 0;
	for (int i = 0; i < 2; ++i){	//потомков двое
		int status;
		wait(&status);
		if(!(WIFEXITED(status) && WEXITSTATUS(status) == 0)){
			flag++;
		}
	}
	clear_cmd(cmd_A);
	clear_cmd(cmd_B);
	if (flag){
		return 20;
	}
	else{
		return 0;
	}
}
