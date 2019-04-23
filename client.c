#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFLEN 256

int starts_with(char *a, char *b);
char* create_error_message (int error_number);
int parse_command(char *command, int aboutToSendConfirmation, int aboutToUnlock);

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int starts_with(char *a, char *b)
{
   return strncmp(a, b, strlen(b)) == 0;
}

int parse_command(char *command, int aboutToSendConfirmation, int aboutToUnlock)
{
    if (aboutToSendConfirmation)
    {
        return 6;
    }
    if (aboutToUnlock)
    {
        return 7;
    }
    if (starts_with(command, "login")) {
        return 0;
    }
    if (starts_with(command, "logout")) {
        return 1;
    }
    if (starts_with(command, "listsold")) {
        return 2;
    }
    if (starts_with(command, "transfer")) {
        return 3;
    }
    if (starts_with(command, "unlock")) {
        return 4;
    }
    if (starts_with(command, "quit")) {
        return 5;
    }
    return -1;
}

char* create_error_message (int error_number)
{
    switch (error_number)
    {
        case -1: 
                return "IBANK> -1 : Clientul nu este autentificat";
        case -2:
                return "IBANK> -2 : Sesiune deja deschisa";
        case -3:
                return "IBANK> -3 : Pin gresit";
        case -4:
                return "IBANK> -4 : Numar card inexistent";
        case -5:
                return "IBANK> -5 : Card blocat";
        case -6:
                return "IBANK> -6 : Operatie esuata";
        case -7:
                return "UNLOCK> -7 : Deblocare esuata";
        case -8:
                return "IBANK> -8 : Fonduri insuficiente";
        case -9:
                return "IBANK> -9 : Operatie anulata";
        case -10:
                return "IBANK> -10 : Eroare la apel functie de sistem";
        case -11:
                return "UNLOCK> -6 : Operatie esuata";
    }
}

int main(int argc, char *argv[])
{
    int loggedIn = 0;
    int aboutToSendConfirmation = 0;
    int aboutToUnlock = 0;

    char log_file_name[100];
    char last_login[100];
    sprintf(log_file_name, "client-%li.log", (long) getpid());
    FILE *log_file = fopen (log_file_name, "w");
    int sockfd, udp_sockfd;
    struct sockaddr_in serv_addr;

    fd_set read_fds;   //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar 
    int fdmax;     //valoare maxima file descriptor din multimea read_fds

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    char buffer[BUFLEN];
    
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    udp_sockfd = socket (PF_INET, SOCK_DGRAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    connect (sockfd,(struct sockaddr*) &serv_addr,sizeof (serv_addr));

    FD_SET(0, &read_fds);
    FD_SET(sockfd, &read_fds);
    FD_SET (udp_sockfd, &read_fds);

    fdmax = udp_sockfd;

    while(1)
    {
        memset (buffer, 0, BUFLEN);
        tmp_fds = read_fds;
        select (fdmax + 1, &tmp_fds, NULL, NULL, NULL);

        for (int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET (i, &tmp_fds) && i == sockfd)
            {
                close (sockfd);
                fclose (log_file);
                return 0;
            }
            if (FD_ISSET (i, &tmp_fds) && i == 0)
            {
                memset(buffer, 0, BUFLEN);
                fgets(buffer, BUFLEN - 1, stdin);
                fprintf (log_file, "%s", buffer);
                switch (parse_command(buffer, aboutToSendConfirmation, aboutToUnlock))
                {
                    case 0:
                        strncpy (last_login, buffer + 6, 6);
                        last_login[6] = 0;
                        if (loggedIn == 1) {
                            strcpy (buffer, create_error_message (-2));
                        }
                        else {  
                            send(sockfd, buffer, strlen(buffer), 0);
                            memset(buffer, 0, BUFLEN);
                            recv(sockfd, buffer, sizeof(buffer), 0);

                            if (starts_with (buffer, "IBANK> Welcome"))
                            {
                                loggedIn = 1;
                            }
                        }
                        break;
                    case 1:
                        if (loggedIn == 0)
                        {
                            strcpy (buffer, create_error_message (-1));   
                        }
                        else
                        {
                            send (sockfd, buffer, strlen(buffer), 0);
                            memset (buffer, 0, BUFLEN);
                            recv (sockfd, buffer, sizeof (buffer), 0);
                            loggedIn = 0;
                        }
                        break;
                    case 2:
                        if (loggedIn == 0)
                        {
                            strcpy (buffer, create_error_message (-1));
                        }
                        else
                        {
                            send (sockfd, buffer, strlen (buffer), 0);
                            memset (buffer, 0, BUFLEN);
                            recv (sockfd, buffer, sizeof (buffer), 0);
                        }
                        break;
                    case 3:
                        if (loggedIn == 0)
                        {
                            strcpy (buffer, create_error_message (-1));
                        }
                        else
                        {
                            aboutToSendConfirmation = 1;
                            send (sockfd, buffer, strlen (buffer), 0);
                            memset (buffer, 0, BUFLEN);
                            recv (sockfd, buffer, sizeof (buffer), 0);
                        }
                        break;
                    case 4:
                        if (loggedIn == 0)
                        {
                            strcpy (buffer, create_error_message (-1));
                        }
                        else
                        {
                            char temp_buffer[100];

                            buffer[strlen (buffer) - 1] = 0;
                            aboutToUnlock = 1;
                            sprintf (temp_buffer,"%s %s", buffer, last_login);
                            sendto (udp_sockfd,
                                    temp_buffer,
                                    strlen (temp_buffer) + 1,
                                    0,
                                    (struct sockaddr*) &serv_addr,
                                    sizeof (struct sockaddr_in));
                            memset (buffer, 0, BUFLEN);
                            recvfrom (udp_sockfd,
                                      buffer,
                                      BUFLEN,
                                      0,
                                      (struct sockaddr *) &serv_addr,
                                      (socklen_t *) sizeof (struct sockaddr_in));
                        }
                        break;
                    case 5:
                        fclose (log_file);
                        return 0;
                    case 6:
                        aboutToSendConfirmation = 0;
                        send (sockfd, buffer, strlen (buffer), 0);
                        memset (buffer, 0, BUFLEN);
                        recv (sockfd, buffer, sizeof (buffer), 0);
                        break;
                    case 7:
                        aboutToUnlock = 0;
                        send (sockfd, buffer, strlen (buffer), 0);
                        memset (buffer, 0, BUFLEN);
                        recv (sockfd, buffer, sizeof (buffer), 0);
                        break;
                }
                if (starts_with (buffer, "IBANK> -") ||
                    starts_with (buffer, "UNLOCK> -"))
                {
                    printf("%s\n", buffer);
                }
                fprintf(log_file, "%s\n\n", buffer);
            }
        }
    }
    fclose(log_file);
    return 0;
}


