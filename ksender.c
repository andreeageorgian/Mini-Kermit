#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

//functie pentru retrimiterea mesajului in caz de timeout pentru
//pachetele de tip 'S'
msg* resesend_message_S(msg* k, minikermits package_s){
	msg t;
	int count = 0;

	//cat timp primim ca raspuns NAK-uri
	while(k->payload[3] == 'N'){
		//actualizam campurile pachetului S si payloadul
        package_s.seq = k->payload[2] + 1;

        memcpy(t.payload, &package_s, 4);
    	memcpy(t.payload + 4, &package_s.data, 11);
    	memcpy(t.payload + 15, &package_s.check, 2);
    	memcpy(t.payload + 17, &package_s.mark, 1);
    	t.len = sizeof(package_s);
 
        package_s.check = crc16_ccitt(t.payload, t.len - 4);

        memcpy(t.payload + 15, &package_s.check, 2);
    	t.len = sizeof(package_s);

    	//trimitem pachetul
        send_message(&t);

        //asteptam raspuns
        k = receive_message_timeout(5000);

        //daca raspunsul nu a fost primit => TIEMOUT si iesim
        if(k == NULL){
            printf("TIMEOUT!\n");
            return NULL;
        }
        //daca primim un pachet Y, iesim din functie
        if( k -> payload[3] == 'Y'){
            printf("[./ksender] Got ACK!\n");
            return NULL;
        }
	}
}

//functie de retrimitere a pachetelor care nu sunt de tip S
void resesend_message_kermite(msg* k, minikermit package){
	msg t;

	//cat timp confirmarea este negativa
	while(k->payload[3] == 'N'){
		//actualizam pachetul si payload-ul
        package.seq = k->payload[2] + 1;
    	t = send_F(package);
        package.check = crc16_ccitt(&package, package.len - 2);
        memcpy(t.payload + package.len, &package.check, 2);
    	t.len = package.len + 2;

    	//il trimitem
        send_message(&t);

        //asteptam raspunsul
        k = receive_message_timeout(5000);

        //daca raspunsul nu a venit => TIMEOUT
        if(k == NULL){
            printf("TIMEOUT!\n");
        	return;
        }
        
        //daca raspunsul este pozitiv, iesim din functie
        if( k -> payload[3] == 'Y'){
            printf("[./ksender] Got ACK!.\n");
            return;
        }
	}
}

//functie folosita pentru confirmarea raspunsului
int confirm_message(msg* k){
	if(k == NULL){
    	return -1;
    }
    else if(k->payload[3] == 'N'){
    	return 0;
    }
    else if(k->payload[3] == 'Y'){
    	return 1;
    }
}

int main(int argc, char** argv) {
    msg t;


    init(HOST, PORT);
    unsigned short crc;

    //initializam un pachet de tip 'S'
    minikermits package_s = send_init();

    //setam payoad-ul corespunzator
    memcpy(t.payload, &package_s, 4);
    memcpy(t.payload + 4, &package_s.data, 11);
    memcpy(t.payload + 15, &package_s.check, 2);
    memcpy(t.payload + 17, &package_s.mark, 1);
    t.len = sizeof(package_s);
    send_message(&t);

    int count = 0;
    msg* k;

    //asteptam raspunsul de la receiver
    k = receive_message_timeout(5000);

    //daca este gol => TIMEOUT
    //ar trebui retrimis mesajul, dar nu
    //functioneaza
    if(k == NULL){
    	printf("TIMEOUT!\n");
    	return 0;
    	//resesend_message_S(k, package_s);
    }  

    //daca am primit NAK ar trebui sa retrimitem mesajul
    //dar la mine nu funtioneaza
    if(k->payload[3] == 'N'){
    	//resesend_message_S(k, package_s);
    	printf("GOT NAK!\n");
    	return 0;
    }

    //daca am primit ACK trecem mai departe
    if(k->payload[3] == 'Y'){
        printf("[%s] Got ACK.\n", argv[0]);
    }

    //incepem sa trimitem fisiere
    //Trimitem numarul de fisiere
    int i = 1;
    minikermit package_f, package_z, package_b;
    int files_number = argc - 1;
    sprintf(t.payload,"%d",files_number);
    t.len = sizeof(files_number) + 1;
    send_message(&t);

    //verificam daca numarul de fisiere a ajuns
    //la receiver
    if(recv_message(&t) < 0){
        perror("EROARE NUMAR FISIERE!");
        return -1;
    }

    //daca numarul de fisiere a fost receptionat corect 
    //incepem sa trimitem datele
    while(files_number >= 1){

    	//instantiem un pachet de tip F
        package_f = F(argv[i], k->payload[2]);
        t = send_F(package_f);

        //il trimitem receiver-ului
        send_message(&t);

        //asteptam raspunsul
        k = receive_message_timeout(5000);

        //daca nu primim raspuns=> TIMEOUT
        if(k == NULL){
            printf("TIMEOUT!");
            break;
        }

        //daca primim raspuns negativ, ar trebui sa 
        //retrimitem mesajul dar functia nu-mi merge
        // asa ca voi trece la fisierul urmator
        if(k->payload[3] == 'N'){
            //resesend_message_kermite(k, package_f);
        	printf("GOT NAK!\n");
        	files_number = files_number - 1;
        	i = i + 1;
        	break;
        }

        //daca primim raspuns pozitiv, incepem sa 
        //transmitem datele din fisier
        if(k->payload[3] == 'Y'){
           	printf("[%s] Got ACK!\n", argv[0]);

           	//file descriptorul corespunzator fisierului
           	//curent
           	int fd = open(argv[i],O_RDONLY);
            struct stat file_struct;
  			fstat(fd,&file_struct);

  			//extragem numele fisierului din pachetul F
  			//nu il mai trimit catre receiver pentru ca am 
  			//trimis si confirmat deja pachetul F
 	 		msg file_name;
 	 		memcpy(file_name.payload, &package_f.data, package_f.len);
 	 		file_name.len = package_f.len;

 	 		//extragem marimea fisierului
  			msg file_size;
  			sprintf(file_size.payload,"%ld",file_struct.st_size);
  			file_name.len = sizeof(file_struct.st_size) + 1;

  			//o trimitem receiver-ului
  			send_message(&file_size);

   			if(recv_message(&file_size) < 0){
    			perror("EROARE MARIME FISIER!\n");
  			}

  			//daca dimensiunea a fost primita cu succes de catre
  			//receiver, incepem sa scirem efectiv date
  			minikermit package_d;
  			
  			//payload in care vom scrie datele
  			msg block;

  			//dimensiunea unui bloc de date
  			int dim_block;

  			//dimensiunea totala a fisierului
  			int dimensiune = file_struct.st_size;
  			
  			while(dimensiune > 0){
  				memset(block.payload, 0 , sizeof(package_d.data));

  				//citim in payload maxim 250 de bytes pe care ii memoram 
  				//iin block, iar numarul de bytes cititi ii memoram in 
  				//dim_block
    			dim_block = read(fd,block.payload, sizeof(package_d.data));
    			block.len = dim_block;

    			//instantiem un pachet de tip D cu paylaod-ul creat mai sus
    			package_d = D(block.payload, k->payload[2]);
    			t = send_D(package_d);

    			//trimitem mesajul catre receiver
    			send_message(&t);

    			//asteptam confirmarea
    			k = receive_message_timeout(5000);

    			//daca avem TIMEOUT parasim bucla
    			//ar trebui retrimis mesajul dar, din nou,
    			//nu functioneaza
    			if(confirm_message(k) == -1){
    				printf("TIMEOUT!\n");
    				break;
    			}
 				//la fel si pentru pachete de tip N
    			else if(confirm_message(k) == 0){
    				printf("GOT NAK!SHOULD RECEIVE MESSEGE AGAIN!\n");
    				break;
    			}
    			//daca am primit ACK scadem din dimensiunea totala a fisierului 
    			//dimensiunea pe care am scris in fisierul rezultat
    			else if(confirm_message(k) == 1){
    				printf("GOT ACK!\n");
    			}
  				dimensiune = dimensiune - dim_block;
        	}

        	//daca am terminat de transmis intreg fisierul, instantiez 
        	//un pachet de tip Z 
    		package_z = ACK_NAK_EOF_EOT(k, 'Z');
    		t = send_ACK_NAK_EOF_EOT(package_z);

    		//trimit pachetul catre receiver
    		send_message(&t);

    		//astept raspuns
    		k = receive_message_timeout(5000);

    		//daca avem TIMEOUT sau NAK parasim bucla
    		if(confirm_message(k) == -1 || confirm_message(k) == 0){
    			printf("GOT NAK OR TIMEOUT!\n");
    			break;
    		}
    		//altfel inchidem fisierul
    		else if(confirm_message(k) == 1){
    			printf("GOT ACK! CLOSE THE FILE!\n");
    			close(fd);
    		}
    	}
    	i = i + 1;
        files_number = files_number - 1;

        //daca am terminat de trimis toate fsierele instantiem 
        //un pachet de tip B
        if(files_number == 0){
    		package_b = ACK_NAK_EOF_EOT(k, 'B');
    		t = send_ACK_NAK_EOF_EOT(package_b);

    		//trimitem mesajul catre receiver
    		send_message(&t);

    		//asteptam raspuns
    		k = receive_message_timeout(5000);

    		//indiferent de raspuns, pentru ca programul nu functioneaza pentru un canal 
    		//cu erori, parasesc bucla
    		if(confirm_message(k) == 1 || confirm_message(k) == 0 || confirm_message(k) == -1){
    			break;
    		}
    	}
	}
    return 0;
}
