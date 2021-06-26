#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>
#include <time.h>

#ifdef __WIN32__
#include <winsock2.h>
#define usleep(n) Sleep((n) / 1000)
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define  closesocket close
#endif

#define FFHTTPD_SERVER_PORT     8080
#define FFHTTPD_MAX_CONNECTION  1

static char *g_myweb_head = 
"HTTP/1.1 200 OK\r\n"
"Server: ffhttpd/1.0.0\r\n"
"Content-Type: text/html\r\n"
"Content-Length: %d\r\n"
"Accept-Ranges: bytes\r\n"
"Connection: close\r\n\r\n%s";

static char d2c(int d)
{
    if      (d >= 1  && d <= 9 ) return ('1' + d - 1 );
    else if (d >= 10 && d <= 31) return ('A' + d - 10);
    else return 0;
}

static void newsn(struct sockaddr_in *client, char *type, char *sn, int len)
{
    char   buffer[256];
    time_t tt    = time(NULL);
    struct tm *tm= localtime(&tt);
    FILE  *fp    = NULL;
    int    next  = 0x2001;

    snprintf(buffer, sizeof(buffer), "%s-%04d-%02d-%02d.log", type, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    if ((fp = fopen(buffer, "rb"))) { fscanf(fp, "%x", &next); fclose(fp); }
    else { fp = fopen(buffer, "wb"); fclose(fp); }

    if (next <= 0x2001) next = 0x2001;
    if (next >= 0x4000) {
        snprintf(sn, len, "SN_OUT_OF_RANGE");
    } else if (strcmp(type, "DPH-IP300B") == 0) {
        snprintf(sn, len, "55040866%02d%X%c%04X", tm->tm_year % 100, tm->tm_mon + 1, d2c(tm->tm_mday), next);
    } else if (strcmp(type, "DPH-IP350" ) == 0) {
        snprintf(sn, len, "55042090%02d%X%c%04X", tm->tm_year % 100, tm->tm_mon + 1, d2c(tm->tm_mday), next);
    }

    if ((fp = fopen(buffer, "rb+"))) {
        fprintf(fp, "%08X\r\n", next + 1);
        fseek(fp, 0, SEEK_END);
        snprintf(buffer, sizeof(buffer), "[%04d-%02d-%02d %02d:%02d:%02d] client %s request sn: %s\r\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, inet_ntoa(client->sin_addr), sn);
        fprintf(fp    , buffer);
        fprintf(stdout, buffer); fflush(stdout);
        fclose (fp);
    } else printf("failed to open sn log file: %s !\n", buffer);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;
    int fd_sock = -1, port = FFHTTPD_SERVER_PORT, i;

#ifdef __WIN32__
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed !\n"); fflush(stdout);
        goto done;
    }
#endif

    for (i=1; i<argc; i++) {
        if (strstr(argv[i], "--port=") == argv[i]) port = atoi(argv[i] + 7);
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    fd_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_sock == -1) { printf("failed to open socket !\n"); fflush(stdout); goto done; }
    if (bind(fd_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) { printf("failed to bind !\n"); fflush(stdout); goto done; }
    if (listen(fd_sock, FFHTTPD_MAX_CONNECTION) == -1) { printf("failed to listen !\n"); fflush(stdout); goto done; }

    printf("ffsnserver is running ...\n");
    printf("listen on port: %d\n\n", port); fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        int  addrlen, fd_conn, len;
        char buf[1024], sn[32], *type = "unknown", *p;

        addrlen = sizeof(client_addr);
        fd_conn = accept(fd_sock, (struct sockaddr*)&client_addr, &addrlen);
        if (fd_conn == -1) {
            printf("failed to accept !\n"); fflush(stdout);
            usleep(100 * 1000); continue;
        }

        len = recv(fd_conn, buf, sizeof(buf) - 1, 0); buf[len] = '\0';
        if ((p = strstr(buf, "GET /")) == buf) {
            p += 5; type = p;
            p = strstr(p, " HTTP");
            if (p) *p = '\0';
        }

        if (strcmp(type, "DPH-IP300B") == 0 || strcmp(type, "DPH-IP350") == 0) newsn(&client_addr, type, sn, sizeof(sn));
        else strncpy(sn, "ERROR", sizeof(sn));
        snprintf(buf, sizeof(buf), g_myweb_head, strlen(sn), sn);
        send(fd_conn, buf, strlen(buf), 0);
        closesocket(fd_conn);
    }

done:
    if (fd_sock > 0) closesocket(fd_sock);

#ifdef __WIN32__
    WSACleanup();
#endif
    return 0;
}
