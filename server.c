#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS	20
#define BUFLEN 256

typedef struct user_data {
    char nume[13];
    char prenume[13];
    char numar_card[7];
    char pin[5];
    char parola_secreta[9];
    double sold;
} user_data;

void parse_users_data_file(user_data user_data_list[], int *number_of_users, char* file_name);
int starts_with(const char *a, const char *b);
int parse_command(char *command, double transfer_sum[], int user_index_by_sockfd);
void create_reply_message (char* reply_message,
                           int reply_message_type,
                           user_data user_data_list[],
                           int number_of_users,
                           char *numar_card,
                           double to_transfer);
void print_numar_card_list (user_data user_data_list[], int number_of_users);
int get_user_index (char *numar_card, user_data user_data_list[], int number_of_users);
void run_command (char *reply_message,
                   char *command,
                   user_data user_data_list[],
                   int number_of_users,
                   int logged_users[],
                   int failed_login_attempts_users[],
                   int logged_users_sockfd[],
                   int sockfd,
                   int transfer_target[],
                   double transfer_sum[],
                   int about_to_unlock[]);

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void parse_users_data_file(user_data user_data_list[], int *number_of_users, char* file_name)
{
    // Functie care parseaza fisierul cu datele utilizatorilor.
    FILE *user_data_file = fopen (file_name, "r");

    fscanf (user_data_file, "%i", number_of_users);
    for (int i = 0; i < *number_of_users; i++) {
        fscanf(user_data_file, "%s %s %s %s %s %lf", user_data_list[i].nume,
                                                     user_data_list[i].prenume,
                                                     user_data_list[i].numar_card,
                                                     user_data_list[i].pin,
                                                     user_data_list[i].parola_secreta,
                                                     &user_data_list[i].sold);
    }
    fclose (user_data_file);
}

int starts_with(const char *a, const char *b)
{
   return strncmp(a, b, strlen(b)) == 0;
}

int parse_command(char *command,
                  double transfer_sum[],
                  int user_index_by_sockfd)
{
    //Functie care ne va spune ce comanda sa executam.
    if (transfer_sum[user_index_by_sockfd])
    {
        return 6;
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
    // Functie care genereaza cateva mesaje de eroare generale.
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

void create_reply_message (char* reply_message,
                           int reply_message_type,
                           user_data user_data_list[],
                           int number_of_users,
                           char *numar_card,
                           double to_transfer)
{
    // Functie care ne creeaza mesaje de reply.
    int user_index = get_user_index (numar_card, user_data_list, number_of_users);

    switch (reply_message_type)
    {
        case 0:
            sprintf (reply_message, "IBANK> Welcome %s %s", user_data_list[user_index].nume,
                                                            user_data_list[user_index].prenume);
            return;

        case 1:
            sprintf (reply_message, "Clientul a fost deconectat");
            return;

        case 2:
            sprintf (reply_message, "listsold %.02lf", user_data_list[user_index].sold);
            return;

        case 3:
            sprintf (reply_message, "Transfer %.02lf catre %s %s? [y/n]", to_transfer,
                                                                          user_data_list[user_index].nume,
                                                                          user_data_list[user_index].prenume);
            return;

        case 4:
            sprintf (reply_message, "Transfer realizat cu succes");
            return;

        case 5:
            sprintf (reply_message, "Trimite parola secreta");
            return;

    }
}

int get_user_index (char *numar_card, user_data user_data_list[], int number_of_users)
{
    // Functie care ne intoarce index-ul unui user dupa numarul cardului.
    for (int i = 0; i < number_of_users; i++)
    {
        if (strcmp(numar_card, user_data_list[i].numar_card) == 0)
        {
            return i;
        }
    }
    return -1;
}

int get_user_index_by_sockfd (int logged_users_sockfd[], int number_of_users, int sockfd)
{
    // Functie care ne intoarce index-ul unui user dupa socket-ul pe care a fost
    // trimis mesajul.
    for (int i = 0; i < number_of_users; i++)
    {
        if (logged_users_sockfd[i] == sockfd)
        {
            return i;
        }
    }
    return -1;
}

void run_command (char *reply_message,
                   char *command,
                   user_data user_data_list[],
                   int number_of_users,
                   int logged_users[],
                   int failed_login_attempts_users[],
                   int logged_users_sockfd[],
                   int sockfd,
                   int transfer_target[],
                   double transfer_sum[],
                   int about_to_unlock[])
{
    // Functia care efectueaza comanda primita.
    char numar_card[8];
    char pin[5];
    int user_index;
    int user_index_by_sockfd;
    double to_transfer;
    char secret_pass[9];

    switch (parse_command(command,
                          transfer_sum,
                          get_user_index_by_sockfd(logged_users_sockfd,
                                                   number_of_users,
                                                   sockfd)))
    {
        // Login
        case 0:
            strncpy(numar_card, command + 6, 6);
            numar_card[6] = 0;
            strncpy(pin, command + 13, 4);
            pin[4] = 0;
            user_index = get_user_index (numar_card, user_data_list, number_of_users);

            // Card inexistent
            if (user_index == -1)
            {
                strcpy (reply_message, create_error_message (-4));
                return;
            }

            // Parola gresita
            if (strcmp (user_data_list[user_index].pin, pin) != 0)
            {
                failed_login_attempts_users[user_index]++;
                strcpy (reply_message, create_error_message (-3));
                return;
            }

            // Card blocat
            if (failed_login_attempts_users[user_index] >= 3)
            {
                strcpy (reply_message, create_error_message (-5));
                return;
            }

            // Logare cu succes
            if (logged_users[user_index] == 1)
            {
                strcpy (reply_message, create_error_message (-2));
                return;
            }
            // Salvam ultima logare pentru unlock:
            logged_users[user_index] = 1;
            logged_users_sockfd[user_index] = sockfd;
            failed_login_attempts_users[user_index] = 0;
            create_reply_message (reply_message, 
                                  0,
                                  user_data_list,
                                  number_of_users,
                                  numar_card,
                                  0);
            return;
        // Logout
        case 1:
            user_index = get_user_index_by_sockfd (logged_users_sockfd,
                                                   number_of_users,
                                                   sockfd);
            logged_users[user_index] = 0;
            create_reply_message (reply_message, 1, NULL, 0, NULL, 0);
            return;

        case 2:
        // Listsold
            user_index = get_user_index_by_sockfd (logged_users_sockfd,
                                                   number_of_users,
                                                   sockfd);
            create_reply_message (reply_message,
                                  2,
                                  user_data_list, 
                                  number_of_users,
                                  user_data_list[user_index].numar_card,
                                  0);
            return;
        // Transfer
        case 3:
            strncpy (numar_card, command + 9, 6);
            numar_card[6] = 0;
            user_index = get_user_index (numar_card, user_data_list, number_of_users);
            user_index_by_sockfd = get_user_index_by_sockfd (logged_users_sockfd,
                                                             number_of_users,
                                                             sockfd);
            // Card inexistent
            if (user_index == -1)
            {
                strcpy (reply_message, create_error_message (-4));
                return;
            }
            // Fonduri insuficiente
            sscanf (command + 16, "%lf", &to_transfer);
            if (to_transfer > user_data_list[user_index_by_sockfd].sold)
            {
                strcpy (reply_message, create_error_message (-8));
                return;
            }
            // Tranzactie reusita, updatam conturile.
            else
            {
                create_reply_message (reply_message,
                                      3,
                                      user_data_list,
                                      number_of_users,
                                      user_data_list[user_index_by_sockfd].numar_card,
                                      to_transfer);
                transfer_target[user_index_by_sockfd] = user_index;
                transfer_sum[user_index_by_sockfd] = to_transfer;
                return;
            }
        // Unlock, cerem parola secreta:
        case 4:
            strncpy (numar_card, command + 8, 6);
            numar_card[6] = 0;
            user_index = get_user_index (numar_card, user_data_list, number_of_users);
            if (failed_login_attempts_users[user_index] < 3)
            {
                memset (reply_message, 0, 100);
                strcpy (reply_message, create_error_message (-11));
                return;
            }
            create_reply_message (reply_message,
                                  5,
                                  NULL,
                                  0,
                                  NULL,
                                  0);
            about_to_unlock[user_index] = 1;
            return;
        // Confirmarea transferului bancar (sau anularea sa)
        case 6:            
            user_index_by_sockfd = get_user_index_by_sockfd (logged_users_sockfd,
                                                             number_of_users,
                                                             sockfd);
            if (strcmp (command, "y\n") == 0)
            {
                create_reply_message (reply_message,
                                      4,
                                      user_data_list,
                                      0,
                                      NULL,
                                      0);
                user_data_list[user_index_by_sockfd].sold -= transfer_sum[user_index_by_sockfd];
                user_data_list[transfer_target[user_index_by_sockfd]].sold += transfer_sum[user_index_by_sockfd];
            }
            else
            {
                strcpy (reply_message, create_error_message (-9));
            }
            transfer_target[user_index_by_sockfd] = 0;
            transfer_sum[user_index_by_sockfd] = 0;
            return;
        // Preluarea parolei secrete
        // Incomplet pentru ca nu am reusit sa il fac sa functioneze :()
        default:
            strcpy (secret_pass, command);
            secret_pass[strlen (secret_pass) - 1] = 0;
            return;
    }
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, udp_sockfd, portno, clilen;
    char buffer[BUFLEN];
    char server_reply[BUFLEN];
    struct sockaddr_in serv_addr, cli_addr;
    int n, i, j;
    int is_unlock;
    int number_of_users;
    user_data user_data_list[1000];
    int logged_users[1000];
    int logged_users_sockfd[1000];
    int failed_login_attempts_users[1000];
    int transfer_target[1000];
    double transfer_sum[1000];
    int about_to_unlock[1000];

    fd_set read_fds;   //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporaint logged_users[1000];r 
    int fdmax;		//valoare maxima file descriptor din multimea read_fds

    //golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
     
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    udp_sockfd = socket (PF_INET, SOCK_DGRAM, 0);
     
    portno = atoi(argv[1]);

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
    serv_addr.sin_port = htons(portno);
     
    bind (sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    bind (udp_sockfd, (struct sockaddr*) &serv_addr, sizeof (struct sockaddr));

    listen(sockfd, MAX_CLIENTS);

    //adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
    FD_SET (0, &read_fds);
    FD_SET (sockfd, &read_fds);
    FD_SET (udp_sockfd, &read_fds);
    fdmax = udp_sockfd;

    parse_users_data_file(user_data_list, &number_of_users, argv[2]);

    // main loop
	while (1) {
		tmp_fds = read_fds; 
		select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET (i, &tmp_fds) && i == 0)
            {
                for (int i = 3; i <= fdmax; i++)
                {
                    send (i, "quit", strlen ("quit"), 0);
                    close (i);
                    return 0;
                }
            }
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
					// actiunea serverului: accept()
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
						error("ERROR in accept");
					}
					else {
						//adaug noul socket intors de accept() la multimea descriptorilor de citire
						FD_SET(newsockfd, &read_fds);
                        connect(newsockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr));
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
				}

                else if (i == udp_sockfd)
                {
                    recvfrom (udp_sockfd,
                              buffer,
                              BUFLEN,
                              0,
                              (struct sockaddr *) &serv_addr,
                              (socklen_t *) sizeof (struct sockaddr_in));
                    run_command (server_reply,
                                 buffer,
                                 user_data_list,
                                 number_of_users,
                                 logged_users,
                                 failed_login_attempts_users,
                                 logged_users_sockfd,
                                 i,
                                 transfer_target,
                                 transfer_sum,
                                 about_to_unlock);
                    sendto(i,
                           server_reply,
                           strlen(server_reply) + 1,
                           0,
                           (struct sockaddr *) &serv_addr,
                           sizeof (struct sockaddr_in));
                }

				else {
					// am primit date pe unul din socketii cu care vorbesc cu clientii
					//actiunea serverului: recv()
					memset(buffer, 0, BUFLEN);
					if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						// if (n == 0) {
						// 	//conexiunea s-a inchis
						// 	printf("server: socket %d hung up\n", i);
						// }
						close(i); 
						FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care 
					}
					
					else { //recv intoarce >0
                        run_command (server_reply,
                                     buffer,
                                     user_data_list,
                                     number_of_users,
                                     logged_users,
                                     failed_login_attempts_users,
                                     logged_users_sockfd,
                                     i,
                                     transfer_target,
                                     transfer_sum,
                                     about_to_unlock);
                        printf("Server_reply: %s\n", server_reply);
                        send(i, server_reply, strlen(server_reply), 0);
					}
				}
			}
		}
    }


     close(sockfd);
   
     return 0; 
}


