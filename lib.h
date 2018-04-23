#ifndef LIB
#define LIB

//structura pentru toate pachetele 
//in afara de S
typedef struct {
	char soh;
	unsigned char len;
	unsigned char seq;
	char type;
	char data[250];
	unsigned short check;
	char mark;
}__attribute__ ((packed, alligned(1))) minikermit;

//structura pentru campul data din pachetul de 
//tup S
typedef struct {
	unsigned char maxl;
	unsigned char time;
	char npad;
	char padc;
	char eol;
	char qctl;
	char qbin;
	char chkt;
	char rept;
	char capa;
	char r;
}__attribute__ ((packed, alligned(1))) datas;

//structura pentru pachetul de tip S
typedef struct {
	char soh;
	unsigned char len;
	unsigned char seq;
	char type;
	datas data;
	unsigned short check;
	char mark;
}__attribute__ ((packed, alligned(1))) minikermits;

typedef struct {
    int len;
    unsigned char payload[1400];
} msg;

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

//FUNCTIILE MELE
unsigned short reverse_crc(unsigned short crc){
    unsigned short rev_crc  = ((crc >> 8) & 0x00FF) | ((crc << 8) & 0xFF00);
    return rev_crc;
}
//functie pentru initializarea unui pachet de tip S
minikermits send_init(){
    minikermits package_s;
    package_s.soh = 0x01;
    package_s.len = sizeof(package_s) - 2;
    package_s.seq = 0x00;
    package_s.type = 'S';
    package_s.data.maxl = 250;
    package_s.data.time = 5;
    package_s.data.npad = 0;
    package_s.data.padc = 0;
    package_s.data.eol = 0x0d;
    package_s.data.qctl = 0;
    package_s.data.qbin = 0;
    package_s.data.chkt = 0;
    package_s.data.rept = 0;
    package_s.data.capa = 0;
    package_s.data.r = 0;

    //dimensiune mark + dimensiune check + 1 
    package_s.check = crc16_ccitt(&package_s, sizeof(package_s ) - 4);
    package_s.mark = 0x0d;

    return package_s;
}

//functie pentru construirea unui pachet ACK pentru 
// un pachet de tip S
minikermits ACK_S(msg* k){
    minikermits package_y;
    package_y.soh = 0x01;
    package_y.len = sizeof(package_y) - 2;
    package_y.seq = k -> payload[2] + 1;
    package_y.type = 'Y';
    memcpy(&package_y.data, &k->payload[4], 11);

    //dimensiune mark + dimensiune check + 1 
    package_y.check = crc16_ccitt(&package_y, sizeof(package_y) - 4);
    package_y.mark = 0x0d;

    return package_y;
}

//functie folosita pentru construirea payload-ului pentru
//pachete de tip S
msg send_ACK_S(minikermits package_Y){
	msg r;
    //introducem noul pachet in payload-ul pe care
    //il vom trimite sender-ului
    memcpy(r.payload, &package_Y.check, 2);

    memcpy(r.payload, &package_Y, 4);
    memcpy(r.payload + 4, &package_Y.data, 11);
    memcpy(r.payload + 15, &package_Y.check, 2);
    memcpy(r.payload + 17, &package_Y.mark, 1);
    r.len = sizeof(package_Y);
    return r;
}

//functie pentru initializarea pachetelor de tip 
// ACK, NAK, EOF si EOT
minikermit ACK_NAK_EOF_EOT(msg* k, char type){
	minikermit package_n;

    package_n.soh = 0x01;
    
    //din lungimea totala a pachetului scadem lungimea 
    //campului data, si campurile SOH(1) si LEN (1)
    package_n.len = sizeof(package_n) - 252;

    package_n.seq = k -> payload[2];
    package_n.type = type;
    
    memset(package_n.data, 0, sizeof(package_n.data));
    
    //dimensiune mark + dimensiune check + 1 
    package_n.check = crc16_ccitt(&package_n, package_n.len - 4);
    
    package_n.mark = 0x0d;

    return package_n;
}

//functie folosita pentru construirea payload-ului
msg send_ACK_NAK_EOF_EOT(minikermit package){
	msg r;
    //punem in payload primele 4 campuri ale pachetului minikermit
    memcpy(r.payload, &package, 4);

    //pentru ca pachetul primit nu este de tip 'S', campul data 
    //este vid => ii atribuim un singur byte, cu valoare nula
    memcpy(r.payload + 4, &package.data, package.len - 5);

    //incepem sa punem in payload de la pozitia 5 deoarece
    // SOH(1) + LEN (1) + SEQ(1) + TYPE(1) + DATA(1) = 4
    memcpy(r.payload + 5, &package.check, 2);

    //incepem de la 7 pentru ca CHECK are 2 bytes
    memcpy(r.payload + 7, &package.mark, 1);
    r.len = 7;

    return r;
}

//functie folosita pentru instantierea unui pachet 
// de tip F
minikermit F(char* file_name, char seq){
	minikermit package_f;
	package_f.soh = 0x01;

	//in len pun lungimea numelui fisierului +
	// SEQ(1), TYPE(1), CHECK(2), MARK(1)
	package_f.len = sizeof(file_name) + 5;
	package_f.seq = seq + 1;
	package_f.type = 'F';
    memset(package_f.data, 0, sizeof(file_name) + 1);
	memcpy(package_f.data, file_name, sizeof(file_name) + 1);
	package_f.check = crc16_ccitt(&package_f, package_f.len - 2);
	package_f.mark = 0x0d;

	return package_f;
}

//construirea payload-ului pentru pachete F
msg send_F(minikermit package_F){
	msg t;

	memcpy(t.payload, &package_F, 4);
    memcpy(t.payload + 4, &package_F.data, package_F.len - 4);
    memcpy(t.payload + package_F.len, &package_F.check, 2);
    memcpy(t.payload + package_F.len + 2, &package_F.mark, 1);
    t.len = package_F.len + 2;

    return t;
}

//instantierea unui pachet de tip D
minikermit D(char* data, char seq){
    minikermit package_d;
    package_d.soh = 0x01;
    package_d.len = strlen(data) + 5;
    package_d.seq = seq + 1;
    package_d.type = 'D';
    memset(package_d.data, 0, strlen(data) + 1);
    memcpy(package_d.data, data, strlen(data) + 1);
    package_d.check = crc16_ccitt(&package_d, package_d.len - 2);
    package_d.mark = 0x0d;

    return package_d;
}

//construirea payload-ului pentru pachete D
msg send_D(minikermit package_D){
    msg t;

    memcpy(t.payload, &package_D, 4);
    memcpy(t.payload + 4, &package_D.data, package_D.len - 4);
    memcpy(t.payload + package_D.len, &package_D.check, 2);
    memcpy(t.payload + package_D.len + 2, &package_D.mark, 1);
    t.len = package_D.len + 2;

    return t;
}


#endif

