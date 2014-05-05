#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
//ADD -DDEBUG for debug options
#include "sommeTab.h"

/** GETTING VALS FROM BINARY FILE **/
long long int getVals(int fd, int first, int* tab){//0 true, 1 false
	char *buf;
	long long int val;
	int i, j, tabpos = 0;
	/*Expl
	Type particule = 2bits
	Valeur enregistrée = 14 bits
	Temps = 60 bits
	
	First:
	12345678.(x6).1234XXXX
	Second:
	32 bytes d'écart (2+14 *16 bits), mais 4 bits de First = décalage de 4 bits
	<=>
	1234XXXX XXXXXXXX XXXXYYYY YYYYYYYY YYYY.(x13).WWWWZZZZ ZZZZ1234 5678...
	<=> La valeur suivante aura un décalage de 8 bits (1 byte)
	<=> Le schéma est identique à First

	Le saut des 31 bytes joue sur le modulo 1 byte via la variable first!
	*/
	if((buf = (char*)calloc(40, sizeof(char))) == NULL){
		printf("ERR CALLOC\n");
		perror("");
		return -1;
	}
	//check this
	if(tab != NULL){
		free(tab);
	}
	if((tab = (int*)calloc(17, sizeof(int))) == NULL){
		printf("ERR CALLOC\n");
		perror("");
		return -1;
	}
	if(read(fd, buf, 40) < 0){
		printf("ERR READ\n");
		perror("");
		return -2;
	}

	//Convertir les bits en long long int (vérifier endian)
	//Pour temps
	val = 0;
	for(i=0;i<8;i++){
		for(j=0;j<8;j++){
			if(buf[i]>>(7-j)&1){
				val *= 2;
				val += 1;
			}else{
				val *= 2;
			}
		}
	}
	//Suppression de bits inutiles
	if(first == 1){
		val = val<<4;
	}
	val = val>>4;
	//Pour capteurs
	
	//IF XXXXYYYY YYYYYYYY YYYYZZZZ
	//Lire 4 dernier bits, 8 bits, 4 premier bits
	//IF YYYYYYYY YYYYYYYY
	//Lire 8x2 bits
	//no shift
	if(first == 1){// YYYYYYYY YYYYYYYY
		for(i=8;i<40;i=i+2){
			for(j=0;j<8;j++){
				if(buf[i]>>(7-j)&1){
					tab[tabpos] *= 2;
					tab[tabpos] += 1;
				}else{
					tab[tabpos] *= 2;
				}
			}
			for(j=0;j<8;j++){
				if(buf[i+1]>>(7-j)&1){
					tab[tabpos] *= 2;
					tab[tabpos] += 1;
				}else{
					tab[tabpos] *= 2;
				}
			}
			tabpos++;
		}
	}
	else{// XXXXYYYY YYYYYYYY YYYYZZZZ
		for(i=7;i<40;i=i+2){
			for(j=4;j<8;j++){
				if(buf[i]>>(7-j)&1){
					tab[tabpos] *= 2;
					tab[tabpos] += 1;
				}else{
					tab[tabpos] *= 2;
				}
			}
			for(j=0;j<8;j++){
				if(buf[i+1]>>(7-j)&1){
					tab[tabpos] *= 2;
					tab[tabpos] += 1;
				}else{
					tab[tabpos] *= 2;
				}
			}
			for(j=0;j<4;j++){
				if(buf[i+2]>>(7-j)&1){
					tab[tabpos] *= 2;
					tab[tabpos] += 1;
				}else{
					tab[tabpos] *= 2;
				}
			}
			tabpos++;
		}
	}
	//Aller à la valeur suivante.
	//-1 car modulo
	if(first == 0){
		lseek(fd, -1, SEEK_CUR);
	}
	#ifdef DEBUG
		printf("Time: %lld",val);
	#endif
	return val;
}
/** END GETTING VALS FROM BINARY FILE **/
/** DELTA STUFF **/
//Gain moyen avec le fichier donné >= 30 bits, soit >= 50%
long long int deltacompression(long long int old, long long int next){
	return next - old;
}
//nombres de bits dans un long long int
int getSize(long long int val){
	int i, ret;
	ret = 0;
	for(i=0;i<64;i++){
		if((val>>i)&1){
			ret = i;
		}
	}
	//+1 car décallage avec boucle for i
	return ret+1;
}
//nombres de bits dans unlong long int encodé en binaire sur 6 bits (2 premiers du char à ignorer)
char getLength(long long int val){
	char ret = getSize(val);
	return ret;
}
/** END DELTA STUFF **/

/** WRITING STUFF **/
typedef struct buffer{
	int bufferSize;
	int bitPos;
	int fd;
	char* buffer;
} buffer;

void initBufferStruct(buffer *buf){
	if(buf->buffer != NULL){
		#ifdef DEBUG
			printf("FREEING\n");
		#endif
		free(buf->buffer);
	}
	if((buf->buffer = (char*)calloc(1, sizeof(char))) == NULL){
		printf("ERR CALLOC\n");
		return;
	}
	buf->bufferSize = 1;
	buf->bitPos = 0;
}
/* Théoriquement jamais appelé */
void biggify(buffer *buf){
	if((buf->buffer = (char*)realloc(buf->buffer, buf->bufferSize*2)) == NULL){
		printf("ERR REALLOC\n");
		buf->bufferSize = 0;
		return;
	}
	buf->bufferSize *= 2;
}
void checkBuffer(buffer *buf){
	if(buf->bufferSize -1 <= buf->bitPos/8){
		#ifdef DEBUG
			printf("\nMaking buffer bigger\n");
		#endif
		biggify(buf);
	}
}
void writeToFile(buffer *buf){
	//TODO check if fail
	#ifdef DEBUG
		printf("\nWriting %d char %d bits stuff to %d\n",buf->bitPos/8, buf->bitPos, buf->fd);
	#endif
	if(write(buf->fd, buf->buffer, buf->bitPos/8)<0){
		printf("\nERR WRITE\n");
	}
	initBufferStruct(buf);
}
//Capteur
void insertCaptInBuffer(buffer *buf, int val, int length){
	int i;
	for(i=0; i<length; i++){
		if(val>>i&1){
			checkBuffer(buf);
			buf->buffer[buf->bitPos/8] += pow(2,buf->bitPos%8);
			buf->bitPos++;
			#ifdef DEBUG
				printf("LEN VAL %d %lf\n",buf->bitPos/8,pow(2,buf->bitPos%8));
			#endif
		}else{
			#ifdef DEBUG
				printf("LEN VAL %d\n",buf->bitPos/8);
			#endif
			buf->bitPos++;
		}
	}
	#ifdef DEBUG
		printf("BIT POS %d\n",buf->bitPos);
	#endif
	if(buf->bitPos%8 == 0){
		#ifdef DEBUG
			printf("\nWRITING TO FILE\n");
		#endif
		writeToFile(buf);
	}
}
//Delta
void insertDeltInBuffer(buffer *buf, long long int val, int length){
	int i;
	//Write length
	#ifdef DEBUG
		printf("\nAdding %lld of length %d, bit pos is %d\n",val, length, buf->bitPos);
	#endif
	for(i=0; i<6; i++){
		if(length>>i&1){
			checkBuffer(buf);
			buf->buffer[(buf->bitPos/8)] += pow(2,buf->bitPos%8);
			buf->bitPos++;
			#ifdef DEBUG
				printf("LEN VAL %d %lf\n",buf->bitPos/8,pow(2,buf->bitPos%8));
			#endif
			
		}else{
			buf->bitPos++;
			#ifdef DEBUG
				printf("LEN VAL %d\n",buf->bitPos/8);
			#endif
		}
	}
	#ifdef DEBUG
		printf("BIT POS %d\n",buf->bitPos);
	#endif
	//Write delta
	for(i=0; i<length; i++){
		if(val>>i&1){
			checkBuffer(buf);
			buf->buffer[buf->bitPos/8] += pow(2,buf->bitPos%8);
			buf->bitPos++;
			#ifdef DEBUG
				printf("DELTA VAL %d %lf\n",buf->bitPos/8,pow(2,buf->bitPos%8));
			#endif
			
		}else{
			buf->bitPos++;
			#ifdef DEBUG
				printf("DELTA VAL %d\n",buf->bitPos/8);
			#endif
		}
	}
	#ifdef DEBUG
		printf("BIT POS %d\n",buf->bitPos);
	#endif
	if(buf->bitPos%8 == 0){
		#ifdef DEBUG
			printf("\nWRITING TO FILE\n");
		#endif
		writeToFile(buf);
	}
	#ifdef DEBUG
		printf("BIT POS %d\n",buf->bitPos);
	#endif
}
/** CALL THESE FUNCTIONS ONLY **/
//Delta
void addDelta(buffer *buf, long long int val){
	insertDeltInBuffer(buf, val, getSize(val));
}
//16bits
void addCapteurVal(buffer *buf, int val){
	insertCaptInBuffer(buf, val, 16);
}
//10bits
void addCapteurInd(buffer *buf, int val){
	insertCaptInBuffer(buf, val, 10);
}
/** END CALL THESE FUNCTIONS ONLY **/
/** END WRITING STUFF **/

void freeAll(buffer *buf){
	if(buf->buffer != NULL){
		#ifdef DEBUG
			printf("FREEING\n");
		#endif
		free(buf->buffer);
	}
	free(buf);
}

void print_capteurs(int *capteurs) {
				int i;
				int *sommes = NULL;
				printf("Somme launched\n");
				somme_capteurs(capteurs, sommes);
				printf("Somme done\n");
				for(i=0; i<5; i++) printf("Capteur %d : %d\n", i, sommes[i]);
}

//argv[1] = name of file READ, argv[2] = name of file WRITTEN
int main(int argc, char **argv){
	int fdr, fdw, i; 
	long long int old, next;
	int* tab = NULL;
	buffer *buf;
	if((fdr = open(argv[1], O_RDONLY)) < 0){
		printf("ERR OPEN READ\n");
		return 1;
	}
	if((fdw = open(argv[2], O_WRONLY)) < 0){
		printf("ERR OPEN WRITE\n");
		return 1;
	}
	//Init structure
	if((buf = (buffer*)calloc(1, sizeof(buffer))) == NULL){
		printf("ERR CALLOC\n");
		return 1;
	}
	initBufferStruct(buf);
	buf->fd = fdw;
	//End init structure

	//Initialisation du delta
	old = getVals(fdr,0, tab);
	addDelta(buf, old);
	//TODO ADD FIRST CAPTEUR HERE
	//End init
	#ifdef DEBUG
		printf(" Delta %lld\n",old);
	#endif
	
	for(i=1;(next = getVals(fdr,i%2, tab))>0;i++){
		//Alterner First et Second
		#ifdef DEBUG
			printf("\nNext: %lld\nOld: %lld\n",next, old);
			printf(" Length: %d\n",getSize(deltacompression(old,next)));
			printf(" Delta: %lld\n",deltacompression(old,next));
			//print_capteurs(tab);
		#endif
		addDelta(buf, deltacompression(old,next));
		old = next;
		//TODO ADD ACTIVE CAPTEURS + CAPTEUR VAL HERE
	}
	
	close(fdr);
	close(fdw);
	freeAll(buf);
	return 0;
}
