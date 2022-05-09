/* FreeRTOS.org includes. */
#include "Arduino_FreeRTOS.h"
#include "queue.h"
#include "DHT.h"

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0X27,16,2); 



/* Define */
#define DHTTYPE DHT11
#define DHTPIN 48
#define LED1   49
#define LED2   50
#define BUZZER 51

/* DHT11 */
DHT dht(DHTPIN, DHTTYPE);

float Hum = 0;
float Temp = 0;

/*  Task control */
String StateLed1 = "0";
String StateLed2 = "0";

/* Module Sim */
int _timeout;
String _buffer;
String number = "0528343980"; 

/* The task function. */
void vTaskControl( void *pvParameters );
void vTaskDHT( void *pvParameters );
void vTaskLCD(void *pvParameters);



/* Handle */
TaskHandle_t HandleDHT;
TaskHandle_t HandleLCD;
TaskHandle_t HandleReceiveFromESP8266;
TaskHandle_t HandleSendToESP8266;
TaskHandle_t HandleControl;
TaskHandle_t HandleWarning;


/* Queue */
QueueHandle_t xQueueControl;
QueueHandle_t xQueueDHT;



typedef struct{
  int  Led1;
  int  Led2;
}StructControl;

typedef struct{
  int  Hum;
  int Temp;
}StructDHT;



void setup() {

   /* join i2c bus with address 8 */
  Serial.begin(9600);
  Serial1.begin(9600);
  dht.begin();
  /* Module Sim */
  _buffer.reserve(50);

  /* Signal Pin */
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  
  /* LCD */
  lcd.init();                    
  lcd.backlight();
  lcd.setCursor(2,0);
  lcd.print("start"); 


  /* FreeRTOS */
  xQueueControl = xQueueCreate( 10, sizeof( StructControl) );
  xQueueDHT     = xQueueCreate( 10, sizeof( StructDHT)     );
  
   xTaskCreate( vTaskWarning, "vTaskWarning", 300, NULL, 5,&HandleWarning                  );   
   xTaskCreate( vTaskSendToESP8266, "vTaskSendToESP8266", 300, NULL, 4,&HandleSendToESP8266); 
   

  if ( xQueueControl != NULL)
  {
    xTaskCreate( vTaskReceiveFromESP8266, "vTaskReceiveFromESP8266", 500, NULL , 3,&HandleReceiveFromESP8266);
    xTaskCreate( vTaskControl, "Task Control", 300, NULL, 4,&HandleControl                                  );
  }
  
  if( xQueueDHT  != NULL )
  {
    xTaskCreate( vTaskDHT, "Task DHT",500, NULL, 2,&HandleDHT );
    xTaskCreate( vTaskLCD, "Task LCD", 500, NULL, 1,&HandleLCD );
  }

  /* Start the scheduler so our tasks start executing. */
  vTaskStartScheduler();
}


void vTaskDHT(void *pvParameters)
{

  long SendTemp;
  long SendHum;
  
  for(;;)
  {
//   Serial.println("---DHT---");
   Hum = dht.readHumidity();
   Temp = dht.readTemperature();
   
   if(isnan(Hum) || isnan(Temp))
   {
      Temp = 0;
      Hum = 0;
   }

   StructDHT SendDHT;
   SendDHT.Hum = int(Hum);
   SendDHT.Temp= int(Temp);
   

   xQueueSendToFront( xQueueDHT ,&SendDHT, 0 );
 
   vTaskDelay(500/portTICK_PERIOD_MS );
   /* Allow the other sender task to execute. */
   taskYIELD();

  }
}

void vTaskLCD(void *pvParameters)
{
  
  StructDHT ReceiveDHT;
  
  const TickType_t xTicksToWait = 100 / portTICK_PERIOD_MS;
  for(;;)
  {
//    Serial.println("---LCD---");  
    xQueueReceive( xQueueDHT, &ReceiveDHT, xTicksToWait );
    
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("hum:");
    lcd.print(ReceiveDHT.Hum);
    lcd.print("%");
    lcd.setCursor(0,1);
    lcd.print("temp:");
    lcd.print(ReceiveDHT.Temp);
    lcd.print(char(223));
    lcd.print("C");

    vTaskDelay(500/portTICK_PERIOD_MS );
  }
}

void vTaskReceiveFromESP8266(void *pvParameters)
{ const TickType_t xTicksToWait = 100 / portTICK_PERIOD_MS;
  BaseType_t xStatus;
  for(;;)
  { 
    if (Serial1.available() >0)
    {
    
//    Serial.println("---ReceiveDataFromESP8266---");
    String str;

    str =  Serial1.readStringUntil('\n');
    StateLed1 = str.substring(0,1);
    StateLed2 = str.substring(2,3);
    StructControl DataToSend;
    DataToSend.Led1 = StateLed1.toInt();
    DataToSend.Led2 = StateLed2.toInt();
    
    xQueueSendToFront( xQueueControl,&DataToSend, xTicksToWait );

    }
    vTaskDelay(500/portTICK_PERIOD_MS );
    /* Allow the other sender task to execute. */
    taskYIELD();
  }
}

void vTaskSendToESP8266(void *pvParameters)
{
  StructDHT DHTValue;
  String DataSend;
  
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
  for(;;)
  {
    
    DataSend = "";
//    Serial.println("---SendToESP8266---");
    xQueueReceive( xQueueDHT, &DHTValue, xTicksToWait );
 
  
    
    DataSend = DataSend + DHTValue.Hum+":"+DHTValue.Temp;
    Serial1.println(DataSend);

    vTaskDelay(300/portTICK_PERIOD_MS );
  }
}

void vTaskControl(void *pvParameters)
{ BaseType_t xStatus;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(100);
  StructControl queue_control;
  
  for(;;)
  {
//    Serial.println("---Control---");
    xStatus = xQueueReceive( xQueueControl, &queue_control,xTicksToWait );
    if(xStatus == pdPASS){
      

      if (queue_control.Led1 == 1) digitalWrite(LED1, HIGH);
      else digitalWrite(LED1, LOW);

      if (queue_control.Led2 == 1) digitalWrite(LED2, HIGH);
      else digitalWrite(LED2, LOW);
            
      }
    

    vTaskDelay(300/portTICK_PERIOD_MS );
  }
}


void vTaskWarning(void *pvParameters)
{ 
 
   

  for(;;)
  {  
    int value = analogRead(A0);   //đọc giá trị điện áp ở chân A0 - chân cảm biến (value luôn nằm trong khoảng 0-1023)
    int per = map(value,0,1023,0,100);
//    Serial.println("---Modle sim---");
    if (per >20)
    { 
      digitalWrite(BUZZER, HIGH);
      
      int Content = 1;
        
      callNumber();
      SendMessage(Content); 
      break;
    }
    else
    {
      digitalWrite(BUZZER, LOW);
    }
    

    vTaskDelay(300/portTICK_PERIOD_MS );
  }
}



///* functions for module sim 800l */
void SendMessage(int x)
{

  Serial.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  vTaskDelay(1000/portTICK_PERIOD_MS );

  Serial.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
  vTaskDelay(1000/portTICK_PERIOD_MS );
  
  String SMS ;
  if (x ==1) SMS = "CO KHI DE CHAY !";
  else SMS ="CO TROM!";
  
  Serial.println(SMS);
  vTaskDelay(100/portTICK_PERIOD_MS );
  Serial.println((char)26);// ASCII code of CTRL+Z
  vTaskDelay(1000/portTICK_PERIOD_MS );
  _buffer = _readSerial();

}
String _readSerial() {
  _timeout = 0;
  while  (!Serial.available() && _timeout < 12000  )
  {
    vTaskDelay(13/portTICK_PERIOD_MS );
    _timeout++;
  }
  if (Serial.available()) {
    return Serial.readString();
  }
}
void callNumber() {
  Serial.print(F("ATD"));
  Serial.print(number);
  Serial.print(F(";\r\n"));
  _buffer = _readSerial();
}


 
void loop()
{
}
