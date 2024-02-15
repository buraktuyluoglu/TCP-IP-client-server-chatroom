#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#define MAX_CLIENTS 10
#include <stdint.h>

// her bir biti sağa kaydırma ve belirli bir polinom ile XOR işlemini yapacak.
uint32_t crc32(const void *data, size_t n_bytes) {
    uint32_t crc = 0xffffffff;
    const uint8_t *p = data;
    for (size_t i = 0; i < n_bytes; ++i) {
        crc ^= p[i]; //Veriyi XOR'la
        for (uint32_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc; //Hesaplanan CRC'yi döndür (bitleri ters çevirilmiş olarak)
}

unsigned int calculate_checksum(char *message) {
    unsigned int checksum = 0;
    for(int i = 0; i < strlen(message); i++) {
        checksum += message[i];
    }
    return checksum;
}

typedef struct {
    SOCKET socket;
    char name[50];
} Client;

Client clients[MAX_CLIENTS] = {0};

//log kayıdı tutar
void logMessage(const char* message) {
    FILE* logFile = fopen("server.log", "a");
    if(logFile == NULL) {
        printf("Error opening log file!\n");
        return;
    }

    time_t now = time(NULL);
    char* timeStr = ctime(&now);
    timeStr[strlen(timeStr) - 1] = '\0';  

    fprintf(logFile, "[%s] %s\n", timeStr, message);

    fclose(logFile);
}

int isMessageCorrupt(char* message) {
    // Basit bir mesaj uzunlğu kontrolü
    if(strlen(message) > 4096) {
        return 1;  // Message çok uzun
    }

    return 0;  // Message sıkıntı yok
}


DWORD WINAPI HandleClient(void* data) {
    Client* client = (Client*)data;
    char buffer[4096] = {0};
    
    // İstemcinin ismini al
    int read = recv(client->socket, client->name, sizeof(client->name) - 1, 0);
    if(read <= 0) {
        printf("Failed to get client name.\n");
        closesocket(client->socket);
        return 0;
    }
    printf("Client joined: %s\n", client->name);
    sprintf(buffer, "%s has joined the chat.", client->name);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket != INVALID_SOCKET) {
            send(clients[i].socket, buffer, strlen(buffer), 0);
        }
    }

    while(1) {
        read = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        if(read <= 0) {
            // Sunucudan ayrılmayı logla
            char logMessageBuffer[4096];
            sprintf(logMessageBuffer, "Client disconnected: %s", client->name);
            logMessage(logMessageBuffer);

            // Sunucudan ayrıldığını sunucuya yaz
            printf("%s\n", logMessageBuffer);

            // Sunucudan ayrıldığının bilgisini kullanıcılarına bildir
            sprintf(buffer, "%s has left the chat.", client->name);
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != INVALID_SOCKET && clients[i].socket != client->socket) {
                    send(clients[i].socket, buffer, strlen(buffer), 0);
                }
            }

            closesocket(client->socket);
            client->socket = INVALID_SOCKET;  // İstemcinin socket numarasını boşa çıkartır
            return 0;
        }

        buffer[read] = '\0';
        printf("buffer: %s\n", buffer);
        char *separator1 = strchr(buffer, '|');
        if(separator1 == NULL) {
            printf("Invalid message format.\n");
            continue;
        }

        *separator1 = '\0';
        uint32_t received_crc = atoi(separator1 + 1);

        char *separator2 = strchr(separator1 + 1, '|');
        if(separator2 == NULL) {
            printf("Invalid message format.\n");
            continue;
        }

        *separator2 = '\0';
        unsigned int received_checksum = atoi(separator2 + 1);

        uint32_t calculated_crc = crc32(buffer, strlen(buffer));
        unsigned int calculated_checksum = calculate_checksum(buffer);

        if(received_crc != calculated_crc) {
            printf("CRC mismatch.\n");
            continue;
        }

        if(received_checksum != calculated_checksum) {
            printf("Checksum mismatch.\n");
            continue;
        }
        
        // Log the received message
        char logMessageBuffer[4096];
        sprintf(logMessageBuffer, "Received message from %s: %s", client->name, buffer);
        logMessage(logMessageBuffer);

        //Gelen mesaj listeleme komutu mu
        if(strcmp(buffer, "/userlist") == 0) {
            // Server'a bağlı client'ları listeler
            char userList[4096] = "Connected users:\n";
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != INVALID_SOCKET) {
                    strcat(userList, clients[i].name);
                    strcat(userList, "\n");
                }
            }

            // Komutu yazan client'a, kişileri listeler
            send(client->socket, userList, strlen(userList), 0);
        }

        // Private message kontrolü (Whisper)
        if(strncmp(buffer, "/w ", 3) == 0) {
            //strtok komutu ile hem mesajı hem de kime gideceğini ayırıyoruz ||PARSE MESSAGE
            char* username = strtok(buffer + 3, " ");
            char* message = strtok(NULL, "");

            if(username == NULL || message == NULL) {
                printf("Invalid whisper format.\n");
                continue;
            }

            // Mesajın gideceği kişiyi bul, mesajı ilet (MESG|alici|gönderici|mesaj)
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != INVALID_SOCKET && strcmp(clients[i].name, username) == 0) {
                    char formattedMessage[4150] = {0};
                    sprintf(formattedMessage, "%s whispers: %s", client->name, message);
                    send(clients[i].socket, formattedMessage, strlen(formattedMessage), 0);
                    break;
                }
            }
        } 
        // Herkese mesajı ilet
        else {
            printf("%s says: %s\n", client->name, buffer);
            for(int i = 0; i < MAX_CLIENTS; i++) {
                if(clients[i].socket != INVALID_SOCKET && clients[i].socket != client->socket) {
                    char message[4150] = {0};
                    sprintf(message, "%s says: %s", client->name, buffer);
                    send(clients[i].socket, message, strlen(message), 0);
                }
            }
        }
    }
    sprintf(buffer, "%s has left the chat.", client->name);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].socket != INVALID_SOCKET && clients[i].socket != client->socket) {
            send(clients[i].socket, buffer, strlen(buffer), 0);
        }
    }

    closesocket(client->socket);

    return 0;
}



int main() {
    WSADATA wsaData; // Winsock'ı başlatma
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed to load Winsock.\n");
        return -1;
    }
// Soket oluşturma
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == INVALID_SOCKET) {
        printf("Failed to create socket.\n");
        WSACleanup();
        return -1;
    }

 
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8080);

// Soketi belirli bir adres ve porta bağlama
    if(bind(serverSocket, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("Failed to bind.\n");
        WSACleanup();
        return -1;
    }
 // Soketi dinleme moduna aldı
    if(listen(serverSocket, 5) < 0) {
        printf("Failed to listen.\n");
        WSACleanup();
        return -1;
    }

    printf("Server is listening.\n");

// İstemci dizisini başlangıçta geçersiz soket değerleriyle doldur
    for(int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = INVALID_SOCKET;
    }

    while(1) { 
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if(clientSocket == INVALID_SOCKET) { // Yeni bir istemci bağlantısını kabul etme
            printf("Failed to accept.\n");
            continue;
        }

 // Boş bir konum bul ve yeni istemciyi ekleyip işleme başla
        for(int i = 0; i < MAX_CLIENTS; i++) {
            if(clients[i].socket == INVALID_SOCKET) {
                clients[i].socket = clientSocket;
                CreateThread(NULL, 0, HandleClient, &clients[i], 0, NULL);
                break;
            }
        }
    }

    closesocket(serverSocket); // Sunucu soketini kapat
    WSACleanup(); // Winsock'ı temizle

    return 0;
}