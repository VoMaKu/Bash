#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t stop = 0;	//меняется из обработчика, поэтому volatile sig_atomic_t

void handler(int signal){	//ловит Ctrl+C вместо того, чтобы умереть от него
	ssize_t n = write(1, "HELLO\n", 6);	//write, а не printf: внутри обработчика можно только его
	(void)n;	//под -O2 glibc требует проверять результат write
	stop = 1;
}

int main(){
	signal(SIGINT, handler);
	while (!stop){	//крутится, пока не придёт SIGINT
	}
	return 0;
}
