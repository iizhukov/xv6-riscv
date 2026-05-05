#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FIFO_PATH      "/tmp/logserver.fifo"
#define LOG_PATH       "/tmp/logserver.log"
#define ALARM_INTERVAL 5
#define READ_BUF_SIZE  4096

static volatile sig_atomic_t flag_sigterm = 0;
static volatile sig_atomic_t flag_sigint  = 0;
static volatile sig_atomic_t flag_sigalrm = 0;
static volatile sig_atomic_t flag_sigusr1 = 0;
static volatile sig_atomic_t flag_sighup  = 0;

static int  is_daemon      = 0;
static int  alarm_interval = ALARM_INTERVAL;
static const char *fifo_path = FIFO_PATH;
static const char *log_path  = LOG_PATH;

static unsigned long stat_messages = 0;
static unsigned long stat_bytes    = 0;
static unsigned long stat_alarms   = 0;

static void handle_sigterm(int sig) { (void)sig; flag_sigterm = 1; }
static void handle_sigint (int sig) { (void)sig; flag_sigint  = 1; }
static void handle_sigalrm(int sig) { (void)sig; flag_sigalrm = 1; }
static void handle_sigusr1(int sig) { (void)sig; flag_sigusr1 = 1; }
static void handle_sighup (int sig) { (void)sig; flag_sighup  = 1; }

static void logmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

static void print_stats(void)
{
    logmsg("[статистика] сообщений=%lu  байт=%lu  будильников=%lu",
           stat_messages, stat_bytes, stat_alarms);
}

static void daemonize(int from_sighup)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid > 0) exit(0);

    if (setsid() < 0) { perror("setsid"); exit(1); }

    pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid > 0) exit(0);

    int devnull = open("/dev/null", O_RDONLY);
    if (devnull < 0) { perror("open /dev/null"); exit(1); }
    dup2(devnull, STDIN_FILENO);
    close(devnull);

    int logfd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (logfd < 0) { perror("open log file"); exit(1); }
    dup2(logfd, STDOUT_FILENO);
    dup2(logfd, STDERR_FILENO);
    close(logfd);

    is_daemon = 1;

    if (from_sighup)
        logmsg("[демон] демонизирован по SIGHUP");
    else
        logmsg("[демон] запущен как демон");

    if (from_sighup)
        print_stats();
}

static void setup_fifo(void)
{
    if (mkfifo(fifo_path, 0600) < 0) {
        if (errno != EEXIST) {
            fprintf(stderr, "mkfifo %s: %s\n", fifo_path, strerror(errno));
            exit(1);
        }

        struct stat st;
        if (stat(fifo_path, &st) < 0) {
            fprintf(stderr, "stat %s: %s\n", fifo_path, strerror(errno));
            exit(1);
        }

        if (!S_ISFIFO(st.st_mode)) {
            fprintf(stderr, "%s: существует, но не является FIFO\n", fifo_path);
            exit(1);
        }

        logmsg("[инфо] используется существующий FIFO %s", fifo_path);
    } else {
        logmsg("[инфо] создан FIFO %s", fifo_path);
    }
}

static void register_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigterm;
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        perror("sigaction SIGTERM");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction SIGINT");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGQUIT, &sa, NULL) < 0) {
        perror("sigaction SIGQUIT");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigalrm;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction SIGALRM");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigusr1;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction SIGUSR1");
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sighup;
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction SIGHUP");
        exit(1);
    }
}

static int handle_eintr_flags(void)
{
    if (flag_sigalrm) {
        flag_sigalrm = 0;
        stat_alarms++;
        logmsg("[пульс] работаю, жду данных  (сообщений=%lu  байт=%lu  будильников=%lu)",
               stat_messages, stat_bytes, stat_alarms);
        alarm(alarm_interval);
    }
    if (flag_sigusr1) {
        flag_sigusr1 = 0;
        print_stats();
    }
    if (flag_sighup && !is_daemon) {
        flag_sighup = 0;
        logmsg("[инфо] получен SIGHUP, демонизируюсь...");
        daemonize(1);
    }
    return flag_sigterm || flag_sigint;
}

static int read_loop(int fd)
{
    char buf[READ_BUF_SIZE + 1];
    int last_char = '\n';
    int got_sigint = 0;

    while (1) {
        ssize_t n = read(fd, buf, READ_BUF_SIZE);

        if (n > 0) {
            buf[n] = '\0';
            last_char = buf[n - 1];
            fputs(buf, stdout);
            fflush(stdout);
            stat_bytes += (unsigned long)n;
            continue;
        }

        if (n == 0) {
            if (last_char != '\n') { putchar('\n'); fflush(stdout); }
            return 0;
        }

        if (errno != EINTR) {
            fprintf(stderr, "read: %s\n", strerror(errno));
            close(fd);
            exit(1);
        }

        if (flag_sigint && !got_sigint) {
            got_sigint = 1;
            logmsg("[инфо] получен SIGINT, дочитываю FIFO...");
        }

        if (handle_eintr_flags()) {
            if (got_sigint) {
                ssize_t m;
                while (1) {
                    m = read(fd, buf, READ_BUF_SIZE);
                    if (m > 0) {
                        buf[m] = '\0';
                        last_char = buf[m - 1];
                        fputs(buf, stdout);
                        fflush(stdout);
                        stat_bytes += (unsigned long)m;
                        continue;
                    }
                    if (m == 0) break;
                    if (errno == EINTR) {
                        if (flag_sigterm) break;
                        handle_eintr_flags();
                        continue;
                    }
                    break;
                }
            }
            if (last_char != '\n') { putchar('\n'); fflush(stdout); }
            return 1;
        }
    }
}

static void run(void)
{
    alarm(alarm_interval);
    int stop = 0;

    while (!stop) {
        if (handle_eintr_flags()) break;

        int fd = -1;
        while (fd < 0) {
            fd = open(fifo_path, O_RDONLY);
            if (fd >= 0) break;
            if (errno == EINTR) {
                if (handle_eintr_flags()) { stop = 1; break; }
                continue;
            }
            fprintf(stderr, "open %s: %s\n", fifo_path, strerror(errno));
            exit(1);
        }
        if (stop) break;

        logmsg("[инфо] FIFO открыт, читаем...");
        int result = read_loop(fd);
        close(fd);
        stat_messages++;

        if (result) break;

        logmsg("[инфо] FIFO закрыт (EOF)");

        if (handle_eintr_flags()) break;
    }

    alarm(0);

    if (flag_sigterm)
        logmsg("[инфо] завершение по SIGTERM");
    else if (flag_sigint)
        logmsg("[инфо] завершение по SIGINT");

    print_stats();

    if (unlink(fifo_path) < 0)
        fprintf(stderr, "unlink %s: %s\n", fifo_path, strerror(errno));
    else
        logmsg("[инфо] FIFO %s удалён", fifo_path);
}

static void print_help(const char *prog)
{
    printf(
        "Использование: %s [-d] [-f FIFO] [-l ЛОГ] [-n ИНТЕРВАЛ]\n"
        "\n"
        "Лог-сервер через именованный канал (FIFO).\n"
        "Читает всё, что пишут в канал, и выводит в протокол.\n"
        "\n"
        "Опции:\n"
        "  -d            запустить сразу как демон\n"
        "  -f FIFO       путь к именованному каналу (умолчание: %s)\n"
        "  -l ЛОГ        путь к файлу протокола, используется в режиме демона\n"
        "                (умолчание: %s)\n"
        "  -n ИНТЕРВАЛ   интервал пульса в секундах (умолчание: %d)\n"
        "  -h, --help    показать эту справку\n"
        "\n"
        "Сигналы:\n"
        "  SIGTERM (kill -15)   немедленное завершение\n"
        "  SIGINT  (Ctrl+C)     дочитать текущий канал до EOF, затем завершить\n"
        "  SIGQUIT (Ctrl+\\)    игнорируется\n"
        "  SIGUSR1 (kill -USR1) вывести статистику прямо сейчас\n"
        "  SIGHUP  (kill -HUP)  демонизироваться (только в foreground-режиме)\n"
        "\n"
        "Примеры:\n"
        "  %s -n 10\n"
        "  echo \"привет\" > %s\n"
        "  cat файл.txt  > %s\n",
        prog,
        FIFO_PATH, LOG_PATH, ALARM_INTERVAL,
        prog, FIFO_PATH, FIFO_PATH
    );
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }

    int start_as_daemon = 0;
    int opt;

    while ((opt = getopt(argc, argv, "hdf:l:n:")) != -1) {
        switch (opt) {
        case 'h': print_help(argv[0]); return 0;
        case 'd': start_as_daemon = 1; break;
        case 'f': fifo_path = optarg;  break;
        case 'l': log_path  = optarg;  break;
        case 'n':
            alarm_interval = atoi(optarg);
            if (alarm_interval <= 0) {
                fprintf(stderr, "интервал должен быть > 0\n");
                return 1;
            }
            break;
        default:
            fprintf(stderr, "Попробуйте '%s --help'\n", argv[0]);
            return 1;
        }
    }

    if (start_as_daemon)
        daemonize(0);

    register_signals();
    setup_fifo();

    logmsg("[инфо] лог-сервер запущен  pid=%d  fifo=%s  интервал=%dс",
           (int)getpid(), fifo_path, alarm_interval);

    run();
    return 0;
}
