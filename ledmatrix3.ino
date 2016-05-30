#include <SPI.h>
#include "fonts.h"
#include <avr/pgmspace.h>

#define HEN 2    // decoder enable pin
#define HA 6    // decoder pin a and a rising enables decoder
#define HB 7    // decoder pin b
#define LAT 10  //latches data on shift registers
#define PANEL 3  //NUMBER OF PANELS IN SERIES
#define MAXROW 16  // NUMBER OF ROWS
#define ROWBUFFCOUNT  MAXROW/1  //NUMBER OF ROW BUFF
#define COL8BIT 4*PANEL  // NUMBER OF SHIFT IN A PANEL
#define MSBs 4*PANEL
#define TEMP_PIN A0

#include <pt.h>

unsigned char  tempchar[2];

unsigned char invert = 1;
unsigned int count = 0;

uint8_t x;

static long SHIFTRATE = 50;
static struct pt pt1,pt2;

String alphabetsUpper="ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789*";

String msgString = "KEC LAB BLOCK    TEMP -=*    ";
//String msgString = "";
uint8_t shiftedMSB[MSBs]={0};
uint16_t msgPos = 0;

uint8_t msgIndex[100];
uint8_t msgLength = 0;
uint8_t cbit = 7;

uint8_t rowBuff[ROWBUFFCOUNT][COL8BIT]={0};//={
                     // {0xff,0xFF,0xff,0xFf},  // 0 4 8 12 ROW DATA
                     // {0xFF,0xff,0xFF,0xff},  //1 5 9 13 ROW DATA
                     // {0xff,0xFF,0xff,0xFF},  //2 6 10 14  ROW DATA
                    //  {0xff,0xff,0xFF,0xFF}  // 3 7 11 15 ROW DATA
                    //  };

/**********function to calculate temperature in cel***********************/
void getTemp(){
  float temp;
  temp = analogRead(TEMP_PIN);
  temp*=0.48828125;
  tempchar[0] =  (char)temp/10;
  tempchar[1] = (char)temp%10;

}


/*********************this contains the index number in********************* 
**********************upper alphabets array >>>>>>>>>>>>********************
***************************************************************************/
void getMsgIndex(){
    uint8_t n;
    msgLength = msgString.length();
    for(n = 0;n< msgLength;n++){
      msgIndex[n] = alphabetsUpper.indexOf(msgString.charAt(n));
    }
}
                 
                      
void shiftRowBuff(uint8_t invert){
//  uint8_t rowBuffMsb[ROWBUFFCOUNT][COL8BIT];
  uint8_t i,j,currentByte;
  //////// storing MSB OF ALL BUFFER ELEMENTS////////
//  for(i = 0;i< ROWBUFFCOUNT; i++){
//    for(j = 0;j< COL8BIT; j++){
//      rowBuffMsb[i][j] = rowBuff[i][j]>>7;
//    }
//  }
  /////////SHIFTING ALL THE BUFFER BY 1 TO THE LEFT/////////
  /////////ALSO ADD MSB TO LSB OF PRECEDING WORD///////////
  for(i = 0;i< ROWBUFFCOUNT; i++){
    for(j = 0;j<COL8BIT;j++){
      rowBuff[i][j] = rowBuff[i][j]<<1;
      if(j == COL8BIT-1 ){    // last one has to fed by another msb whiich is not loaded yet
        continue;
      }  
      rowBuff[i][j] = rowBuff[i][j] | (rowBuff[i][j+1]>>7);
    }
  }
  
  ////////////////////////////////////////////////////////////////
  /*************loading new data**********************////////////
  
  for( i = 0;i< ROWBUFFCOUNT ; i++){
    currentByte = pgm_read_byte_near(font16x8 + msgIndex[msgPos] * 16 + i);
    if(invert == 1)
      currentByte = ~currentByte;
    // Serial.println(currentByte,HEX);
    currentByte >>= cbit;
    currentByte &= 0x01;
    rowBuff[i][4*PANEL-1] |= currentByte;
   
  }
  cbit--;
  //Serial.println(cbit);
  if(cbit == 255){
      msgPos++;
      cbit = 7;
  }
  if(msgPos >= msgLength)
     msgPos = 0;
}

////////////thread to call temperature update//////////////
static int thread2(struct pt *pt,long timeout){
  static long t1 = 0;
  PT_BEGIN(pt);
  while(1){
    PT_WAIT_UNTIL(pt,(millis()-t1)>timeout);
   // Serial.println(getTemp());
   getTemp();  
   msgString.setCharAt(x,tempchar[0]+48);
   msgString.setCharAt(x+1,tempchar[1]+48);
   getMsgIndex();
 //  Serial.print(tempchar[0]);
   Serial.println(msgString);
   t1 = millis();
  }
  PT_END(pt);
}



////////////////////thread to call shifting row//////////////////////
static int thread1(struct pt *pt,long timeout){
  static long t1 = 0;
  PT_BEGIN(pt);
  while(1){
    PT_WAIT_UNTIL(pt,(millis()-t1)>timeout);
    shiftRowBuff(invert);
    count++;
    if(count == 1000){
      count = 0;
      invert = !invert;
    }
    t1 = millis();
  }
  PT_END(pt);
}

/**********************************************
******** function to select row****************
*********************************************/
// row 0 4 8 12 are row 0
// row 1 5 9 13 are row 1
// row 2 6 10 14 are row 2
// row 3 7 11 15 are row 3

void selRow(uint8_t row){  
  int j;
  digitalWrite(HEN,HIGH);  // disables the decoder
     
  digitalWrite(HA,LOW);    // preapre for rising edge
  digitalWrite(HA,HIGH);    // rising edge on A enables decoder e\1\
  
     ///////////selsect proper row////////////
  if(row ==0){    
    digitalWrite(HA,0);      
    digitalWrite(HB,0);
  }
  else if (row == 2){
    digitalWrite(HA,0);
    digitalWrite(HB,1);
  }
  else if (row ==1 ){
    digitalWrite(HA,1);
    digitalWrite(HB,0);
  }
  else{
    digitalWrite(HA,1);
    digitalWrite(HB,1);
  }
  
   digitalWrite(HEN,LOW);    //enables the deocder e\2\
  
  digitalWrite(LAT,LOW);
  for(j=0;j<(16*PANEL);j++)
        SPI.transfer(0x00);

  digitalWrite(LAT,HIGH);
  digitalWrite(HEN,HIGH);
  
}

/////////////////////////////////////////////////////////////////////////////
/////////////function  to scan each row/////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

void scanRow(uint8_t row){
  int x;
  if( row <= 3 ){
       ////////////////////////////////////////////
      digitalWrite(LAT,LOW);
      
      for(x= 0;x < MSBs;x++){
        SPI.transfer(0);
        SPI.transfer(0x00);
        SPI.transfer(0x0);
      //  Serial.println(rowBuff[row][x]);
        SPI.transfer(rowBuff[row][x]);
    }
      digitalWrite(LAT,HIGH);
      selRow(row);
      ///////////////////////////////////////
  }else if (row >= 4 && row <=7){
         ////////////////////////////////////////////
      digitalWrite(LAT,LOW);
      for(x= 0;x < MSBs;x++){
        SPI.transfer(0);
        SPI.transfer(0x00);
        SPI.transfer(rowBuff[row][x]);
        SPI.transfer(0x0);
      }
      digitalWrite(LAT,HIGH);
      selRow(row-4);
      ///////////////////////////////////////
  }else if (row >= 8 && row <=11){
         ////////////////////////////////////////////
      digitalWrite(LAT,LOW);
      for(x= 0;x < MSBs;x++){
        SPI.transfer(0);
        SPI.transfer(rowBuff[row][x]);
        SPI.transfer(0x00);
        SPI.transfer(0x0);
      }
      digitalWrite(LAT,HIGH);
      selRow(row-8);
      ///////////////////////////////////////
  }else if (row >= 12){
         ////////////////////////////////////////////
      digitalWrite(LAT,LOW);
      for(x= 0;x < MSBs;x++){
        SPI.transfer(rowBuff[row][x]);
        SPI.transfer(0);
        SPI.transfer(0x00);
        SPI.transfer(0x0);
      }
      digitalWrite(LAT,HIGH);
      selRow(row-12);
      ///////////////////////////////////////
  }
}

void scanDisplay(){
  int i;
  for(i = 0;i<MAXROW;i++){
    scanRow(i);
  }
}

void IoInit(){
  pinMode(HEN,OUTPUT);
  pinMode(HA,OUTPUT);
  pinMode(HB,OUTPUT);
  pinMode(LAT,OUTPUT);

  digitalWrite(HA,LOW);
  digitalWrite(HB,LOW);
  digitalWrite(LAT,LOW);
  digitalWrite(HEN,LOW);
}

void SpiInit(){
   SPI.begin();		
   SPI.setBitOrder(MSBFIRST);	
   SPI.setDataMode(SPI_MODE0);	
   SPI.setClockDivider(SPI_CLOCK_DIV2);
}


void setup() {
  Serial.begin(57600);
  IoInit();
  SpiInit();  
  PT_INIT(&pt1);
  x=msgString.indexOf('-');
  getMsgIndex();
}


void loop() {
  thread1(&pt1,SHIFTRATE);
  thread2(&pt2,1000);
  scanDisplay();
  
}

