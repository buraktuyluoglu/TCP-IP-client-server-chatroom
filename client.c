#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

uint32_t crc32(const void *data, size_t n_bytes) {
    uint32_t crc = 0xffffffff;
    const uint8_t *p = data;
    for (size_t i = 0; i < n_bytes; ++i) {
        crc ^= p[i];
        for (uint32_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

unsigned int calculate_checksum(char *message) {
    unsigned int checksum = 0;
    for(int i = 0; i < strlen(message); i++) {
        checksum += message[i];
    }
    return checksum;
}

DWORD WINAPI ReceiveMessages(void* data) {
    SOCKET clientSocket = *(SOCKET*)data;
    char buffer[4096] = {0};

    while(1) {
        int read = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if(read <= 0) {
            printf("Server disconnected.\n");
            break;
        }

        buffer[read] = '\0';
        printf("\n%s\nEnter message: ", buffer);
        fflush(stdout);  // Make sure "Enter message: " is printed immediately
    }

    return 0;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed to load Winsock.\n");
        return -1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSocket == INVALID_SOCKET) {
        printf("Failed to create socket.\n");
        WSACleanup();
        return -1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(8080);

    if(connect(clientSocket, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("Failed to connect.\n");
        WSACleanup();
        return -1;
    }

    printf("Connected to server.\n");

    char buffer[4096] = {0};
    printf("Enter username: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;  // enter yani alt satırı sil

    // Mesajdan önce isimi gönder
    send(clientSocket, buffer, strlen(buffer), 0);

    //Mesajları asenkron olarak alacak yeni bir thread başlatılır.
    CreateThread(NULL, 0, ReceiveMessages, &clientSocket, 0, NULL);

    while(1) {
    printf("Enter message: ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;  // Remove newline

    // Calculate the CRC of the message
    uint32_t crc = crc32(buffer, strlen(buffer));
    unsigned int checksum = calculate_checksum(buffer);
    // Create a new buffer to hold the message and the CRC
    char crcBuffer[4096] = {0};
    sprintf(crcBuffer, "%s|%u|%u", buffer, crc, checksum);

    // Send the message with the CRC
    send(clientSocket, crcBuffer, strlen(crcBuffer), 0);
}

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}