#include <stdio.h> 
#include <string.h> 
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
    int socketNr = socket(AF_INET, SOCK_STREAM, 0);
    if(socketNr < 0)
    {
        perror("Nu s-a creat socket-ul");
        return 1;
    }

    struct sockaddr_in socketAddr;
    memset(&socketAddr, 0, sizeof(socketAddr));
    socketAddr.sin_family = AF_INET;
    socketAddr.sin_port = htons(5000);
    socketAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(socketNr, (struct sockaddr *)&socketAddr, sizeof(socketAddr)) < 0)
    {
        perror("Eroare la legarea socket-ului de adresa si port");
        return 1;
    }
    else
        printf("Socket legat de portul 5000\n");

    if(listen(socketNr, 1) < 0)
    {
        perror("Eroare la trecerea in modul de asteptare a conexiunii");
        return 1;
    }

    while(1)
    {
        int socketConnection = accept(socketNr, NULL, NULL);
        if(socketConnection < 0)
        {
            perror("Eroare la conectare");
            continue;
        }
        else
            printf("Client conectat\n");

        while(1)
        {
            char data[65536];
            ssize_t octetiPrimiti = recv(socketConnection, data, sizeof(data), 0);
            if(octetiPrimiti <= 0)
            {
                if(octetiPrimiti < 0)
                {
                    perror("Eroare");
                }
                else
                {
                    printf("Clientul a inchis conexiunea\n");
                }
                break;
            }
            else
            {
                printf("S-au primit %zd octeti\n", octetiPrimiti);
            }

            ssize_t trimisi = 0;

            while(trimisi < octetiPrimiti)
            {
                ssize_t octetiTrimisi = send(socketConnection, data + trimisi, octetiPrimiti - trimisi, 0);
                if(octetiTrimisi < 0)
                {
                    perror("Eroare la trimiterea mesajului");
                    break;
                }

                trimisi += octetiTrimisi;
            }
        }
        close(socketConnection);
        printf("Client deconectat\n");
    }
    close(socketNr);
    return 0;

}