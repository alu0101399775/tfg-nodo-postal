/*
  Hito 3 - 9 taquillas virtuales con codigo unico por envio.
  Pines: rele 4 (ACTIVO A LOW), microinterruptor 3, HC-SR04 trig 5 / echo 6.
  Teclado 4x4 en 22-29. LCD I2C en 0x27.
  Protocolo serie con el PC:
    Arduino -> PC : EVT:DEP_INI:<taq>:      (pide codigo unico)
    PC -> Arduino : SET:<taq>:<codigo>      (asigna el codigo generado)
    PC -> Arduino : QR:<codigo>             (recogida, QR leido por webcam)
    Arduino -> PC : EVT:<tipo>:<taq>:<dato> (telemetria)
  Monitor a 115200.
*/

#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const uint8_t PIN_RELE=4, PIN_FINCAR=3, PIN_TRIG=5, PIN_ECHO=6;
LiquidCrystal_I2C lcd(0x27,20,4);

const byte FILAS=4,COLS=4;
char teclas[FILAS][COLS]={{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte pinesFila[FILAS]={22,23,24,25}, pinesCol[COLS]={26,27,28,29};
Keypad teclado=Keypad(makeKeymap(teclas),pinesFila,pinesCol,FILAS,COLS);

const uint8_t N=9;
struct Taquilla{ bool ocupada; String codigo; };
Taquilla taquillas[N];

enum Estado{ REPOSO, ESPERA_CODIGO, ABIERTA_DEPOSITO, ABIERTA_RECOGIDA };
Estado estado=REPOSO;
int8_t taqActiva=-1;
unsigned long tPuerta=0;
unsigned long tEspera=0;
const unsigned long TIMEOUT=20000;
const unsigned long TIMEOUT_COD=10000;
bool paqueteVisto=false;

void abrir(){  digitalWrite(PIN_RELE, LOW);  }   // rele activo a LOW
void cerrar(){ digitalWrite(PIN_RELE, HIGH); }

void tele(const String&tipo,int taq,const String&dato){
  Serial.print("EVT:");Serial.print(tipo);Serial.print(":");
  Serial.print(taq);Serial.print(":");Serial.println(dato);
}
int buscarLibre(){ for(uint8_t i=0;i<N;i++) if(!taquillas[i].ocupada) return i; return -1; }
int buscarPorCodigo(const String&c){ for(uint8_t i=0;i<N;i++) if(taquillas[i].ocupada&&taquillas[i].codigo==c) return i; return -1; }
bool puertaCerrada(){ return digitalRead(PIN_FINCAR)==LOW; }
long distanciaCM(){
  digitalWrite(PIN_TRIG,LOW);delayMicroseconds(2);
  digitalWrite(PIN_TRIG,HIGH);delayMicroseconds(10);
  digitalWrite(PIN_TRIG,LOW);
  long d=pulseIn(PIN_ECHO,HIGH,30000);
  return d? d/58 : 999;
}
bool hayPaquete(){ return distanciaCM() < 24; }   // umbral calibrado: fondo 28, paquete min 8 cm
void mostrar(const String&a,const String&b){
  lcd.clear();lcd.setCursor(0,0);lcd.print(a.substring(0,20));
  lcd.setCursor(0,1);lcd.print(b.substring(0,20));
}
void reposo(){ estado=REPOSO;taqActiva=-1;paqueteVisto=false;mostrar("Nodo postal listo","C=deposito QR=recog"); }

void setup(){
  Serial.begin(115200);
  digitalWrite(PIN_RELE, HIGH);   // reposo antes de declarar salida (evita pulso de arranque)
  pinMode(PIN_RELE, OUTPUT);
  cerrar();
  pinMode(PIN_FINCAR,INPUT_PULLUP);
  pinMode(PIN_TRIG,OUTPUT);pinMode(PIN_ECHO,INPUT);
  lcd.init();lcd.backlight();
  for(uint8_t i=0;i<N;i++){taquillas[i].ocupada=false;taquillas[i].codigo="";}
  reposo(); tele("BOOT",-1,"listo");
}

void loop(){
  char t=teclado.getKey();
  procesarSerie();

  switch(estado){
    case REPOSO:
      if(t=='C'){
        int libre=buscarLibre();
        if(libre<0){ mostrar("Sin espacio","Todas ocupadas"); tele("ERR",-1,"lleno"); delay(1500); reposo(); break; }
        taqActiva=libre; estado=ESPERA_CODIGO; tEspera=millis();
        mostrar("Deposito taq "+String(libre+1),"Generando codigo...");
        tele("DEP_INI",libre,"");
      }
      break;

    case ESPERA_CODIGO:
      if(millis()-tEspera>TIMEOUT_COD){
        tele("ERR",taqActiva,"sin_codigo_PC");
        mostrar("Sin respuesta PC","Operacion anulada"); delay(1500); reposo();
      }
      break;

    case ABIERTA_DEPOSITO:
     if(hayPaquete()) paqueteVisto=true;
      if(millis()-tPuerta > 2000){          // espera de gracia de 2 s tras abrir
        if(paqueteVisto && puertaCerrada()){
          cerrar(); taquillas[taqActiva].ocupada=true;
          tele("DEP_OK",taqActiva,taquillas[taqActiva].codigo);
          mostrar("Deposito OK","Taq "+String(taqActiva+1)); delay(1500); reposo();
        } else if(millis()-tPuerta>TIMEOUT){
          cerrar(); taquillas[taqActiva].codigo=""; tele("ERR",taqActiva,"timeout_dep");
          mostrar("Tiempo agotado","Anulado"); delay(1500); reposo();
        }
      }
      break;

    case ABIERTA_RECOGIDA:
      if(millis()-tPuerta > 2000){            // espera de gracia de 2 s tras abrir
        if(!hayPaquete() && puertaCerrada()){
          cerrar(); taquillas[taqActiva].ocupada=false; taquillas[taqActiva].codigo="";
          tele("REC_OK",taqActiva,"");
          mostrar("Recogida OK","Taq "+String(taqActiva+1)); delay(1500); reposo();
        } else if(millis()-tPuerta>TIMEOUT){
          cerrar(); tele("ERR",taqActiva,"timeout_rec");
          mostrar("Tiempo agotado","Cierre la puerta"); delay(1500); reposo();
        }
      }
      break;
  }
}

void procesarSerie(){
  if(!Serial.available()) return;
  String linea=Serial.readStringUntil('\n'); linea.trim();
  if(linea.length()==0) return;

  if(linea.startsWith("SET:")){
    int p=linea.indexOf(':',4);
    if(p>0 && estado==ESPERA_CODIGO){
      int taq=linea.substring(4,p).toInt();
      String cod=linea.substring(p+1);
      if(taq==taqActiva){
        taquillas[taqActiva].codigo=cod;
        paqueteVisto=false;
        abrir(); tPuerta=millis(); estado=ABIERTA_DEPOSITO;
        mostrar("Taq "+String(taqActiva+1)+" abierta","Introduzca paquete");
        tele("DEP_COD",taqActiva,cod);
      }
    }
  }
  else if(linea.startsWith("QR:")){
    if(estado==REPOSO){
      String cod=linea.substring(3);
      int taq=buscarPorCodigo(cod);
      if(taq>=0){ taqActiva=taq; abrir(); tPuerta=millis(); estado=ABIERTA_RECOGIDA;
        mostrar("Taq "+String(taq+1)+" abierta","Retire paquete");
        tele("REC_INI",taq,cod); }
      else { mostrar("QR no valido","o taq vacia"); tele("ERR",-1,"qr_invalido"); delay(1500); reposo(); }
    }
  }
}
