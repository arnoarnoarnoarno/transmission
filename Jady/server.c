/*
 Simple udp server
 http://www.binarytides.com/programming-udp-sockets-c-linux/
 */

#include "useful.h"

int main(void) {
	struct sockaddr_in si_me; //server struct addr
	struct sockaddr_in si_other; //client struct addr
	int s; //socket
	int slen = sizeof(si_other);
	char *buf = (char*) malloc(sizeof(char) * BUFLEN); //recu
	char *message = (char*) malloc(sizeof(char) * FRAGLEN); //envoyé
	int i; //filling the temporary little buffer
	int c; //useful to get the char in the file
	int PORT2 = PORT;
	int pid_fils = -1;
	int compare = 1; // il y a une difference
	FILE * f_in;
	int id_frag = 0, lastACK = 0;
	int read = FRAGLEN, sent = 0, recv = 0;
	char * ENTREE = (char*) malloc(sizeof(char) * FRAGLEN); //what server is sending

	struct timeval end, start, beginread;
	float RTT = 0, counting = 0, previous, display = 0, displayer = 0;
	char *id_frag_temp = (char*) malloc(sizeof(char) * 7);
	char *res = (char*) malloc(sizeof(char) * FRAGLEN + 7); //what's finally sent (id_frag + data)

	int rwnd = 1; //receiver window
	int cwnd = 10; //congestion window
	int flightSize = 0; //data that has been sent, but not yet acknowledged

	int j = 0;
	//
	if (create_server(&s, &si_me, PORT) != 0) {
		die("Error on creating first server\n");
	}
	printf("I am %d \n", getpid());
	//we clean up the buffer
	delete(buf, sizeof(buf));

	//keep listening for data
	while (1) {
		delete(buf, strlen(buf));
		receive_message(s, buf, (struct sockaddr *) &si_other, &slen);
		printf("Received by %d : %s\n", getpid(), buf);
		if (compare != 0) { //acceptation de connexion
			compare = strcmp("SYN", buf);
			if (compare == 0) {
				PORT2++;

				sprintf(message, "SYN-ACK%d", PORT2);

				send_message(s, message, 15, (struct sockaddr *) &si_other,
						slen);
				pid_fils = fork();

				if (pid_fils == 0) {
					printf("FILS %d de port %d\n", getpid(), PORT2);

					//On recrée un nouveau socket
					if (create_server(&s, &si_me, PORT2) != 0) {
						die("Error on creating a son server\n");
					}
				} else {
					printf("PERE %d de port %d\n", getpid(), PORT);
					compare = 1;
					continue;
				}
			} else {
				compare = 1;
			}
		} else { //une fois la connexion acceptée
			ENTREE = buf;
			if (access(ENTREE, 0) == 0) {
				//sends a "OK" to say that the
				//send_message(s,"OK",2,(struct sockaddr *)&si_other, slen);

				printf("\nEnvoi du fichier %s par %d...\n", ENTREE, getpid());

				if ((f_in = fopen(ENTREE, "rb")) == NULL) {	//b because nbinary
					die("Probleme ouverture fichier\n");
				}

				gettimeofday(&beginread, NULL);

				while (1) {
					if (flightSize < cwnd) {

						read = fread(message, 1, FRAGLEN, f_in);
						id_frag++;
						//printf("same id_frag %d \n", read, id_frag);
						//printf("\nRead: %d\n",read);
						if (read > 1) {
							gettimeofday(&start, NULL);

							sprintf(res, "%0.6d%s\n", id_frag, message);//the 6 first digits are for the id_frag
							sent = send_message(s, res, read,
									(struct sockaddr *) &si_other, slen);
							//printf("Sent: %d by %d\n", sent, getpid());
							flightSize++;
						} else {

							break;
						}
					} else {
						recv = rcv_msg_timeout(s, buf,
								(struct sockaddr *) &si_other, &slen);
						printf("Recv: %s de taille %d\n", buf, recv);
						flightSize = 0;

						lastACK = atoi(index(buf, 'K') + 1);
						printf("lastACK: %d\n", lastACK);

						gettimeofday(&end, NULL);
						RTT = ((end.tv_sec - start.tv_sec) * 1000.0f
								+ (end.tv_usec - start.tv_usec) / 1000.0f)
								/ 1000.0f;
						counting = ((end.tv_sec - beginread.tv_sec) * 1000.0f
								+ (end.tv_usec - beginread.tv_usec) / 1000.0f)
								/ 1000.0f;

						//display every second BUT increases RTT (x2 or x3) -> need another independant process
						if (recv > 0 && (previous != (int) counting % read)) {
							display = (int) counting % read;
							printf("RTT: %f secondes\n", RTT);
							previous = display;
						}
					}
				}

				printf("Fichier de taille %d transmis avec succès\n\n",
						ftell(f_in));
				fclose(f_in);
				send_message(s, "FIN", 3, (struct sockaddr *) &si_other, slen);
				delete(buf, strlen(buf));
			} else {
				if (strcmp(" ", ENTREE) != 0 && strcmp("", ENTREE) != 0) {
					send_message(s, "FIN", 3, (struct sockaddr *) &si_other,
							slen);
					printf("Le fichier '%s' n'existe pas.\n", ENTREE);
					delete(buf, strlen(buf));

				} else {
					printf("Pourquoi envoyer du vide?\n");
				}
			}
		}
	}
	close(s);

}

