#!/bin/sh
# Сборка и проверки. Запускать из любого места: sh test.sh
# Выход 0 — всё прошло, 1 — что-то упало.
set -u

root=$(cd "$(dirname "$0")" && pwd)
cd "$root" || exit 1
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT INT TERM
fail=0

echo "== $(uname -s) $(uname -m), $(gcc --version 2>&1 | head -1) =="

echo "-- сборка --"
for t in sources/easy_terminal examples/demo_pipe examples/reader examples/catcher; do
	rm -f "$t"
	if make "$t" >/dev/null 2>"$tmp/build.log"; then
		echo "  OK    $t"
	else
		echo "  FAIL  $t"; cat "$tmp/build.log"; fail=1
	fi
done
if [ ! -x examples/catcher ] || [ ! -x sources/easy_terminal ]; then
	echo "нет бинарников, дальше проверять нечего"
	exit 1
fi

# A: catcher ловит SIGINT сам по себе
echo "-- A: catcher отдельно, kill -INT --"
./examples/catcher > "$tmp/a.out" 2>&1 &
cpid=$!
sleep 1
kill -INT "$cpid" 2>/dev/null
wait "$cpid"; rc=$?
if [ "$rc" -eq 0 ] && grep -q HELLO "$tmp/a.out"; then
	echo "  OK    напечатал HELLO, вышел с $rc"
else
	echo "  FAIL  код возврата $rc, вывод: $(cat "$tmp/a.out")"; fail=1
fi

# B: сигнал уходит шеллу, шелл рассылает его звеньям команды и остаётся жив
echo "-- B: catcher под easy_terminal, SIGINT шеллу --"
{ printf 'examples/catcher\n'; sleep 4; } | ./sources/easy_terminal > "$tmp/b.out" 2>&1 &
spid=$!
sleep 1
kill -INT "$spid" 2>/dev/null
sleep 1
alive=no; kill -0 "$spid" 2>/dev/null && alive=yes
wait "$spid" 2>/dev/null; rc=$?
if grep -q HELLO "$tmp/b.out" && [ "$alive" = yes ]; then
	echo "  OK    ребёнок получил SIGINT, шелл пережил сигнал (вышел с $rc по концу ввода)"
else
	echo "  FAIL  HELLO=$(grep -c HELLO "$tmp/b.out"), шелл жив после сигнала: $alive, код $rc"
	sed -n '1,20p' "$tmp/b.out" | cat -v; fail=1
fi

# C: ничего не зависло. Только сообщаем — сами никого не убиваем
echo "-- C: не осталось висящих catcher --"
left=$(ps -ax -o pid=,command= 2>/dev/null | grep "$root/examples/catcher" | grep -cv grep)
if [ "${left:-0}" -eq 0 ]; then
	echo "  OK    нет"
else
	echo "  WARN  ещё живы: $left (убейте вручную)"
fi

# D: цикл ожидания не должен свернуться оптимизатором
echo "-- D: catcher на -O0/-O2/-O3 --"
for O in -O0 -O2 -O3; do
	printf "  %-4s " "$O"
	if ! gcc "$O" -Wall -Werror examples/catcher.c -o "$tmp/c" 2>"$tmp/o.log"; then
		echo "не собрался"; head -3 "$tmp/o.log"; fail=1; continue
	fi
	"$tmp/c" > "$tmp/o.out" 2>&1 &
	p=$!
	sleep 1; kill -INT "$p" 2>/dev/null; sleep 1
	if kill -0 "$p" 2>/dev/null; then
		echo "ЗАВИС — цикл ожидания свернули"; kill -9 "$p" 2>/dev/null; fail=1
	else
		wait "$p"; echo "OK, вывод: $(tr -d '\n' < "$tmp/o.out")"
	fi
done

# E: возможности самого шелла. Команды пишут в файлы через перенаправление,
# иначе вывод смешался бы с приглашением, которое шелл печатает в stdout
echo "-- E: пайпы, перенаправления, && , фон --"
sh_bin="$root/sources/easy_terminal"
work="$tmp/work"
mkdir -p "$work"
printf 'one\ntwo\n' > "$work/in.txt"
( cd "$work" && "$sh_bin" >/dev/null 2>&1 ) <<'CMDS'
echo hello | tr a-z A-Z > pipe.out
echo one > redir.out
echo two >> redir.out
sort < in.txt | uniq | tr a-z A-Z > chain.out
true && echo yes > and_ok.out
false && echo no > and_skip.out
echo tight>nospace.out
echo a > job1.out & echo b > job2.out
CMDS

expect() {	# expect <что проверяем> <ожидаемое, строки через /> <файл>
	got=$(tr '\n' '/' < "$work/$3" 2>/dev/null)
	if [ "$got" = "$2" ]; then
		echo "  OK    $1"
	else
		echo "  FAIL  $1: ждали [$2], получили [$got]"; fail=1
	fi
}

expect "пайп"                  "HELLO/"     pipe.out
expect "> и >>"                "one/two/"   redir.out
expect "< и цепочка из трёх"   "ONE/TWO/"   chain.out
expect "true && — правая часть выполнилась" "yes/" and_ok.out
expect "оператор без пробелов" "tight/"     nospace.out
expect "две задачи в строке (1)" "a/"       job1.out
expect "две задачи в строке (2)" "b/"       job2.out

if [ -e "$work/and_skip.out" ]; then
	echo "  FAIL  false && — правая часть выполнилась, хотя не должна была"; fail=1
else
	echo "  OK    false && — правая часть пропущена"
fi

# фон проверяем по времени: с "&" шелл не должен ждать sleep
started=$(date +%s)
printf 'sleep 6 &\n' | "$sh_bin" >/dev/null 2>&1
elapsed=$(( $(date +%s) - started ))
if [ "$elapsed" -lt 4 ]; then
	echo "  OK    фон: шелл не стал ждать sleep 6 ($elapsed c)"
else
	echo "  FAIL  фон: шелл прождал $elapsed c, значит выполнил на переднем плане"; fail=1
fi

[ "$fail" -eq 0 ] && echo "ИТОГ: всё прошло" || echo "ИТОГ: есть падения"
exit "$fail"
