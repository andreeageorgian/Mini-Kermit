#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

//functie pe care ar fi trebuit sa o folosesc la retrimiterea raspunsurilor
void resend_message(unsigned short* crc, unsigned short* new_crc, msg* k){
    minikermit package;
    msg r, t;

    while((*new_crc) != (*crc)){
            //daca mesajul nu a ajuns => TIMEOUT
            if(k == NULL){
                perror("TIMEOUT!\n");
                return -1;
            }
            else {
                    //atfel trimitem NAK
                    package = ACK_NAK_EOF_EOT(k,'N');
                    package.seq = k->payload[2] + 1;
                    r = send_ACK_NAK_EOF_EOT(package);
                    memcpy(t.payload, r.payload, r.len + 1);
                    t.len = r.len;
                    send_message(&t);

                    //asteptam raspunsul de la sender
                    k = receive_message_timeout(5000);

                    //recalculam crc-uls i il comparam cu cel 
                    //din paylaod
                    *crc = crc16_ccitt(k->payload, k->len - 4);
                    memcpy(new_crc, &(k->payload[15]), 2);

                    //daca crc-urile sunt egale, parasim functia
                    if(*crc == *new_crc){
                        break;
                    }
                }
    }
    return;
}

//functie de confirmare a unui pachet
int confirm_message(msg* k){
    unsigned short new_crc, crc;
    minikermit package;
    msg r,t;

    //daca mesajul este null => TIMEOUT
    if(k == NULL){
            printf("TIMEOUT!\n");
            return -1;
    }
    else if (k!= NULL) {
            //comparam crc-ul primit si cel calculat
            crc = crc16_ccitt(k->payload, k->payload[1] - 4);
            memcpy(&new_crc, &(k->payload[k->payload[1]]), 2);

            //daca nu sunt egale, trimitem NAK
            if(new_crc != crc){
                package = ACK_NAK_EOF_EOT(k, 'N');
                package.seq = package.seq + 1;
                r = send_ACK_NAK_EOF_EOT(package);
                memcpy(t.payload, r.payload, r.len + 1);
                t.len = r.len;
                send_message(&t);
            }

            //daca sunt egale trimitem ACK
            if(new_crc == crc){
                package = ACK_NAK_EOF_EOT(k, 'Y');
                package.seq = package.seq + 1;
                r = send_ACK_NAK_EOF_EOT(package);
                memcpy(t.payload, r.payload, r.len + 1);
                t.len = r.len;
                send_message(&t);
                return 1;
            }
        }
    return 0;
}

int main(int argc, char** argv) {
    msg r, t;

    init(HOST, PORT);
    unsigned short crc, new_crc;
    msg* k;

    minikermit package_Y;
    minikermits package_YS;

    //asteptam primirea pachetului de tip S
    k = receive_message_timeout(5000);

    //calculam crc-ul si il comparam cu cel primit
    crc = crc16_ccitt(k->payload, k->len - 4);
    memcpy(&new_crc, &(k->payload[15]), 2);

    //daca nu am primit raspuns => TIMEOUT
    if(k == NULL){
        printf("TIMEOUT!\n");
        return 0;
    }

    //daca crc-urile sunt diferite ar trebui sa 
    //retrimitem mesajul, dar functia nu merge
    if(new_crc != crc){
        resend_message(&crc, &new_crc, k);
    }

    //daca crc-urile sunt egale, trimitem ACK
    if(new_crc == crc){
        package_YS = ACK_S(k);
        package_Y.seq = package_Y.seq + 1;
        r = send_ACK_S(package_YS);
        memcpy(t.payload, r.payload, r.len);
        t.len = r.len;
        send_message(&t);
    }

    //INCEPEM SA PRIMIM FISERE
    //primim numarul de fisiere
    if (recv_message(&r)<0){
        printf("EROARE NUMAR DE FISIERE!\n");
        return 0;
    }

    sprintf(t.payload,"%s",r.payload);
    t.len = strlen(t.payload+1);
    send_message(&t);

    int files_number = atoi(r.payload);
    
    msg newfile;
    while(files_number >=1)
    {
        memset(k->payload, 0 , sizeof(k->payload));
        
        //primim un pachet de tip F
        k = receive_message_timeout(5000);
        if(k == NULL){
            printf("TIMEOUT!\n");
            break;
        }
        if (k!= NULL) {
            //calculam crc-ul si il comparam cu cel primit
            crc = crc16_ccitt(k->payload, k->payload[1] - 2);
            memcpy(&new_crc, &(k->payload[k->payload[1]]), 2);

            //daca sunt diferite trimitem NAK
            if(new_crc != crc){
                //resend_message(&crc, &new_crc, k);
                package_Y = ACK_NAK_EOF_EOT(k, 'N');
                package_Y.seq = package_Y.seq + 1;
                r = send_ACK_NAK_EOF_EOT(package_Y);
                memcpy(t.payload, r.payload, r.len + 1);
                t.len = r.len;
                send_message(&t);
            }

            if(new_crc == crc){
                //daca sunt eale trimitem ACK
                package_Y = ACK_NAK_EOF_EOT(k, 'Y');
                package_Y.seq = package_Y.seq + 1;
                r = send_ACK_NAK_EOF_EOT(package_Y);
                memcpy(t.payload, r.payload, r.len + 1);
                t.len = r.len;
                send_message(&t);
            
                //extragem numele fisierlui din pachet
                memset(newfile.payload, 0 , sizeof(newfile.payload));
                memcpy(newfile.payload, &(k->payload[4]), k->payload[1] - 4);
                newfile.len = k->payload[1] - 4;

                //primim dimensiunea fisierului
                if (recv_message(&r)<0){
                    perror("Receive message");
                    return -1;
                }
                printf("[%s] Got msg with payload: %s\n",argv[0],r.payload);
                sprintf(t.payload,"%s",r.payload);
                t.len = strlen(t.payload+1);
                //trimitem confirmare
                send_message(&t);

                //concatenam "recv_" cu numele fisierului
                char src[20]  = "recv_";
                char dest[250];
                strcpy(dest, newfile.payload);
                char temp[250];
                strcpy(temp, src);
                strcat(temp, dest);
                strcpy(dest, temp);
                strcpy(newfile.payload, dest);

                //file descriptul corespunzator fisierlui
                int fd = open(newfile.payload, O_WRONLY | O_CREAT, 0777);

                //dimensiunea fisierului
                int dimensiune = atoi(r.payload);

                //dimensiunea unui bloc de date
                int dim_block;
                while(dimensiune > 0){
                    memset(k->payload, 0 , sizeof(k->payload));

                    //asteptam sa primim un pachet de tip D
                    k = receive_message_timeout(5000);

                    //daca nu primim raspuns => TIMEOUT
                    if(k == NULL){
                        printf("TIMEOUT!\n");
                        break;
                    }
                    else{
                        //altfel, calculam crc-ul si il comparam cu 
                        //cel primit
                        crc = crc16_ccitt(k->payload, k->payload[1] - 2);
                        memcpy(&new_crc, &(k->payload[k->payload[1]]), 2);

                        //daca sunt diferite, trimitem NAK
                        if(new_crc != crc){
                            //resend_message_kermite(&crc, &new_crc, k);
                            package_Y = ACK_NAK_EOF_EOT(k, 'N');
                            r = send_ACK_NAK_EOF_EOT(package_Y);
                            memcpy(t.payload, r.payload, r.len + 1);
                            t.len = r.len;
                            send_message(&t);
                        }

                        //daca sunt egale, scriem datele din pachetul D 
                        // si trimitem ACK
                        if(new_crc == crc){
                            dim_block = write(fd,&(k->payload[4]), k->payload[1] - 5);
                            package_Y = ACK_NAK_EOF_EOT(k, 'Y');
                            r = send_ACK_NAK_EOF_EOT(package_Y);
                            memcpy(t.payload, r.payload, r.len + 1);
                            t.len = r.len;
                            send_message(&t);
                        }
                    }
                    //scadem din dimensiunea totala a fisierului dimensiunea blocului 
                    //scris
                    dimensiune = dimensiune - dim_block;
                }

                //daca am terminat de scris in fisier, asteptam sa primim un 
                //pachet de tip Z
                k = receive_message_timeout(5000);

                //daca am primit cu succes pachetul Z, inchidem fisierul
                if(confirm_message(k) == 1){
                    close(fd);
                }
            }
        }
        files_number = files_number -1;
        
        //daca am terminat de scris in toate fisierele, asteptam sa primim 
        //un pachet de tip B
        if(files_number == 0){
            k = receive_message_timeout(5000);

            //daca am primit pachetul B cu succes, parasim bucla
            if(confirm_message(k) == 1){
                break;
            }
        }
    }
    return 0;
}
