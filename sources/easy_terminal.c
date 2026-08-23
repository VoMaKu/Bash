#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>

pid_t child; 	// PID ребенка
pid_t *children = NULL;	//PID всех звеньев текущей команды
int chld_sz = 0;	//сколько их сейчас в массиве
int stopflag = 0; 	// для завершения дочернего процесса SIGINT
int background = 0;		//если есть фоновый процесс
int bckgrd_count = 0;	//количество фоновых процессов
int readfd = -1, writefd = -1;		//если открываем файл
int readflag = 0, writeflag = 0;	//проверка на открытие файлов, проверка на ошибки

void handler(int signal){	//убийца всех дочерних процессов
	for (int i = 0; i < chld_sz; i++){
		if (children[i] > 0){	//нулями заполнены звенья, которые ещё не запустились
			kill(children[i], SIGINT);
		}
	}
	stopflag = 1;
}

void print(){ 	// стандартное приветствие
	char pwd[PATH_MAX];
	getcwd(pwd, PATH_MAX);
	char* user = getenv("USER");
	char host[256];
	gethostname(host, 256);
	printf("\x1b[1;31m%s@%s\x1b[0m:\x1b[34;1m%s\x1b[37m$\x1b[0m\n", user, host, pwd);
	fflush(stdout);
}

void read_file(char *word){ 	//если открывается 2ой файл на чтение то игнорит его
	if (readfd == -1 && readflag == 0){
		readfd = open(word, O_RDONLY);
		if (readfd < 0){
			readflag = 0;
			return;
		}
		readflag++;		//служит для проверки открыт ли файл на чтение
	}
}

void write_file(char *word, int append){	//если открывает 2ой файл на запись, то игнорит его
	if (writefd == -1 && writeflag == 0){
		int flags = O_CREAT | O_WRONLY;
		if (append){
			flags = flags | O_APPEND;	//">>" дописывает в конец
		} else {
			flags = flags | O_TRUNC;	//">" затирает содержимое
		}
		writefd = open(word, flags, 0664);
		if (writefd < 0){
			writeflag = 0;
			return;
		}
		writeflag++;	//служит для проверки открыт ли файл на запись
	}
}

char *get_word(char *end){
	char c;
	if (read(0, &c, sizeof(char)) < 0){
		perror("word was not read");
		*end  = '\0';
		return NULL;
	}
	if (c == '>' || c == '<'){	//проверка на открытие файла
		*end = c;
		return NULL;
	}
	char *array = NULL;
	int array_size = 0;
	int flg = 0;	// этот флаг нужен для проверок ковычек
	if (c == '"'){
		flg = 1;	
		if (read(0, &c, sizeof(char)) < 0){
			perror("word was not read");
			*end  = '\0';
			return NULL;
		}
	}
	while (c != '\n' && ((c != ' ' && c != '\t' && c != '|') || flg)){	//всякие проверки, но окончанием команды всегда будет ENTER!!!
		if (c == '"'){
			if (read(0, &c, sizeof(char)) < 0){
				perror("word was not read");
				free(array);
				*end  = '\0';
				return NULL;
			}	
			if(c != ' ' && c != '\n' && c != '|' && c != '\t'){	//если после ковычек не идет SPACE, TAB, PIPE или ENTER
				free(array);
				*end = '\0';
				return NULL;
			}
			flg = 0;
			break;
		}
		char *temp_array = realloc(array, (++array_size) * sizeof(char));
		if (temp_array == NULL){
			perror("realloc error");
			free(array);
			*end = '\0';
			return NULL;
		}
		array = temp_array;
		array[array_size - 1] = c;
		if (read(0, &c, sizeof(char)) < 0){
			perror("word was not read");
			*end  = '\0';
			return NULL;
		}
	}
	if (flg){	//если мы не встретили закрывающиеся ковычки, то не позволяем пользователю вводить что-то дальше
		free(array);
		*end = '\0';
		return NULL;
	}
	*end = c;
	if (array == NULL){
		return NULL;
	}
	char *temp_array = realloc(array, (++array_size) * sizeof(char)); // выделяем память на окончание слова
	if (temp_array == NULL){
		perror("realloc error");
		free(array);
		*end = '\0';
		return NULL;
	}
	array = temp_array;
	array[array_size - 1] = '\0';	// окончание словa будет символ 
	return array;
}

char **get_list(char *symbol){
	int arg_c = 0;
	char **list = NULL, *word = NULL; 
	*symbol = '\0';
	while (*symbol != '\n' && *symbol != '|'){ // проверка на окончание команды или pipe
		word = get_word(&(*symbol));
		if (word != NULL){	//если вдруг вернул ничего, то нужно проверить на ошибку или на заврешение команды(или пробел/таб в начале)
			if (strcmp(word, "&") == 0){	//фоновый режим
				free(word);
				background = 1;
				if (list == NULL){
					return NULL;
				}
				char **temp_array = realloc(list, (arg_c + 1) * sizeof(char *)); //список тоже нужно закончить NULL, иначе execvp читает за его границей
				if (temp_array == NULL){
					perror("realloc error");
					for (int i = 0; i < arg_c; i++){
						free(list[i]);
					}
					free(list);
					*symbol = '\0';
					return NULL;
				}
				list = temp_array;
				list[arg_c] = NULL;
				return list;
			}
		}
		if (*symbol == '<' || *symbol == '>'){ // проверка на открытие файла
			char tmp = *symbol; // сохраняем нашу переменную
			int append = 0;	// ">" или ">>"
			word = get_word(&(*symbol)); // ищем наш файл, который нужно прочитать/ на который нужно записать
			while (word == NULL && (*symbol != '\n' && *symbol != '|')){
				if (word == NULL && *symbol == '\0'){ //если возникла ошибка при поиске имени файла, то завершаем
					for (int i = 0; i < arg_c; i++){
						free(list[i]);
					}
					free(list);
					if (writeflag){
						close(writefd);
						writeflag = 0;
					}
					if (readflag){
						close(readfd);
						readflag = 0;
					}
					return NULL;
				}
				if (tmp == '>' && *symbol == '>'){	//второй ">" подряд — это дозапись
					append = 1;
				}
				word = get_word(&(*symbol));	// продолжаем искать имя файла
			}
			if (word == NULL && (*symbol == '\n' || *symbol == '|')){	//если после > или < вдруг нажали на ENTER или поставили PIPE
				perror("name of file not found");
				for (int i = 0; i < arg_c; i++){
					free(list[i]);
				}
				free(list);
				*symbol = '\0';
				if (writeflag){
					close(writefd);
					writeflag = 0;
				}
				if (readflag){
					close(readfd);
					readflag = 0;
				}
				return NULL;
			}
			if (tmp == '<'){
				read_file(word);
				if (readfd < 0 && readflag == 0){ // проверка, если файл не открылся на чтение
					perror("file did not open");
					*symbol = '\0';
					for (int i = 0; i < arg_c; i++){
						free(list[i]);
					}
					free(list);
					return NULL;
				}
			} else {
				write_file(word, append);
				if (writeflag == 0 && writefd < 0){ // проверка, если файл не открылся на запись
					perror("file did not open");
					*symbol = '\0';
					for (int i = 0; i < arg_c; i++){
						free(list[i]);
					}
					free(list);
					return NULL;
				}
			}
			free(word);
			word = NULL;
		}
		if (word == NULL && *symbol == '\0'){ // проверка на ошибку в get_word
			for (int i = 0; i < arg_c; i++){
				free(list[i]);
			}
			free(list);
			if (writeflag){
				close(writefd);
				writeflag = 0;
			}
			if (readflag){
				close(readfd);
				readflag = 0;
			}
			return NULL;
		} else if (word == NULL && (*symbol == '\n' || *symbol == '|')){ // проверка на окончание команды, например: "ls\t\n" или "ls |"
			break;
		} else if (word == NULL && (*symbol == ' ' || *symbol == '\t')){ // проверка на SPACE или TAB
			continue;
		}
		char **temp_array = realloc(list, (arg_c + 1) * sizeof(char *));
		if (temp_array == NULL){
			perror("realloc error");
			for (int i = 0; i < arg_c; i++){
				free(list[i]);
			}
			free(list);
			free(word);
			if (writeflag){
				close(writefd);
				writeflag = 0;
			}
			if (readflag){
				close(readfd);
				readflag = 0;
			}
			*symbol = '\0';
			return NULL;
		}
		list = temp_array;
		list[arg_c] = word;
		arg_c++;
	}
	if (list == NULL){
		return NULL;
	}
	char **temp_array = realloc(list, (arg_c + 1) * sizeof(char *)); //выделяем указатель на NULL
	if (temp_array == NULL){
		perror("realloc error");
		for (int i = 0; i < arg_c; i++){
			free(list[i]);
		}
		free(list);
		if (writeflag){
			close(writefd);
			writeflag = 0;
		}
		if (readflag){
			close(readfd);
			readflag = 0;
		}
		*symbol = '\0';
		return NULL;
	}
	list = temp_array;
	list[arg_c] = NULL; //заканчиваем наш "список" NULL
	return list;
}

void clear_list(char ***list){ // очистка массива указателей на предложение
	for(int i = 0; ;i++){
		if ((*list)[i] == NULL){
			free((*list)[i]);
			break;
		}
		free((*list)[i]);
	}
	free(*list);
}

void clear_cmd(char ****cmd, int arg_c){ // очистка массива указателей на предложения
	for(int i = 0; i < arg_c; i++){
		clear_list(&((*cmd)[i]));
	}
	free(*cmd);
}

char ***get_cmd(int *arg_c, int *err){
	*err = 0; //err == 1 - выход из программы, err == 0 - нормальное считывание, -err == -1 - считывание с ошибкой или пустой список
	*arg_c = 0;
	char symbol;
	char **list = get_list(&symbol);
	if (list == NULL){ // если ошибка или ничего не было введено
		*err = -1;
		return NULL;
	}
	if (!strcmp(list[0], "exit") || !strcmp(list[0], "quit")){ //  проверка на завершения программы
		clear_list(&list);
		*err = 1;
		return NULL;
	}
	char ***cmd = malloc(sizeof(char **)); // создание команд
	if (cmd == NULL){
		perror("malloc error");
		free(list);
		clear_cmd(&cmd, *arg_c);
		*err = -1;
		return NULL;
	}
	cmd[0] = list;
	(*arg_c)++;
	while (symbol != '\n'){	// создание команд в цикле
		list = get_list(&symbol);
		if (list == NULL && symbol == '\0'){
			clear_cmd(&cmd, *arg_c);
			*err = 1;
			return NULL;
		}
		char ***temp_array = realloc(cmd, ((*arg_c) + 1) * sizeof(char **));
		if (temp_array == NULL){
			perror("realloc error");
			free(list);
			*err = -1;
			clear_cmd(&cmd, *arg_c);
			return NULL;
		}
		cmd = temp_array;
		cmd[*arg_c] = list;
		(*arg_c)++;
	}
	return cmd;
}

int mk_pipeline(int (**ppe)[2], int num){	//создание массивов pipe
	*ppe = malloc(num * 2 * sizeof(int *));
	if (ppe == NULL){
		perror("malloc error with pipe");
		return -1;
	}
	for (int i = 0; i < num; ++i){
		if (pipe((*ppe)[i]) == -1){
			for (int j = 0; j < i; j++){
				perror("pipe was not created");
				close((*ppe)[i][0]);
				close((*ppe)[i][1]);
			}
			free(ppe);
			return -1;
		}
	}	
	return 0;
}

void forget_children(void){	//сначала обнуляем размер, чтобы handler не пошёл по освобождённой памяти
	chld_sz = 0;
	free(children);
	children = NULL;
}

void mk_chld_proc(char ***cmd, int arg_c){
    if (strcmp(cmd[0][0], "cd") == 0) { // смена директории
		const char *home = getenv("HOME");
		if (cmd[0][1] == NULL || strcmp(cmd[0][1] , "~") == 0) {
			chdir(home);
		} else {
			chdir(cmd[0][1]);
		}
		return;
    }
	int (*ppe)[2] = NULL;
	int const pipe_c = arg_c - 1;
	int tmp = mk_pipeline(&ppe, pipe_c);
	if (tmp == -1){
		return;
	}
	children = calloc(arg_c, sizeof(pid_t));	//нули означают "звено ещё не запущено"
	if (children == NULL){
		perror("malloc error");
		for (int i = 0; i < pipe_c; ++i){
			close(ppe[i][0]);
			close(ppe[i][1]);
		}
		free(ppe);
		return;
	}
	chld_sz = arg_c;
	for(int i = 0; i < arg_c; i++){
		child = fork();
		if (child == -1){
			perror("fork failed");
			for (int i = 0; i < pipe_c; ++i){
				close(ppe[i][0]);
				close(ppe[i][1]);
			}
			free(ppe);
			forget_children();
			return;
		} else if (child == 0){
			if (i == 0 && readflag == 1){
				dup2(readfd, 0);
				close(readfd);
			}
			if (ppe != NULL){
				if (i != 0){
					dup2(ppe[i - 1][0], 0);
				}
				if (i != pipe_c){
					dup2(ppe[i][1], 1);
				}
			}           
			for (int j = 0; j < pipe_c; j++) {
				close(ppe[j][0]);
				close(ppe[j][1]);
			}
			if (i == pipe_c && writeflag == 1){
				dup2(writefd, 1);
				close(writefd);
			}
			if (execvp(cmd[i][0], cmd[i]) < 0){
				if (writeflag){
					close(writefd);
				}
				if (readflag){
					close(readfd);
				}
				perror("exec failed");
				exit(1);
			}
		}
		children[i] = child;	//звено запущено — теперь handler сможет его убить
		if (stopflag == 1){
			stopflag = 0;
			break;
		}
	}
	for (int i = 0; i < arg_c; ++i){
		if (i != pipe_c){	
			close(ppe[i][0]);
			close(ppe[i][1]);
		}
		if (background == 0){
			wait(NULL);
		} else {
			bckgrd_count++;
			char buf = '[';
			write(1, &buf, sizeof(char));
			int temp = bckgrd_count;
			char digits[16];	//цифры набираются с конца, поэтому пишем их в обратном порядке
			int digit_c = 0;
			do {
				digits[digit_c++] = temp % 10 + '0';
				temp /= 10;
			} while (temp != 0);
			while (digit_c != 0){
				buf = digits[--digit_c];
				write(1, &buf, sizeof(char));
			}
			buf = ']';
			write(1, &buf, sizeof(char));
			buf = ' ';
			write(1, &buf, sizeof(char));
			pid_t temp_t = child;
			digit_c = 0;
			do {
				digits[digit_c++] = temp_t % 10 + '0';
				temp_t /= 10;
			} while (temp_t != 0);
			while (digit_c != 0){
				buf = digits[--digit_c];
				write(1, &buf, sizeof(char));
			}
			buf = '\n';
			write(1, &buf, sizeof(char));
			background = 0;
		}
	}
	free(ppe);
	forget_children();
	return;
}

void back_to_begin(char ****cmd, int arg_c){
	if (cmd != NULL){
		clear_cmd(cmd, arg_c);
	}
	if (writeflag){
		close(writefd);
	}
	if (readflag){
		close(readfd);
	}
	writefd = -1;
	readfd = -1;
	writeflag = 0;
	readflag = 0;
	stopflag = 0;
	background = 0;
	usleep(500000);	//полсекунды: sleep() принимает целые секунды и округлил бы до нуля
}

int main(){
	signal(SIGINT, handler);
	while (1){
		int err = 0, arg_c = 0;
		print();
		char ***cmd = get_cmd(&arg_c, &err);
		if (err == 1){
			back_to_begin(&cmd, arg_c);
			break;
		} else if (err == -1){
			back_to_begin(NULL, 0);
			continue;
		}
		mk_chld_proc(cmd, arg_c);
		back_to_begin(&cmd, arg_c);
	}
	return 0;
}