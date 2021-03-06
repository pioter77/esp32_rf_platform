
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <SdFat.h>
#include <uRTCLib.h>
#include <nRF24L01.h>
#include <RF24.h>

/*
trzeba przerobic tak by menu bylo struktura kazda struktora z wlasnym poem na zmienna na nazwe i zmienna na opcje ktore submenu ma zawierac potem
trzeba tez zrobic tablice tych struktur ta tablica bedzie przechowywac te glowne menu rowsy i kazdy row bedzie struktura- elementem tablicy struktur 
i tylko zmienne sie bedzie wybierac i podawac funcji wyswietlajacej ktora tez naezy przerobic
*/
#define SD_CS_PIN SS
#define OLED_ADDR   0x3C
#define BTN1 33 //d3
#define BTN2 32 //d4
#define BTN3 27 //d5 do d8

//kolejnosc menu row
#define MENU_ORDER_RTC 2
#define MENU_ORDER_GPS 1
#define MENU_ORDER_SLIDER 0
#define MENU_ORDER_NRF24 3
#define MENU_ORDER_HC05 4
#define MENU_ORDER_RF 5
#define MENU_ORDER_EMPTY1 6
#define MENU_ORDER_EMPTY2 7

#define DEL_BTN 550
#define OLED_RESET 4

#define GPS_BAUD 9600
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
unsigned long time_last_nrf_sent=0;

//int ac_cursor_pos_prev=ac_cursor_pos;
volatile bool flag3=0;//right button pressed flag
volatile bool flag1=0;//left button pressed flag
volatile bool flag2=0;//middle button pressed flag
volatile bool flag4=0;//screen refresh need flag
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
uint8_t rtc_year;
};

struct time_struct time_holder={0,0,0,0,0,0};

struct gps_struct
{
  bool gps_last_pos_succ,gps_last_date_succ,gps_last_time_succ;
  double gps_lati,gps_longi,gps_alti;
  uint8_t gps_day,gps_month,gps_hour,gps_minutes,gps_seconds;
  uint16_t gps_year;
}gps_holder{0,0,0,0,0,0,0,0,0,0,0,0};

struct nrf_struct
{
  const uint64_t pipe1;
  const char txt[6];
  bool status;
}nrf_holder{0xE8E8F0F0E1LL,"12",0};



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

flag1=1;
Serial.println("inter1");
}

void IRAM_ATTR isr2()
{

flag2=1;
Serial.println("inter2");
//flag2=0;

}

void IRAM_ATTR isr3()
{

flag3=1;
Serial.println("inter3");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void menu_display();
int menu_slider_calc(int *rows,int *vert_space);
void menu_row(int lp,const char txt[],int x_start,int *actual_pos);
void submenu_type1(int *select,int *slider_pos);
void submenu_display(int option);
void submenu_type_def(int *select);
void submenu_rtc(int *select,time_struct *ts);
void submenu_gps(int *select, gps_struct *gs);
void submenu_nrf(int *select, nrf_struct *ns);
bool gps_values_read(uint8_t selector,gps_struct *gs);
//void submenu_logic_fcn(byte *actual_row_posit,int *actual_submenu_pos);
void submenu_logic_fcn(byte *actual_row_posit,int *actual_submenu_pos, gps_struct *gs,time_struct *ts,nrf_struct *ns);
bool log_maker(byte selector,const char *filename,gps_struct *gs,time_struct *ts,nrf_struct *ns);
bool nrf_send_fcn(nrf_struct *ns,unsigned long *prev_time);


void rtc_read_fcn(time_struct *ts);
bool save_value_to_sd(double value,const char *filename,bool new_line);
bool save_char_to_sd(char *txt,const char *filename,bool new_line);
//bool gps_values_read(uint8_t selector,double *lati,double *longi,double *alti,uint8_t *day,uint8_t *month,uint16_t *year,uint8_t *hour,uint8_t *minutes,uint8_t *sec);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

File myFile;
TinyGPSPlus gps;
uRTCLib rtc;
SdFat SD;
Adafruit_SSD1306 OLED(128,64,&Wire,OLED_RESET);
RF24 radio(13,12);  //chip enable csn d8 d2

void setup() {
  Serial.begin(115200);
  Serial.println("Basic Encoder Test:");
  Serial2.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2); //for gps communication
  rtc.set_rtc_address(0x68);
	rtc.set_model(URTCLIB_MODEL_DS3231);
  menu_slider_height=menu_slider_calc(&menu_rows,&menu_vertical_space);
  while(!SD.begin(SD_CS_PIN,SD_SCK_MHZ(10)))
    Serial.println("sd initialization failed!");
    
  
  Serial.println("sd initialization done.");
//inicjalizacja przuciskow do obslugi
  pinMode(BTN1,INPUT_PULLUP);
  pinMode(BTN2,INPUT_PULLUP);
  pinMode(BTN3,INPUT_PULLUP);
 //3 piny na przerwaniach
  attachInterrupt(digitalPinToInterrupt(BTN1),isr1,FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN2),isr2,FALLING);
  attachInterrupt(digitalPinToInterrupt(BTN3),isr3,FALLING);
//przygotowanie wyswietlacza do bootowania
  OLED.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR);
  OLED.clearDisplay();
  OLED.setFont(NULL);
  OLED.setTextWrap(false);
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  flag4=1; 
 //Serial.println(menu_slider_height);
 radio.begin(); //begin rf24 operation
}

void loop()
{
  
  unsigned long ac_time=millis();   // for calculating time delays, actual time value

 // flag1_1=(flag1 && ac_time-time_last_buton1_pressed>DEL_BTN)  ? 1:0;
  //flag2_2=(flag2 && ac_time-time_last_buton2_pressed>DEL_BTN)  ? 1:0;
  //flag3_3=(flag3 && ac_time-time_last_buton3_pressed>DEL_BTN)  ? 1:0;

  if(flag1 && ac_time-time_last_buton1_pressed>DEL_BTN)
  {
    flag1=0;
    flag4=1;  //display needs refresh when 1
      if(!flag_submenu)
      { //this happens when user isnt inside submenu main menu scrolling
        ac_cursor_pos-=8;
        counter1--;
        if(ac_cursor_pos<0) 
          {
          ac_cursor_pos=0;
          counter1=1;
          }
        Serial.println(ac_cursor_pos);  //for debugging
      }else if(flag_horizontal_slider1 && flag_submenu) //when select button was pressed when on slider, slider active need to move it
      {
        submenu_slid_pos-=8;
      }else{
        counter_submenu--;  //move through  submenu items
      }
    time_last_buton1_pressed=millis(); // important to prevent oscillations fro mbutton pressing
  }

/////////////////////////////////////////////////////////////////////////////////////middle button
   if(flag2 && ac_time-time_last_buton2_pressed>DEL_BTN)
  {
    flag2=0;
    flag4=1;

    //w to wejdzie jak f-submenu=0 wiec jako wcisniemy srdokowy bedac w menu glownym
   if(!flag_submenu) //when flag sumbmenu 0
   {
    flag_submenu=1; //to get into submenu
    Serial.println("flag submenu");
    Serial.println(flag_submenu);
   }else//ten else musi tu byc bo bez tego po wlaczeniu submenu od razu  by z miego wyszedl
  //if(flag_submenu)
  {//to obśługa srodkowego przycisku bedac w submenu jakims
    //Serial.println("inside");

  /////////////////////////////////////////funkcja logiki poruszania po menu selecta srodkowego przycisku
  submenu_logic_fcn(&counter1,&counter_submenu,&gps_holder,&time_holder,&nrf_holder);

  }
     

   // Serial.println(ac_cursor_pos);

     
   time_last_buton2_pressed=millis();
    //interrupts();
     //portEXIT_CRITICAL(&mux);
  }
/////////////////////////////////////////////////////////////////////////////////////
   if(flag3 && ac_time-time_last_buton3_pressed>DEL_BTN)
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


        ////////////////////////////////////time block/////////////////////////////////////
    //rtc_read_fcn(&time_holder);

   // flag=0;
  //interrupts();


  }
/////////////////////////////////////end test//////////////////////////////////////////////
}




int menu_slider_calc(int *rows,int *vert_space)
{
  int ret=(*vert_space/ *rows<1) ? 1: *vert_space/ *rows;
  return ret;
}
/*displays a main menu row
place--text to display--x start write position--actual cursor position for inverting colour*/
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
/*main function to display things on the oled screen
takes no parameters for now*/
void menu_display(void)
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



  menu_row(MENU_ORDER_SLIDER,"slider",0,&ac_cursor_pos);
  menu_row(MENU_ORDER_RTC,"rtc",0,&ac_cursor_pos);
  menu_row(MENU_ORDER_GPS,"gps",0,&ac_cursor_pos);
  menu_row(MENU_ORDER_NRF24,"NRF24",0,&ac_cursor_pos);
  menu_row(MENU_ORDER_HC05,"HC_12",0,&ac_cursor_pos);
  menu_row(MENU_ORDER_RF,"RF",0,&ac_cursor_pos);
  menu_row(MENU_ORDER_EMPTY1,"empty1",0,&ac_cursor_pos);
  menu_row(MENU_ORDER_EMPTY2,"empty2",0,&ac_cursor_pos);
  


 OLED.display(); //output 'display buffer' to screen  
}
/*function for displaying submenus scontent
takes number of the submenu to display
for now--1 slider menu--2 rtc time and date set/display--other default empty submenu*/
void submenu_display(int option)
{
  switch (option)
  {//+1 bo counter 1 dziala na zakresie od 1 a ordery sa od zera
    case MENU_ORDER_SLIDER+1:
    submenu_type1(&counter_submenu,&submenu_slid_pos);
    break;
    case MENU_ORDER_RTC+1:
    submenu_rtc(&counter_submenu,&time_holder);
    break;
    case MENU_ORDER_GPS+1:
    submenu_gps(&counter_submenu,&gps_holder);
    break;
    case MENU_ORDER_NRF24+1:
    submenu_nrf(&counter_submenu,&nrf_holder);
    break;
    default:
   // Serial.println("err occured i nsubmenu");
    submenu_type_def(&counter_submenu);
  }
  
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

void submenu_rtc(int *select,time_struct *ts)
{
  const uint8_t sec_line=9,trd_line=19;
  //const for positioning buttons
  const uint8_t y_but_level=55;
  const uint8_t x_first_btn=1;
  const uint8_t x_sec_btn=32;
  const uint8_t x_rd_btn=63;
  //thxt in buttons
  const char btn1_t[]= "exit";
  const char btn2_t[]="set";
  const char btn3_t[]="F5";
  //rtc_read_fcn(ts); //update struct values 
  OLED.clearDisplay();

  OLED.setCursor(0,0);
  OLED.println("time menu");

  if(*select>3) *select=3;
  if(*select<1) *select=1;
  //date printout
  OLED.setCursor(0,sec_line);
  OLED.println("date");
  OLED.setCursor(25,sec_line);
  OLED.println(ts->rtc_day);
  OLED.setCursor(36,sec_line);
  OLED.println(".");
  OLED.setCursor(41,sec_line);
  OLED.println(ts->rtc_month);
  OLED.setCursor(51,sec_line);
  OLED.println(".");
  OLED.setCursor(56,sec_line);
  OLED.println(ts->rtc_year);
  //time printout
  OLED.setCursor(0,trd_line);
  OLED.println("time");
  OLED.setCursor(25,trd_line);
  OLED.println(ts->rtc_hour);
  OLED.setCursor(36,trd_line);
  OLED.println(":");
  OLED.setCursor(41,trd_line);
  OLED.println(ts->rtc_minutes);
  OLED.setCursor(51,trd_line);
  OLED.println(":");
  OLED.setCursor(56,trd_line);
  OLED.println(ts->rtc_seconds);

  //button display
     switch(*select)
  {       
    case 3: //refresh gps readout btn
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.fillRect(x_rd_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
      OLED.setTextColor(WHITE);
      break;
    case 2: //save btn
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.fillRect(x_sec_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.setTextColor(WHITE);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
      break;

    default: //jka jeden exit musi byc jako default
      OLED.fillRect(x_first_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.setTextColor(WHITE);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
  }
OLED.display(); //output 'display buffer' to screen  
}
void submenu_gps(int *select, gps_struct *gs)
{
  if(*select>3) *select=1;
  if(*select<1) *select=3;

  //values dror buttons positioning
  const uint8_t y_but_level=55;
  const uint8_t x_first_btn=1;
  const uint8_t x_sec_btn=32;
  const uint8_t x_rd_btn=63;

  const uint8_t y_readout_pos=9;
  const uint8_t x_readout_pos=0;
  const uint8_t x_shift=40;
  const uint8_t y_shift=10;
   //thxt in buttons
  const char btn1_t[]= "exit";
  const char btn2_t[]="save";
  const char btn3_t[]="F5";
  const char err_msg[]="not found";

  OLED.clearDisplay();
  //main title
  OLED.setCursor(0,0);
  OLED.println("gps menu");

  OLED.setCursor(x_readout_pos,y_readout_pos);
  OLED.println("lat:");
  OLED.setCursor(x_readout_pos+x_shift,y_readout_pos);
  if(gs->gps_last_pos_succ){
    OLED.println(gs->gps_lati);
  }else{
    OLED.println(err_msg);
  }
  OLED.setCursor(x_readout_pos,y_readout_pos+y_shift);
  OLED.println("long:");
  OLED.setCursor(x_readout_pos+x_shift,y_readout_pos+y_shift);
  if(gs->gps_last_pos_succ){
    OLED.println(gs->gps_longi);
  }else{
    OLED.println(err_msg);
  }
  OLED.setCursor(x_readout_pos,y_readout_pos+2*y_shift);
  OLED.println("alt:");
  OLED.setCursor(x_readout_pos+x_shift,y_readout_pos+2*y_shift);
  if(gs->gps_last_pos_succ)
  {
    OLED.println(gs->gps_alti);
  }else{
    OLED.println(err_msg);
  }
  //buttons display
    switch(*select)
  {       
    case 3: //refresh gps readout btn
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.fillRect(x_rd_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
      OLED.setTextColor(WHITE);
      break;
    case 2: //save btn
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.fillRect(x_sec_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.setTextColor(WHITE);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
      break;

    default: //jka jeden exit musi byc jako default
      OLED.fillRect(x_first_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.setTextColor(WHITE);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
  }
  OLED.display(); //output 'display buffer' to screen  
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
/*selector:1->location 2->date 3->time---&global gps structure name
should be used to save retrurned value to first 3 variables in gps_structure if 0 the readout was wrong!*/
bool gps_values_read(uint8_t selector,gps_struct *gs)
{
  bool tmp_flag=0;
   while (Serial2.available() > 0)
    if(gps.encode(Serial2.read())) tmp_flag=1;
    if(!tmp_flag) return 0;

  switch (selector)
  {
    case 1:
      if (!gps.location.isValid()) return 0;
      gs->gps_lati=gps.location.lat();
      gs->gps_longi=gps.location.lng();
      gs->gps_alti=gps.altitude.meters();
    return 1;
   
    case 2:
      if (!gps.date.isValid()) return 0;
      gs->gps_month=gps.date.month();
      gs->gps_day=gps.date.day();
      gs->gps_year=gps.date.year();
    return 1;

    case 3:
       if (!gps.time.isValid()) return 0;
      gs->gps_hour=gps.time.hour();
      gs->gps_minutes=gps.time.minute();
      gs->gps_seconds=gps.time.second();
    return 1;

    default:
    return 0;
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

//counter1, counter_submenu to jest funcka obslugujaca logkige wcisniec srodkowego przycisku dla poszeglonych submenu rozniacych sie przyciskami
//trzeba zrobi strukture struct menu_curs_n_flags i ona bedzie miec wszystkie cursory globalne i flagi słuzace do oreintacji i logiki w menu oprocz? flag od przerwan
void submenu_logic_fcn(byte *actual_row_posit,int *actual_submenu_pos, gps_struct *gs,time_struct *ts,nrf_struct *ns)
{
  //counter1
  if(*actual_row_posit==MENU_ORDER_SLIDER+1)
  {
      //menu slider logic
    switch (counter_submenu)  //&& counter1 1-8
    {
      case 2://ok button
        flag_submenu=0;
        flag_horizontal_slider1=0;//get back to operate on vertical slider values
      break;

      case 3: //get off/in the slider selection to control buttons/to control slider
        flag_horizontal_slider1=!flag_horizontal_slider1; //bez tego odwracania wartosci by sie nie dal owysjc
      break;

      default: //when 1 thats exit
      flag_submenu=0;
      flag_horizontal_slider1=0;//get back to operate on vertical slider values
    }
  }
  else if(*actual_row_posit==MENU_ORDER_GPS+1)
  {
          //menu gps logic
    switch (counter_submenu)  //&& counter1 1-8
    {
      case 2://save button
       // flag_submenu=0;
        //flag_horizontal_slider1=0;//get back to operate on vertical slider values
      log_maker(1,"log_gps_submenu.csv",&gps_holder,&time_holder,&nrf_holder);
         
      break;

      case 3: //refresh //po winien flage ze struktury zmienic na 1 i wywlac funcke robiaca odczyt do stuktury gps i zerujacej ta flage
        //flag_horizontal_slider1=!flag_horizontal_slider1; //bez tego odwracania wartosci by sie nie dal owysjc
        flag4=1; //refresh the screen
       // flag_submenu=1;
        gs->gps_last_pos_succ=gps_values_read(1,&gps_holder);
        gs->gps_last_date_succ=gps_values_read(2,&gps_holder);
        gs->gps_last_time_succ=gps_values_read(3,&gps_holder);
        //debug:
        Serial.println("gps readout refresh:");
        Serial.println(gs->gps_lati);
        Serial.println(gs->gps_longi);
        Serial.println(gs->gps_alti);
         Serial.println(gs->gps_day);
        Serial.println(gs->gps_month);
        Serial.println(gs->gps_year);
         Serial.println(gs->gps_hour);
        Serial.println(gs->gps_minutes);
        Serial.println(gs->gps_seconds);
       // flag_horizontal_slider1=0;
      break;

      default: //when 1 thats exit
      flag_submenu=0;
      flag_horizontal_slider1=0;//get back to operate on vertical slider values
    }
  }
  else if(*actual_row_posit==MENU_ORDER_RTC+1)
  {   //tylko exit na ten moment
        switch (counter_submenu)  //&& counter1 1-8
    {
      case 2://set button
        flag_submenu=0;
        flag_horizontal_slider1=0;//get back to operate on vertical slider values
      break;

      case 3: //refresh button
      flag4=1;    //express the nedd for refrshing screen
      rtc_read_fcn(ts); //update struct values 
      //flag_horizontal_slider1=0;//get back to operate on vertical slider values      break;
      //flag_submenu=0;
      break;

      default: //when 1 thats exit
      flag_submenu=0;
      flag_horizontal_slider1=0;//get back to operate on vertical slider values
    }
  }
  else if(*actual_row_posit==MENU_ORDER_NRF24+1)
  {
          //menu gps logic
    switch (counter_submenu)  //&& counter1 1-8
    {
      case 2://save button
       // flag_submenu=0;
        //flag_horizontal_slider1=0;//get back to operate on vertical slider values
          gs->gps_last_pos_succ=gps_values_read(1,&gps_holder);
        gs->gps_last_date_succ=gps_values_read(2,&gps_holder);
        gs->gps_last_time_succ=gps_values_read(3,&gps_holder);
      log_maker(2,"log_nrf24_submenu.csv",&gps_holder,&time_holder,&nrf_holder);
         
      break;

      case 3: //refresh //po winien flage ze struktury zmienic na 1 i wywlac funcke robiaca odczyt do stuktury gps i zerujacej ta flage
        //flag_horizontal_slider1=!flag_horizontal_slider1; //bez tego odwracania wartosci by sie nie dal owysjc
        flag4=1; //refresh the screen
       // flag_submenu=1;
      nrf_send_fcn(&nrf_holder,&time_last_nrf_sent);
        //debug:
    
       // flag_horizontal_slider1=0;
      break;

      default: //when 1 thats exit
      flag_submenu=0;
      flag_horizontal_slider1=0;//get back to operate on vertical slider values
    }
  }
  else
  {
    //when only exit in menu or not defined function yet exit case 
      flag_submenu=0;
      flag_horizontal_slider1=0;//get back to operate on vertical slider values
  }
  
}

bool log_maker(byte selector,const char *filename,gps_struct *gs,time_struct *ts,nrf_struct *ns)
{
  switch (selector)
  {
    case 1://gps menu log call
      gs->gps_last_pos_succ=gps_values_read(1,&gps_holder);
      gs->gps_last_date_succ=gps_values_read(2,&gps_holder);
      gs->gps_last_time_succ=gps_values_read(3,&gps_holder);
      rtc_read_fcn(ts);


      myFile = SD.open(filename, FILE_WRITE);
      if(myFile)
      {
        //rtc date
        myFile.print(ts->rtc_day);
        myFile.print(";");
        myFile.print(ts->rtc_month);
        myFile.print(";");
        myFile.print(ts->rtc_year);
        myFile.print(";");
        //rtc time
        myFile.print(ts->rtc_hour);
        myFile.print(";");
        myFile.print(ts->rtc_minutes);
        myFile.print(";");
        myFile.print(ts->rtc_seconds);
        myFile.print(";");
        //gps position
        myFile.print(gs->gps_last_pos_succ);
        myFile.print(";");
        myFile.print(gs->gps_lati);
        myFile.print(";");
        myFile.print(gs->gps_longi);
        myFile.print(";");
        myFile.print(gs->gps_alti);
        myFile.print(";");
        //gps date
        myFile.print(gs->gps_last_date_succ);
        myFile.print(";");
        myFile.print(gs->gps_day);
        myFile.print(";");
        myFile.print(gs->gps_month);
        myFile.print(";");
        myFile.print(gs->gps_year);
        myFile.print(";");
        //gps time
        myFile.print(gs->gps_last_time_succ);
        myFile.print(";");
        myFile.print(gs->gps_hour);
        myFile.print(";");
        myFile.print(gs->gps_minutes);
        myFile.print(";");
        myFile.print(gs->gps_seconds);
        myFile.println(";");

        myFile.close();
        return 1;//file opened succesfully and text written to it
      }
    case 2://nrf24 menu log call
      gs->gps_last_pos_succ=gps_values_read(1,&gps_holder);
      gs->gps_last_date_succ=gps_values_read(2,&gps_holder);
      gs->gps_last_time_succ=gps_values_read(3,&gps_holder);
      rtc_read_fcn(ts);


      myFile = SD.open(filename, FILE_WRITE);
      if(myFile)
      {
        //rtc date
        myFile.print(ts->rtc_day);
        myFile.print(";");
        myFile.print(ts->rtc_month);
        myFile.print(";");
        myFile.print(ts->rtc_year);
        myFile.print(";");
        //rtc time
        myFile.print(ts->rtc_hour);
        myFile.print(";");
        myFile.print(ts->rtc_minutes);
        myFile.print(";");
        myFile.print(ts->rtc_seconds);
        myFile.print(";");
        //gps position
        myFile.print(gs->gps_last_pos_succ);
        myFile.print(";");
        myFile.print(gs->gps_lati);
        myFile.print(";");
        myFile.print(gs->gps_longi);
        myFile.print(";");
        myFile.print(gs->gps_alti);
        myFile.print(";");
        //gps date
        myFile.print(gs->gps_last_date_succ);
        myFile.print(";");
        myFile.print(gs->gps_day);
        myFile.print(";");
        myFile.print(gs->gps_month);
        myFile.print(";");
        myFile.print(gs->gps_year);
        myFile.print(";");
        //gps time
        myFile.print(gs->gps_last_time_succ);
        myFile.print(";");
        myFile.print(gs->gps_hour);
        myFile.print(";");
        myFile.print(gs->gps_minutes);
        myFile.print(";");
        myFile.print(gs->gps_seconds);
        myFile.println(";");
         //nrf24 status
        myFile.print(ns->status);
        myFile.print(";");
        myFile.print(ns->txt);
        myFile.println(";");

        myFile.close();
        return 1;//file opened succesfully and text written to it
      }
      default:
      return 0;//file opening error
  }
}

void submenu_nrf(int *select,nrf_struct *ns)
{
  const uint8_t sec_line=9,trd_line=19;
  //const for positioning buttons
  const uint8_t y_but_level=55;
  const uint8_t x_first_btn=1;
  const uint8_t x_sec_btn=32;
  const uint8_t x_rd_btn=63;
  //thxt in buttons
  const char btn1_t[]= "exit";
  const char btn2_t[]="log";
  const char btn3_t[]="change";
  //rtc_read_fcn(ts); //update struct values 
  OLED.clearDisplay();

  OLED.setCursor(0,0);
  OLED.println("NRF_24 menu");

  if(*select>3) *select=3;
  if(*select<1) *select=1;
  //date printout
  OLED.setCursor(0,sec_line);
  OLED.println("status:");
  OLED.setCursor(55,sec_line);
  OLED.println(ns->status);
 
  //time printout
  OLED.setCursor(0,trd_line);
  OLED.println("status:");
  OLED.setCursor(55,trd_line);
  OLED.println(ns->status);
 

  //button display
     switch(*select)
  {       
    case 3: //refresh gps readout btn
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.fillRect(x_rd_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
      OLED.setTextColor(WHITE);
      break;
    case 2: //save btn
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.fillRect(x_sec_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.setTextColor(WHITE);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
      break;

    default: //jka jeden exit musi byc jako default
      OLED.fillRect(x_first_btn-1,y_but_level-1,30,9,WHITE);
      OLED.setTextColor(BLACK);
      OLED.setCursor(x_first_btn,y_but_level);
      OLED.println(btn1_t);
      OLED.setTextColor(WHITE);
      OLED.setCursor(x_sec_btn,y_but_level);
      OLED.println(btn2_t);
      OLED.setCursor(x_rd_btn,y_but_level);
      OLED.println(btn3_t);
  }
OLED.display(); //output 'display buffer' to screen  
}

bool nrf_send_fcn(nrf_struct *ns,unsigned long *prev_time)
{
  radio.setChannel(2);
  radio.setPayloadSize(7);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(ns->pipe1);


  unsigned long tmp=millis();
  if(tmp-*prev_time>500)
  {
    radio.write(ns->txt, 6);
    ns->status=!ns->status;
    *prev_time=millis();
  }
  return 1;
}