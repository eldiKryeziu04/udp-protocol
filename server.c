#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <process.h>
#include <time.h>
#include <sys/stat.h>
#include <direct.h>

#pragma comment(lib, "ws2_32.lib")

#define MAIN_PORT 1200
#define HTTP_PORT 8080
#define MAX_CLIENTS 5
#define STORAGE_DIR "./server_files"

typedef struct {
    char ip[20];
    int port;
    int msg_count;
    char last_command[100];
    int is_active;
    int is_admin;
    time_t last_seen;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
int active_count = 0;

void log_client(struct sockaddr_in addr, int is_admin) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].is_active) {
            clients[i].is_active = 1;
            clients[i].is_admin = is_admin;
            clients[i].port = ntohs(addr.sin_port);
            strcpy(clients[i].ip, inet_ntoa(addr.sin_addr));
            clients[i].last_seen = time(NULL);
            clients[i].msg_count = 0;
            active_count++;
            break;
        }
    }
}

void http_thread(void* param) {
    SOCKET httpS = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { AF_INET, htons(HTTP_PORT), INADDR_ANY };
    bind(httpS, (struct sockaddr*)&addr, sizeof(addr));
    listen(httpS, 5);

    while (1) {
        SOCKET c = accept(httpS, NULL, NULL);
        char buffer[1024];
        recv(c, buffer, sizeof(buffer), 0);

        char body[2048] = "<html><body><h1>Statistikat e Serverit</h1><ul>";
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].is_active) {
                char entry[256];
                sprintf(entry, "<li>IP: %s | Admin: %s | Mesazhe: %d</li>",
                    clients[i].ip, clients[i].is_admin ? "PO" : "JO", clients[i].msg_count);
                strcat(body, entry);
            }
        }
        strcat(body, "</ul></body></html>");

        char response[3000];
        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(body), body);
        send(c, response, (int)strlen(response), 0);
        closesocket(c);
    }
}

void handle_command(SOCKET s, char* cmd, int isAdmin, struct sockaddr_in cAddr) {
    char response[4096] = ""; 
    char filename[100], content[2048];

    if (strncmp(cmd, "/list", 5) == 0) {
        if (!isAdmin) {
            strcpy(response, "GABIM: Vetem Admini mund te shikoj listen.");
        } else {
        system("dir /b server_files > list.txt");
        FILE *f = fopen("list.txt", "r");
        if (f) {
            size_t n = fread(response, 1, sizeof(response) - 1, f);
            response[n] = '\0';
            fclose(f);
            if (strlen(response) == 0) strcpy(response, "Folderi eshte bosh.");
        } else strcpy(response, "Gabim gjate listimit.");
    }
    }

    else if (strncmp(cmd, "/read", 5) == 0) {
        if (sscanf(cmd, "/read %s", filename) == 1) {
            char path[150]; sprintf(path, "server_files/%s", filename);
            FILE *f = fopen(path, "r");
            if (f) {
                size_t n = fread(response, 1, sizeof(response) - 1, f);
                response[n] = '\0';
                fclose(f);
            } else strcpy(response, "Skedari nuk u gjet.");
        } else strcpy(response, "Perdorimi: /read <filename>");
    }

    else if (strncmp(cmd, "/upload", 7) == 0) {
        if (!isAdmin) {
            strcpy(response, "GABIM: Vetem Admini mund te beje upload.");
        } else {
            if (sscanf(cmd, "/upload %s %[^\n]", filename, content) >= 2) {
                char path[150]; sprintf(path, "server_files/%s", filename);
                FILE *f = fopen(path, "w");
                if (f) {
                    fprintf(f, "%s", content);
                    fclose(f);
                    sprintf(response, "Skedari '%s' u ngarkua me sukses.", filename);
                } else strcpy(response, "Deshtoi krijimi i skedarit.");
            } else strcpy(response, "Perdorimi: /upload <emri> <teksti>");
        }
    }

    else if (strncmp(cmd, "/download", 9) == 0) {
        if (!isAdmin) {
            strcpy(response, "GABIM: Vetem Admini mund te beje download.");
        }
        else{
        if (sscanf(cmd, "/download %s", filename) == 1) {
            char path[150]; sprintf(path, "server_files/%s", filename);
            FILE *f = fopen(path, "r");
            if (f) {
                strcpy(response, "--- PERMBAJTJA E SHKARKUAR ---\n");
                size_t n = fread(response + strlen(response), 1, 2000, f);
                response[strlen(response) + n] = '\0';
                fclose(f);
            } else strcpy(response, "Gabim: Skedari nuk u gjet ne server.");
        }
        }
    }

    else if (strncmp(cmd, "/delete", 7) == 0) {
        if (!isAdmin) {
            strcpy(response, "GABIM: Vetem Admini mund te fshije.");
        } else {
            if (sscanf(cmd, "/delete %s", filename) == 1) {
                char path[150]; sprintf(path, "server_files/%s", filename);
                if (remove(path) == 0) sprintf(response, "U fshi: %s", filename);
                else strcpy(response, "Skedari nuk u gjet.");
            }
        }
    }

    else if (strncmp(cmd, "/search", 7) == 0) {
        if (!isAdmin) {
            strcpy(response, "GABIM: Vetem Admini mund te beje search.");
        } else {
        char keyword[100];
        if (sscanf(cmd, "/search %s", keyword) == 1) {
            char sysCmd[256];
            sprintf(sysCmd, "dir /b server_files | findstr /i \"%s\" > search.txt", keyword);
            system(sysCmd);
            FILE *f = fopen("search.txt", "r");
            if (f) {
                size_t n = fread(response, 1, sizeof(response) - 1, f);
                response[n] = '\0';
                fclose(f);
                if (strlen(response) == 0) strcpy(response, "Nuk u gjet asnje fajll.");
            }
        } else strcpy(response, "Perdorimi: /search <fjala>");
    }
    }
    else if (strncmp(cmd, "/info", 5) == 0) {
         if (!isAdmin) {
            strcpy(response, "GABIM: Vetem Admini mund te shikoj informatat e fajllave.");
        } else {
        if (sscanf(cmd, "/info %s", filename) == 1) {
            char path[150]; 
            sprintf(path, "server_files/%s", filename);
            
            struct stat file_stat;
            if (stat(path, &file_stat) == 0) {
                char dt_create[26], dt_mod[26];
                
                strcpy(dt_create, ctime(&file_stat.st_ctime));
                dt_create[strlen(dt_create)-1] = '\0';
                
                strcpy(dt_mod, ctime(&file_stat.st_mtime));
                dt_mod[strlen(dt_mod)-1] = '\0';

                sprintf(response, 
                    "INFOT PER SKEDARIN: %s\n"
                    "--------------------------\n"
                    "Madhesia: %ld bytes\n"
                    "Krijuar me: %s\n"
                    "Modifikuar: %s", 
                    filename, file_stat.st_size, dt_create, dt_mod);
            } else {
                strcpy(response, "Gabim: Skedari nuk u gjet.");
            }
        } else {
            strcpy(response, "Perdorimi: /info <filename>");
        }
    }
    }
    else {
        strcpy(response, "Komanda e panjohur.");
    }


    strcat(response, "\n");
    send(s, response, (int)strlen(response), 0);
}

void save_log(const char* ip, const char* cmd) {
    FILE *f = fopen("./server_files/logs.txt", "a"); 
    if (f == NULL) {
        printf("[DEBUG] Deshtoi hapja e skedarit te log-ve! Error: %d\n", errno);
        return;
    }

    time_t tani = time(NULL);
    char *koha = ctime(&tani);
    koha[strlen(koha) - 1] = '\0'; 

    fprintf(f, "[%s] IP: %s | Komanda: %s\n", koha, ip, cmd);
    
    fflush(f);    fclose(f);
    printf("[DEBUG] Log-u u ruajt me sukses ne folderin 'server_files'.\n");
}
int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
char hostName[256];
struct hostent *hostEntry;
char primaryIP[20] = "127.0.0.1"; 

if (gethostname(hostName, sizeof(hostName)) == 0) {
    hostEntry = gethostbyname(hostName);
    if (hostEntry != NULL) {
        for (int i = 0; hostEntry->h_addr_list[i] != NULL; i++) {
            struct in_addr addr;
            memcpy(&addr, hostEntry->h_addr_list[i], sizeof(struct in_addr));
            char* ip_str = inet_ntoa(addr);

            if (strncmp(ip_str, "192.", 4) == 0 || strncmp(ip_str, "10.", 3) == 0) {
                strcpy(primaryIP, ip_str);
                break; 
            }
        }
    }
}

printf("========================================\n");
printf("              SERVERI AKTIV             \n");
printf("========================================\n");
printf("IP Adresa per Klientin: %s\n", primaryIP);
printf("Porti:                  %d\n", MAIN_PORT);
printf("HTTP Monitorimi:        http://%s:%d\n", primaryIP, HTTP_PORT);
printf("========================================\n");
fflush(stdout);
    _mkdir(STORAGE_DIR);

    _beginthread(http_thread, 0, NULL);

    SOCKET serverS = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sAddr = { AF_INET, htons(MAIN_PORT), INADDR_ANY };
    bind(serverS, (struct sockaddr*)&sAddr, sizeof(sAddr));
    listen(serverS, MAX_CLIENTS);

    printf("SERVERI AKTIV\nMain Port: %d | HTTP Port: %d\n", MAIN_PORT, HTTP_PORT);
    fflush(stdout);

    while (1) {
        struct sockaddr_in cAddr;
        int cLen = sizeof(cAddr);
        SOCKET clientS = accept(serverS, (struct sockaddr*)&cAddr, &cLen);
        if (clientS != INVALID_SOCKET) {
        DWORD timeout = 30 * 1000;             if (setsockopt(clientS, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
                printf("Gabim ne vendosjen e timeout!\n");
            }
        }
        if (clientS == INVALID_SOCKET) continue;

        int isAdmin = (active_count == 0) ? 1 : 0;
        log_client(cAddr, isAdmin);

        
        char welcome[128];
        sprintf(welcome, "\n*** LIDHUR ***\nStatusi: %s\n", isAdmin ? "ADMIN" : "USER");
        send(clientS, welcome, (int)strlen(welcome), 0);

        
        while (1) {
            char buf[1024] = { 0 };
            int n = recv(clientS, buf, sizeof(buf) - 1, 0); 

            if (n > 0) {
                buf[n] = '\0';
                save_log(inet_ntoa(cAddr.sin_addr), buf);
        for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_active && clients[i].port == ntohs(cAddr.sin_port)) {
            clients[i].msg_count++;
            break;
        }
    }
    fflush(stdout); 

    if (strcmp(buf, "/exit") == 0) break;
    handle_command(clientS, buf, isAdmin, cAddr);
}
            else {
                break; 
            }
        }

        printf("Lidhja u mbyll me [%s]\n", inet_ntoa(cAddr.sin_addr));
        closesocket(clientS);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].is_active && clients[i].port == ntohs(cAddr.sin_port)) {
                clients[i].is_active = 0;
                active_count--;
                break;
            }
        }
    }

    WSACleanup();
    return 0;
}