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
int last_status = 0;	//код возврата последнего задания, по нему решается "&&"
int and_flag = 0;	//задание закончилось на "&&", а не на одиночном "&"
int append_flag = 0;	//файл открыт через ">>", а не через ">"
int peeked = -1;	//символ, прочитанный на один вперёд и пока не использованный
int eof_flag = 0;	//ввод закончился
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

int read_char(){	//чтение по одному символу с возможностью вернуть символ обратно
	if (peeked != -1){
		int c = peeked;
		peeked = -1;
		return c;
	}
	char c;
	int n = read(0, &c, sizeof(char));
	if (n == 0){
		eof_flag = 1;
		return -1;
	}
	if (n < 0){
		perror("word was not read");
		return -1;
	}
	return c;
}

int is_separator(int c){	//символы, которые сами по себе слово и всегда отделяются от соседей
	return c == '|' || c == '&' || c == '<' || c == '>';
}

char *get_word(char *end){
	append_flag = 0;
	int c = read_char();
	if (c == -1){
		*end  = '\0';
		return NULL;
	}
	if (is_separator(c)){	//разделитель разбираем целиком: ">>" и "&&" состоят из двух символов
		int next = read_char();
		if (c == '>' && next == '>'){
			append_flag = 1;
		} else if (c == '&' && next == '&'){
			and_flag = 1;
		} else if (next != -1){
			peeked = next;	//разделитель оказался одиночным — заглянутый символ возвращаем в поток
		}
		*end = c;
		return NULL;
	}
	char *array = NULL;
	int array_size = 0;
	int flg = 0;	// этот флаг нужен для проверок ковычек
	if (c == '"'){
		flg = 1;
		c = read_char();
		if (c == -1){
			*end  = '\0';
			return NULL;
		}
	}
	while (c != '\n' && ((c != ' ' && c != '\t' && !is_separator(c)) || flg)){	//всякие проверки, но окончанием команды всегда будет ENTER!!!
		if (c == '"'){
			c = read_char();
			if (c == -1){
				free(array);
				*end  = '\0';
				return NULL;
			}
			if(c != ' ' && c != '\n' && c != '\t' && !is_separator(c)){	//если после ковычек не идет SPACE, TAB, ENTER или разделитель
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
		c = read_char();
		if (c == -1){
			free(array);
			*end  = '\0';
			return NULL;
		}
	}
	if (flg){	//если мы не встретили закрывающиеся ковычки, то не позволяем пользователю вводить что-то дальше
		free(array);
		*end = '\0';
		return NULL;
	}
	if (is_separator(c)){	//слово упёрлось в разделитель — вернём его в поток, следующий вызов разберёт его целиком
		peeked = c;
		*end = ' ';
	} else {
		*end = c;
	}
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

void skip_line(){	//после неудачной левой части "&&" остаток строки не выполняется
	int c = read_char();
	while (c != -1 && c != '\n'){
		c = read_char();
	}
}

char **get_list(char *symbol){
	int arg_c = 0;
	char **list = NULL, *word = NULL; 
	*symbol = '\0';
	while (*symbol != '\n' && *symbol != '|' && *symbol != '&'){ // проверка на окончание команды, pipe или конец задания
		word = get_word(&(*symbol));
		if (*symbol == '<' || *symbol == '>'){ // проверка на открытие файла
			char tmp = *symbol; // сохраняем нашу переменную
			int append = append_flag;	// ">" или ">>" — запоминаем до того, как следующий get_word перепишет флаг
			word = get_word(&(*symbol)); // ищем наш файл, который нужно прочитать/ на который нужно записать
			while (word == NULL && (*symbol != '\n' && *symbol != '|' && *symbol != '&')){
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
				word = get_word(&(*symbol));	// продолжаем искать имя файла
			}
			if (word == NULL && (*symbol == '\n' || *symbol == '|' || *symbol == '&')){	//если после > или < вдруг нажали на ENTER, поставили PIPE или закончили задание
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
		} else if (word == NULL && (*symbol == '\n' || *symbol == '|' || *symbol == '&')){ // проверка на окончание команды, например: "ls\t\n" или "ls |"
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

char ***get_cmd(int *arg_c, int *err, char *sep){
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
	while (symbol == '|'){	// звенья одного конвейера собираем в цикле
		list = get_list(&symbol);
		if (list == NULL){	//звено пустое или разобрано с ошибкой — отменяем задание, но не выходим из шелла
			clear_cmd(&cmd, *arg_c);
			*err = -1;
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
	*sep = symbol;	//чем задание кончилось: концом строки, "&" или "&&"
	return cmd;
}

int mk_pipeline(int (**ppe)[2], int num){	//создание массивов pipe
	if (num == 0){	//одна команда без конвейера — каналы не нужны
		*ppe = NULL;
		return 0;
	}
	*ppe = malloc(num * 2 * sizeof(int));
	if (*ppe == NULL){	//проверять надо сам массив, а не адрес указателя на него
		perror("malloc error with pipe");
		return -1;
	}
	for (int i = 0; i < num; ++i){
		if (pipe((*ppe)[i]) == -1){
			perror("pipe was not created");
			for (int j = 0; j < i; j++){	//закрываем те каналы, которые успели открыться
				close((*ppe)[j][0]);
				close((*ppe)[j][1]);
			}
			free(*ppe);
			*ppe = NULL;
			return -1;
		}
	}	
	return 0;
}

void forget_children(){	//сначала обнуляем размер, чтобы handler не пошёл по освобождённой памяти
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
	for (int i = 0; i < pipe_c; ++i){	//каналы закрываем все сразу: пока родитель держит конец, читатель не увидит EOF
		close(ppe[i][0]);
		close(ppe[i][1]);
	}
	if (background == 0){
		for (int i = 0; i < arg_c; ++i){	//ждём все звенья конвейера, а не одно
			int status = 0;
			pid_t done = wait(&status);
			if (done == children[arg_c - 1]){	//код возврата задания — это код его последнего звена
				if (WIFEXITED(status)){
					last_status = WEXITSTATUS(status);
				} else {
					last_status = 1;	//убит сигналом — считаем неудачей
				}
			}
		}
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
		pid_t temp_t = children[arg_c - 1];	//отчитываемся PID последнего звена конвейера
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
		int err = 0;
		char sep = '\n';
		print();
		do {	//в одной строке может быть несколько заданий через "&" или "&&"
			int arg_c = 0;
			and_flag = 0;
			char ***cmd = get_cmd(&arg_c, &err, &sep);
			if (err == 1 || eof_flag){
				back_to_begin(&cmd, arg_c);
				return 0;
			} else if (err == -1){
				back_to_begin(NULL, 0);
				break;
			}
			int and_then = and_flag;	//запоминаем до запуска: следующий разбор флаг перезапишет
			background = (sep == '&' && and_then == 0);
			mk_chld_proc(cmd, arg_c);
			back_to_begin(&cmd, arg_c);
			if (and_then && last_status != 0){
				skip_line();	//левая часть "&&" завершилась с ошибкой
				break;
			}
		} while (sep == '&');
	}
	return 0;
}