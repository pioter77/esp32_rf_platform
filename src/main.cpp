
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <SdFat.h>
#include <uRTCLib.h>
/*
trzeba przerobic tak by menu bylo struktura kazda struktora z wlasnym poem na zmienna na nazwe i zmienna na opcje ktore submenu ma zawierac potem
trzeba tez zrobic tablice tych struktur ta tablica bedzie przechowywac te glowne menu rowsy i kazdy row bedzie struktura- elementem tablicy struktur 
i tylko zmienne sie bedzie wybierac i podawac funcji wyswietlajacej ktora tez naezy przerobic
*/
#define SD_CS_PIN SS
#define OLED_ADDR   0x3C
#define btn1 33 //d3
#define btn2 32 //d4
#define btn3 27 //d5 do d8

#define del_btn1 400
#define OLED_RESET 4

#define gps_baud 9600
#define RXD2 16
#define TXD2 17
//#define del_encoder 250

int val;
int ac_cursor_pos=0;
int pinA_last;
int n=LOW;

int menu_rows=8;
int menu_vertical_space=63-8+1;
int menu_slider_height=1;
byte counter1=1;
unsigned long time_last_buton1_pressed=0;
unsigned long time_last_buton2_pressed=0;
unsigned long time_last_buton3_pressed=0;

//int ac_cursor_pos_prev=ac_cursor_pos;
volatile bool flag3=0;
volatile bool flag1=0;
volatile bool flag2=0;
volatile bool flag4=0; //aktualizacja wyswietlacza
bool flag_submenu=0;
bool flag_horizontal_slider1=0; //for incrementation when i n submenu 
int counter_submenu=1;
int submenu_slid_pos=0; //slider for submenu value max allowed 119

//portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;  //for interrupts on esp32

struct menu_bar{
  byte position;  //to sort things to allow to display in proper order
  char txt[];
  byte submenu_t;
};

struct time_struct{

uint8_t rtc_day,rtc_month, rtc_hour, rtc_minutes,rtc_seconds;
uint16_t rtc_year;
};

struct time_struct time_holder={0,0,0,0,0,0};


//global variables for gps readouts clarity
double gps_lati=0;
double gps_longi=0;
double gps_alti=0;

uint8_t gps_day=0;
uint8_t gps_month=0;
uint16_t gps_year=0;

uint8_t gps_hour=0;
uint8_t gps_minutes=0;
uint8_t gps_seconds=0;
void IRAM_ATTR isr1()
{
//noInterrupts();
 //portENTER_CRITICAL(&mux);
flag1=1;
Serial.println("inter1");
}

void IRAM_ATTR isr2()
{
//noInterrupts();
//portENTER_CRITICAL(&mux);
flag2=1;
Serial.println("inter2");
//flag2=0;

}

void IRAM_ATTR isr3()
{
//noInterrupts();
//portENTER_CRITICAL(&mux);
flag3=1;
Serial.println("inter3");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void menu_display();
int menu_slider_calc(int *rows,int *vert_space);
void menu_row( int lp,const char txt[],int x_start,int *actual_pos);
void submenu_type1(int *select,int *slider_pos);
void submenu_display(int option);
void submenu_type_def(int *select);

void rtc_read_fcn(time_struct *ts);
bool save_value_to_sd(double value,const char *filename,bool new_line);
bool save_char_to_sd(char *txt,const char *filename,bool new_line);
bool gps_values_read(uint8_t selector,double *lati,double *longi,double *alti,uint8_t *day,uint8_t *month,uint16_t *year,uint8_t *hour,uint8_t *minutes,uint8_t *sec);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

File myFile;
TinyGPSPlus gps;
uRTCLib rtc;
SdFat SD;
Adafruit_SSD1306 OLED(128,64,&Wire,OLED_RESET);

void setup() {
  Serial.begin(115200);
  Serial.println("Basic Encoder Test:");
  Serial2.begin(gps_baud, SERIAL_8N1, RXD2, TXD2); //for gps communication
  rtc.set_rtc_address(0x68);
	rtc.set_model(URTCLIB_MODEL_DS3231);
 menu_slider_height=menu_slider_calc(&menu_rows,&menu_vertical_space);
  while(!SD.begin(SD_CS_PIN,SD_SCK_MHZ(10)))
  {
    Serial.println("sd initialization failed!");
    
  }
     Serial.println("sd initialization done.");

  pinMode(btn1,INPUT_PULLUP);
   pinMode(btn2,INPUT_PULLUP);
    pinMode(btn3,INPUT_PULLUP);
  
  //pinA_last=digitalRead(pinA_last);
 attachInterrupt(digitalPinToInterrupt(btn1),isr1,FALLING);
attachInterrupt(digitalPinToInterrupt(btn2),isr2,FALLING);
 attachInterrupt(digitalPinToInterrupt(btn3),isr3,FALLING);


  OLED.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR);
  OLED.clearDisplay();
  OLED.setFont(NULL);
  OLED.setTextWrap(false);
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  flag4=1;
 //Serial.println(menu_slider_height);
}

void loop()
{
  
  unsigned long ac_time=millis();

 // flag1_1=(flag1 && ac_time-time_last_buton1_pressed>del_btn1)  ? 1:0;
  //flag2_2=(flag2 && ac_time-time_last_buton2_pressed>del_btn1)  ? 1:0;
  //flag3_3=(flag3 && ac_time-time_last_buton3_pressed>del_btn1)  ? 1:0;

  if(flag1 && ac_time-time_last_buton1_pressed>del_btn1)
  {
    flag1=0;
    flag4=1;
  if(!flag_submenu)
  {
    ac_cursor_pos-=8;
    counter1--;
    if(ac_cursor_pos<0) 
    {ac_cursor_pos=0;
    counter1=1;
    }
     Serial.println(ac_cursor_pos);
  }else if(flag_horizontal_slider1 && flag_submenu)
  {
    submenu_slid_pos-=8;
  }else{
        counter_submenu--;

  }
  
    
   time_last_buton1_pressed=millis();
    //interrupts();
     //portEXIT_CRITICAL(&mux);
  }

/////////////////////////////////////////////////////////////////////////////////////middle button
   if(flag2 && ac_time-time_last_buton2_pressed>del_btn1)
  {
    flag2=0;
    flag4=1;

    //ac_cursor_pos=0;
   // counter1=1;
   if(!flag_submenu) //when flag sumbmenu 0
   {
    flag_submenu=1; //to get into submenu
    Serial.println("flag submenu");
    Serial.println(flag_submenu);
   }else//ten else musi tu byc bo bez tego po wlaczeniu submenu od razu  by z miego wyszedl
  //if(flag_submenu)
  {
    Serial.println("inside");
    switch (counter_submenu)
    {
      case 2:
        flag_submenu=0;
        flag_horizontal_slider1=0;//get back to operate on vertical slider values
      break;

      case 3:
        flag_horizontal_slider1=!flag_horizontal_slider1; //bez tego odwracania wartosci by sie nie dal owysjc
      break;

      default: //when 1 thats exit
      flag_submenu=0;
      flag_horizontal_slider1=0;//get back to operate on vertical slider values
    }
  }
     

   // Serial.println(ac_cursor_pos);

     
   time_last_buton2_pressed=millis();
    //interrupts();
     //portEXIT_CRITICAL(&mux);
  }
/////////////////////////////////////////////////////////////////////////////////////
   if(flag3 && ac_time-time_last_buton3_pressed>del_btn1)
  {
     flag3=0;
    flag4=1;

  if(!flag_submenu)
  {
    ac_cursor_pos+=8;
    counter1++;
     if(ac_cursor_pos>56)//jakby zmieniac ilosc plansz to to by ptrzeba bylo razy 2 dla 2giej planszy?
     { ac_cursor_pos=56;//albo dac flage drugiej planszy na 1 chyba lepsze rozwiazanie i potem menu_display na podstawie tej flagi wielkosci bedzie odpowedniego ifa wyswietlac
     counter1=8;
     }
     Serial.println(ac_cursor_pos);
  }else if(flag_horizontal_slider1 && flag_submenu)
  {
    submenu_slid_pos+=8;
  }else{
        counter_submenu++;

  }
  
    
   time_last_buton3_pressed=millis();
   // interrupts();
    //portEXIT_CRITICAL(&mux);
  }

 
  
  if(flag4)
  {
    if(!flag_submenu)
    {
      menu_display();
    }else{
      submenu_display(counter1);
    }
 flag4=0;

 //////////////////////////////////////test///////////////////////////////////////////////////////////////
  bool flag=0;
    while (Serial2.available() > 0){
    if( gps.encode(Serial2.read())) flag =1;
    }
    if(flag)
    {
        //displayInfo();
      
          if(gps_values_read(1,&gps_lati,&gps_longi,&gps_alti,&gps_day,&gps_month,&gps_year,&gps_hour,&gps_minutes,&gps_seconds))
          {
            Serial.println("posit:");
            Serial.println(gps_lati);
            Serial.println(gps_longi);

            save_char_to_sd((char *)"posit","gps_log.txt",1);
            save_value_to_sd(gps_lati,"gps_log.txt",1);
            save_value_to_sd(gps_longi,"gps_log.txt",1);
          }else{
            Serial.println("no gps pos found");
            save_char_to_sd((char *)"no gps pos found","gps_log.txt",1);
          }

          if(gps_values_read(2,&gps_lati,&gps_longi,&gps_alti,&gps_day,&gps_month,&gps_year,&gps_hour,&gps_minutes,&gps_seconds))
          {
            Serial.println("date:");
            Serial.println(gps_day);
            Serial.println(gps_month);
            Serial.println(gps_year);

            save_char_to_sd((char *)"date","gps_log.txt",1);
            save_value_to_sd(gps_day,"gps_log.txt",1);
            save_value_to_sd(gps_month,"gps_log.txt",1);
            save_value_to_sd(gps_year,"gps_log.txt",1);
          }else{
            Serial.println("no gps date found");
            save_char_to_sd((char *)"no gps date found","gps_log.txt",1);
          }

          if(gps_values_read(3,&gps_lati,&gps_longi,&gps_alti,&gps_day,&gps_month,&gps_year,&gps_hour,&gps_minutes,&gps_seconds))
          {
            Serial.println("time::");
            Serial.println(gps_hour);
            Serial.println(gps_minutes);
            Serial.println(gps_seconds);

            save_char_to_sd((char *)"time","gps_log.txt",1);
            save_value_to_sd(gps_hour,"gps_log.txt",1);
            save_value_to_sd(gps_minutes,"gps_log.txt",1);
            save_value_to_sd(gps_seconds,"gps_log.txt",1);
          }else{
            Serial.println("no gps time found");
            save_char_to_sd((char *)"no gps time found","gps_log.txt",1);
          }
          rtc_read_fcn(&time_holder);

    Serial.println("RTC DateTime: ");

    Serial.print(time_holder.rtc_day);
    Serial.print('/');
    Serial.print(time_holder.rtc_month);
    Serial.print('/');
    Serial.print(time_holder.rtc_year);

    Serial.print(' ');

    Serial.print(time_holder.rtc_hour);
    Serial.print(':');
    Serial.print(time_holder.rtc_minutes);
    Serial.print(':');
    Serial.print(time_holder.rtc_seconds);


  


    flag=0;
  //interrupts();


  }
/////////////////////////////////////end test//////////////////////////////////////////////
}
}

int menu_slider_calc(int *rows,int *vert_space)
{
  int ret=(*vert_space/ *rows<1) ? 1: *vert_space/ *rows;
  return ret;
}

void menu_row( int lp,const char txt[],int x_start,int *actual_pos)
{
  int y_tmp=(lp==0 || lp%8==0)  ? 0:(lp*8);

  if(lp==*actual_pos/8)//wybrana wiec podswietlic bo na tej opcje najechalismy
  {
    OLED.fillRect(0,ac_cursor_pos,110,9,WHITE);
    OLED.setTextColor(BLACK);
    OLED.setCursor(x_start,y_tmp);
    OLED.println(txt);
    OLED.setTextColor(WHITE);
  }else{
    OLED.setCursor(x_start,y_tmp);
    OLED.println(txt);
  }
 
}

void menu_display()
{
   OLED.clearDisplay();

OLED.setCursor(64,0);
OLED.println(ac_cursor_pos);
//OLED.setCursor(0,8);
//OLED.println(counter1);
OLED.setCursor(111,0);
OLED.println(counter1);
OLED.setCursor(117,0);
OLED.println("/");
OLED.setCursor(123,0);

OLED.println(8);
OLED.drawLine(126,8,126,63,WHITE);
OLED.drawRect(125,counter1*7+1,3,menu_slider_height,WHITE);



menu_row(0,"pozycja:",0,&ac_cursor_pos);
menu_row(1,"opcja1",0,&ac_cursor_pos);
menu_row(2,"opcja2",0,&ac_cursor_pos);
menu_row(3,"opcja3",0,&ac_cursor_pos);
menu_row(4,"opcja4",0,&ac_cursor_pos);
menu_row(5,"opcja5",0,&ac_cursor_pos);
menu_row(6,"opcja6",0,&ac_cursor_pos);
menu_row(7,"opcja7",0,&ac_cursor_pos);


 OLED.display(); //output 'display buffer' to screen  
}
void submenu_display(int option)
{
  switch (option)
  {
    case 1:
    submenu_type1(&counter_submenu,&submenu_slid_pos);
    break;
    case 2:
    submenu_type1(&counter_submenu,&submenu_slid_pos);
    break;
    default:
   // Serial.println("err occured i nsubmenu");
    submenu_type_def(&counter_submenu);
  }
  
}

void submenu_type1(int *select,int *slider_pos)
{
  if(*select>3) *select=1;
  if(*select<1) *select=3;

  //protect value of slider from exceeding limit
  *slider_pos=(*slider_pos>120) ? 120:*slider_pos;
  *slider_pos=(*slider_pos<0) ? 0 :*slider_pos;
  OLED.clearDisplay();

  OLED.setCursor(0,10);
  OLED.println("eg vert slider 1");

  OLED.drawLine(0,40,127,40,WHITE);
  switch(*select)
  {
  if(*select==1)

  
   case 3: //slider 
   if(flag_horizontal_slider1) OLED.drawRect(*slider_pos+3,37,2,7,WHITE);
   OLED.drawRect(*slider_pos,39,8,3,WHITE);
  OLED.setCursor(20,25);
  OLED.println("exit");
  OLED.setCursor(80,25);
  OLED.println("OK");
   
    break;
  case 2: //ok btn
   OLED.drawRect(*slider_pos,39,8,3,WHITE);
    OLED.setCursor(20,25);
    OLED.println("exit");
     OLED.fillRect(79,24,30,9,WHITE);
    OLED.setTextColor(BLACK);
    OLED.setCursor(80,25);
    OLED.println("OK");
    OLED.setTextColor(WHITE);
    break;

  default: //jka jeden exit musi byc jako default
 OLED.drawRect(*slider_pos,39,8,3,WHITE);
   OLED.fillRect(19,24,30,9,WHITE);
    OLED.setTextColor(BLACK);
    OLED.setCursor(20,25);
    OLED.println("exit");
    OLED.setTextColor(WHITE);
    OLED.setCursor(80,25);
    OLED.println("OK");
  
  }

  OLED.display(); //output 'display buffer' to screen  
}

void submenu_type_def(int *select){
  OLED.clearDisplay();

  OLED.setCursor(0,10);
  OLED.println("blank menu");

  if(*select>1) *select=1;
  if(*select<1) *select=1;
    OLED.fillRect(19,24,30,9,WHITE);
    OLED.setTextColor(BLACK);
    OLED.setCursor(20,25);
    OLED.println("exit");
    OLED.setTextColor(WHITE);

    OLED.display(); //output 'display buffer' to screen  
}


bool gps_values_read(uint8_t selector,double *lati,double *longi,double *alti,uint8_t *day,uint8_t *month,uint16_t *year,uint8_t *hour,uint8_t *minutes,uint8_t *sec)
{
  switch (selector)
  {
    case 1:
       if (!gps.location.isValid()) return 0;
      *lati=gps.location.lat();
      *longi=gps.location.lng();
      *alti=gps.altitude.meters();
    return 1;
   
    case 2:
      if (!gps.date.isValid()) return 0;
      *day=gps.date.month();
      *month=gps.date.day();
      *year=gps.date.year();
    return 1;

    case 3:
       if (!gps.time.isValid()) return 0;
      *hour=gps.time.hour();
      *minutes=gps.time.minute();
      *sec=gps.time.second();
    return 1;

    default:
    *lati=0;
    *longi=0;
    *alti=0;
    *day=0;
    *month=0;
    *year=0;
    *hour=0;
    *minutes=0;
    *sec=0;
    return 1;

  //return 0; //if not able to connect to gps module at all chceck your connection

 // return 1; //i f gps detected and readout got proper values or at least detected
  }
}

bool save_char_to_sd(char *txt,const char *filename,bool new_line)
{

  myFile = SD.open(filename, FILE_WRITE);
  if(myFile)
  {
    if(new_line)
    {
      myFile.println(txt);
    }else{
      myFile.print(txt);
    }

    myFile.close();
    return 1;//file opened succesfully and text written to it
  }
  return 0;//file opening error
}


bool save_value_to_sd(double value,const char *filename,bool new_line)
{

  myFile = SD.open(filename, FILE_WRITE);
  if(myFile)
  {
    if(new_line)
    {
      myFile.println(value);
    }else{
      myFile.print(value);
    }

    myFile.close();
    return 1;//file opened succesfully and text written to it
  }
  return 0;//file opening error
}

void rtc_read_fcn(time_struct *ts)
{

 //Serial.print("RTC DateTime: ");
  rtc.refresh();
 ts->rtc_day=rtc.day();
 ts->rtc_month=rtc.month();
 ts->rtc_year=rtc.year();

 ts->rtc_hour=rtc.hour();
 ts->rtc_minutes=rtc.minute();
 ts->rtc_seconds=rtc.second();
	
}