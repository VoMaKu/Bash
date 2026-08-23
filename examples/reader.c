#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char *get_word(char *end){	//читает слово, в *end кладёт разделитель, на котором остановились
	int c = getchar();
	while (c == ' ' || c == '\t'){	//пропускаем отступы перед словом
		c = getchar();
	}
	char *array = NULL;
	int array_size = 0;
	while (c != EOF && c != '\n' && c != ' ' && c != '\t'){
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
	*end = (c == EOF) ? '\0' : c;	//'\0' означает конец ввода, дальше читать нечего
	if (array == NULL){
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

void clear(char **list){
	for (int i = 0; list[i] != NULL; i++){
		free(list[i]);
	}
	free(list);
}

char **get_list(char *end){	//собирает слова строки в список, заканчивает его NULL
	int arg_c = 0;
	char **list = NULL;
	*end = '\n';
	while (1){
		char *word = get_word(end);
		if (word != NULL){
			char **temp_list = realloc(list, (arg_c + 1) * sizeof(char *));
			if (temp_list == NULL){
				perror("realloc error");
				if (list != NULL){
					list[arg_c] = NULL;
					clear(list);
				}
				free(word);
				return NULL;
			}
			list = temp_list;
			list[arg_c] = word;
			arg_c++;
		}
		if (*end == '\n' || *end == '\0'){
			break;
		}
	}
	if (list == NULL){	//пустая строка
		return NULL;
	}
	char **temp_list = realloc(list, (arg_c + 1) * sizeof(char *));
	if (temp_list == NULL){
		perror("realloc error");
		list[arg_c] = NULL;
		clear(list);
		return NULL;
	}
	list = temp_list;
	list[arg_c] = NULL;	//заканчиваем список NULL, иначе execvp читает за его границей
	return list;
}

int main(){
	char end = '\n';
	while (end != '\0'){	//выходим по концу ввода
		char **list = get_list(&end);
		if (list == NULL){
			continue;
		}
		pid_t child = fork();
		if (child == -1){
			perror("fork failed");
			clear(list);
			continue;
		} else if (child == 0){
			execvp(list[0], list);
			perror("exec failed");
			clear(list);
			_exit(1);
		}
		wait(NULL);	//без этого потомки накапливаются зомби
		clear(list);
	}
	return 0;
}
