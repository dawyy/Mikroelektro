/*
Projekt feladat Vincze Dávid

A projekt feladat annak bemutatására készült, hogy hogyan lehet az Arduino fejlesztői környezetben 
mérési adatokat bekérni szenzorokról, digitális bemenetekről, azokat LCD kijelzőn megjeleníteni és 
SD kártyán CSV formátumú adatfileban tárolni, ami importálható Excelbe. Továbbá bemutatja hogyan 
lehet bluetooth kommunikációs csatornán keresztül kezelni a kimeneteket. A rendszer állapota, és 
a működés közbeni események megjelennek a soros porton is.

A projekthez az Arduino Mega2560 típusát választottam, mivel ebben több a memória, így nem kell 
tartani a memória elfogyásából adódó problémáktól. A fejlesztőrendszer Arduino 2.0.2. A felhasznált
szoftverkomponensek az internetről szabadon letölthetők. A fejlesztés során felhasználtam az al-
katrészek és szoftverek leírásait és az interneten található oktatóanyagokat és leírásokat.

A felhasznált hardware elemek :
-------------------------------
- Arduino Mega2560
- DHT11 hő és páratartalommérő szenzor
- ACS712 árammérő Hall szenzor
- LCD kijelző 4 sor 20 oszlop
- I2C illesztő modul az LCD kijelzőhöz
- HC06 bluetooth modul
- Data logger shield SD kártya és RTC
- 2 CSATORNÁS RELÉ MODUL 5V DC
- breadboard, vezetékek, ellenállások, LEDek, kapcsolók, izzók
- 12V DC tápegység


*/

#include <dht11.h>  // dht11 szenzor könyvtár

#include "ACS712.h"  // Hall szenzor könyvtár

#include <RTClib.h> // RTC (valós idejű óra) modul könyvtár

#include <Wire.h>  // WIRE I2C könyvtár

#include <SPI.h>  // SPI kommunikáció könyvtár

#include <SD.h>  // SD kártya könyvtár

#include <LiquidCrystal_I2C.h>  // LCD I2C könyvtár

RTC_DS3231 rtc;  // RTC objektum neve rtc

#define DHT11PIN 8  //dht11 kimenet az Arduino 8. láb

dht11 DHT11;  //Szenzor objektum neve DHT11

LiquidCrystal_I2C lcd(0x27,20,4);  // I2C LCD beállítás : címe 0x27, 20 oszlop és 4 sor

ACS712 sensor(ACS712_05B, A1); // árammérő 5A-es típus az A1-ás analóg bemeneten

bool perc_valtas = false;  // percváltás detektálása az SD kártya íráshoz

int lcd_valt=1;  // számláló az LCD első sorába mit irjon ki 

char idobelyeg [20] ;  // időbélyeg az adttároláshoz SD kártyán

char ido [10] ;  // idő a kijelzéshez

int mp ;  // másodperc

int perc ;  //perc

int mp_seged ;  // másodperc segédváltozó

int perc_seged ;  // perc segédváltozó

int mp_szamlalo =0;  // változó a másodpercek számolásához

int soros_szamlalo = 19;  // időzítő számláló a soros port írásához

float I = 0; // áram változó

const int U = 12;  //feszültség konstans 12 V DC

int homerseklet=0;  // hőmérséklet változó

int paratartalom =0;  // páratartalom változó

float teljesitmeny=0; // teljesítmény változó

char BT_in_char;  //bluetooth bejövő karakter

bool D_in_1, D_in_2, D_in_3, D_in_4 ;  // digitális bemenetek változói boolean

int D1_pin = 23;  // 1. digitális bemenet a D23 láb

int D2_pin = 25;  // 2. digitális bemenet a D25 láb

int D3_pin = 27;  // 3. digitális bemenet a D27 láb

int D4_pin = 29;  // 4. digitális bemenet a D29 láb

int rele1 =  6;  // Relé1 kimenet a D6 láb

int rele2 =  7;  // Relé2 kimenet a D7 láb

String R1_status = "KI";  // 1. Relé alaphelyzetben kikapcsolva kiíráshoz

String R2_status = "KI";  // 2. Relé alaphelyzetben kikapcsolva kiíráshoz

void setup()  // beállítások induláskor
{

  Wire.begin();  // I2C inicializálás  

  lcd_inicializalas();  // LCD inicializálás, ékezetes betűk definiálása 

  lcd.clear();  // LCD törlés
  lcd.setCursor(0,0);
  lcd.print("    Vincze D\010vid    ");  // induló kijelzés
  lcd.setCursor(0,2);
  lcd.print("      S E T U P    ");  // induló kijelzés

  Serial.begin(9600);  // soros kapcsolat beállítása
  
  SD_inicializalas();  // SD kártya inicializálás

  RTC_inicializalas();  // RTC inicializálás

  sensor.calibrate();  // ACS712 szenzor kalibrálás

  Serial1.begin(9600);  // bluetooth soros port ( serial1 TX1(18), RX1(19)) beállítása
  
  pinMode(D1_pin, INPUT);  // D2 láb bemenet

  pinMode(D2_pin, INPUT);  // D3 láb bemenet

  pinMode(D3_pin, INPUT);  // D4 láb bemenet

  pinMode(D4_pin, INPUT);  // D5 láb bemenet

  pinMode(rele1, OUTPUT);  // D6 láb kimenet

  digitalWrite(rele1,LOW);  // 1. relé alaphelyzetben kikapcsolva

  pinMode(rele2, OUTPUT);  // D7 láb kimenet

  digitalWrite(rele2,LOW);  // 2. relé alaphelyzetben kikapcsolva

  delay ( 1000);  // 1 mp szünet

  lcd.clear();  // LCD törlés

}

void loop()  // főciklus
{
  ido_feladatok();  // idővel és időzítésekkel kapcsolatos feladatok
  
  ho_paratartalom_meres();  // hőmérséklet és páratartalom mérés

  teljesitmeny_meres();  // teljesítmény mérés

  digitalis_bemenetek_beolvasasa();  // digtális bemenetek beolvasása

  HC06_bluetooth();  // HC06 bluetooth feladat

  kimenet_kezeles();  // kimenetek kezelése

  if (perc_valtas)
    {
    // hogy ne legyen túl nagy az adat.csv file csak percenkeént ír bele
      Serial.println("SD írás");
      SD_iras();  // adatsor írás az SD kártyára
      perc_valtas=false;
    }

   soros_szamlalo ++;
   if (soros_szamlalo==20) // hogy olvasható legyen a kiírás, kb 4 másodprcenként frissül a rendszer állapot
    {
      soros_kiiras();  // adatok kiírása a soros portra
      soros_szamlalo=0;  // soros számláló nullázás
    }

  lcd_kiiras();  // adatok kiírása az LCD kijelzőre

  delay(100);  // 0,1 másodperc szünet 

}

void ho_paratartalom_meres()  // hőfok és páratartalom mérés
{
  int chk = DHT11.read(DHT11PIN);  // adatok beolvasása a DHT11 szenzorból

  paratartalom = (DHT11.humidity);  // Páratartalom kiolvasása a DHT11 szenzorból

  homerseklet = (DHT11.temperature);  // Hőmérséklet kiolvasása a DHT11 szenzorból

}

void teljesitmeny_meres()  //Teljesítménymérés
{
  I = sensor.getCurrentDC();  // egyenfeszültség (DC) áram beolvasása a Hall szenzorból
//  I = sensor.getCurrentAC();  // váltófeszültség (AC) áram beolvasása a Hall szenzorból
  if (I < 0.008) // figyelmen kívül hagyja, ha az áram 0.008A-nál kevesebb (teszteléssel megállapított érték)
    {
      I = 0;
    }
    teljesitmeny = (U*I);  // teljesítmény számolása 230 V AC feszültséggel számolva
}

void digitalis_bemenetek_beolvasasa()  // digtális bemenetek beolvasása
{
  D_in_1 = digitalRead(D1_pin);  // 1. digitális bemenet beolvasása
  D_in_2 = digitalRead(D2_pin);  // 2. digitális bemenet beolvasása
  D_in_3 = digitalRead(D3_pin);  // 3. digitális bemenet beolvasása
  D_in_4 = digitalRead(D4_pin);  // 4. digitális bemenet beolvasása
}

void soros_kiiras()  // rendszerállapot kiírása a soros vonalra
{
  Serial.println("--------------- Rendszer állapot ------------------");
  Serial.println(idobelyeg);
  Serial.print("Paratartalom (%): ");
  Serial.println(paratartalom);  // Páratartalom kiírása a soros vonalra
  Serial.print("Homerseklet (C): ");
  Serial.println(homerseklet);  // Hőmérséklet kiírása a soros vonalra
  Serial.print("Teljesítmeny : ");
  Serial.print((teljesitmeny));
  Serial.println(" W");  // teljesítmény kiírása a soros vonalra
  Serial.print("Rele1  : ");
  Serial.println(R1_status);
  Serial.print("Rele2  : "); 
  Serial.println(R2_status); 
  Serial.print("Digitális bemenetek : ");
  Serial.print(D_in_1);
  Serial.print(D_in_2);
  Serial.print(D_in_3);
  Serial.println(D_in_4 );
}

void lcd_inicializalas()  // LCD inicializálás és az ékezetes karakterek definiálása
{
  lcd.init();       // LCD inicializálás
  lcd.backlight();  // háttérvilágítás be
  //kis ékezetes betűk
  byte a1[8] = {B10, B100, B1110, B1, B1111, B10001, B1111};           //á
  byte e1[8] = {B10, B100, B1110, B10001, B11111, B10000, B1110};      //é
  byte i1[8] = {B10, B100, B0, B1110, B100, B100, B1110};              //í
  byte o1[8] = {B100, B100, B0, B1110, B10001, B10001, B1110};         //ó
  byte o2[8] = {B1010, B0, B1110, B10001, B10001, B10001, B1110};      //ö
  byte o3[8] = {B1010, B1010, B0000, B1110, B10001, B10001, B1110};    //ő
  byte u1[8] = {B0010, B0100, B10001, B10001, B10001, B10011, B1101};  //ú
  byte u2[8] = {B1010, B0, B0, B10001, B10001, B10011, B1101};         //ü
  byte u3[8] = {B1010, B1010, B0, B10001, B10001, B10011, B1101};      //ű
  
    //Ékezetes karakterek definiálása (ö-nek nincs hely!
  {
  lcd.createChar(0, a1); //á
  lcd.createChar(1, e1); //é
  lcd.createChar(2, i1); //í
  lcd.createChar(3, o1); //ó
  lcd.createChar(4, o3); //ő
  lcd.createChar(5, u1); //ú
  lcd.createChar(6, u2); //ü
  lcd.createChar(7, u3); //ű
  lcd.begin(20, 4); //4x20 karakteres LCD
  }
}

void lcd_kiiras()  // kiírás az LCD kijelzőre
{
// LCD első sorának kezelése
  lcd.setCursor(0,0);  // LDC kurzor 0. sor 1. oszlop
  lcd.print("Id\004 : ");  // RTC idő kiírása
  lcd.print(ido);
  lcd.print("            ");
  // LCD harmadik sorának kezelése, digitális bemenetek
  lcd.setCursor(0, 2);
  lcd.print("D-be 1:");  // digitális bemenetek állapotának kiírása
  lcd.print(D_in_1);  // 1.digitális bemenet állapotának kiírása
  lcd.print(" 2:");
  lcd.print(D_in_2);  // 2.digitális bemenet állapotának kiírása
  lcd.print(" 3:");
  lcd.print(D_in_3);  // 3.digitális bemenet állapotának kiírása
  lcd.print(" 4:");
  lcd.print(D_in_4);  // 4.digitális bemenet állapotának kiírása

// LCD negyedik sorának kezelése, reléállapotok
  lcd.setCursor(0, 3);
  lcd.print("Rel\0011 ");
  lcd.print(R1_status);  // R1 relé állapot

  lcd.setCursor(10, 3);
  lcd.print("Rel\0012 ");
  lcd.print(R2_status);  // R2 relé állapot
  
  switch (lcd_valt)  // a második sor tartalma az lcd_valt értékétől függ, analóg mérések
  {
    case 1:  // hőmérséklet
        lcd.setCursor(0,1);  // LDC kurzor 2. sor 1. oszlop
        lcd.print ("H\004m\001rs:\001klet:");
        lcd.print(homerseklet);
        lcd.print(" C  "); 
        break;
    case 2:  // páratartalom
        lcd.setCursor(0,1);  // LDC kurzor 2. sor 1. oszlop
        lcd.print ("P\010ratartalom:");
        lcd.print(paratartalom);
        lcd.print(" %   ");
        break;
    case 3:  // teljesítmény
        lcd.setCursor(0,1);  // LDC kurzor 2. sor 1. oszlop
        lcd.print("Teljes\002tm\001ny:");
        lcd.print(teljesitmeny);
        lcd.print("W");
        break;
  }
}

void HC06_bluetooth()  // bluetooth feladat. Serial1-ről karakter beolvasás
{
  BT_in_char = Serial1.read();  // egy karakter beolvasása a HC06 bluetooth modulból, ha van vett karakter
}

void kimenet_kezeles()  // ha van értelmezhető karakter, akkor kimenet kezelés és az aktuális idő és állapot kiírása a soros portra
{
 //relé1 kezelése
  if(BT_in_char == 'a')  // ha a karakter "a" rele1 bekapcsol
{
  digitalWrite(rele1,HIGH);
  R1_status = "BE";
  Serial.print(" Időbélyeg ");
  Serial.println(idobelyeg);
  Serial.print("Bluetooth vett karakter : ");
  Serial.println(BT_in_char);
  Serial.print("Relé1  : ");
  Serial.println(R1_status);
}
  if(BT_in_char == 'b')  // ha a karakter "b" rele1 kikapcsol
  {
    digitalWrite(rele1,LOW);
    R1_status = "KI";
    Serial.print(" Időbélyeg ");
    Serial.println(idobelyeg);
    Serial.print("Bluetooth vett karakter : ");
    Serial.println(BT_in_char);
    Serial.print("Relé1  : ");
    Serial.println(R1_status);
  }
//relé2 kezelése
  if(BT_in_char == 'c')  // ha a karakter "c" rele2 bekapcsol
  {
    digitalWrite(rele2,HIGH);
    R2_status = "BE";
    Serial.print(" Időbélyeg ");
    Serial.println(idobelyeg);
    Serial.print("Bluetooth vett karakter : ");
    Serial.println(BT_in_char);
    Serial.print("Relé2  : "); 
    Serial.println(R2_status);
  }
  if(BT_in_char == 'd')  // ha a karakter "d" rele2 kikapcsol
  { 
    digitalWrite(rele2,LOW);
    R2_status = "KI";
    Serial.print(" Időbélyeg ");
    Serial.println(idobelyeg);
    Serial.print("Bluetooth vett karakter : ");
    Serial.println(BT_in_char);
    Serial.print("Relé2  : "); 
    Serial.println(R2_status);
  }
delay(10);
}

void SD_inicializalas()  // SD kártya inicializálása
{
  if (!SD.begin(10,11,12,13))  // az Arduino Mega2560 miatt meg kell adni a Datalogger shield által használt lábakat
    {
      Serial.println(" SD kártya nincs, vagy nem inicializálható");  // ha nincs SD kátya, vagy nem inicializálható
      lcd.clear();
      lcd.print(" SD hiba ! ! ! !");  /// hiba esetén kilép
      Serial.println(" SD hiba ! ! ! !");
      exit(1);  // kilépés 
    }
    // ha az SD kártya rendben van, akkor ellenőrzi az adatfile meglétét, ha nincs létrehozza

    if (!SD.exists("adat.csv"))  // Ha nem létezik az adat.csv file, akkor létrehozza és beleírja a fejlécet
      {
        File adatfile = SD.open("adat.csv", FILE_WRITE);
        adatfile.println("Időbélyeg,Hőmérséklet,Páratartalom,Teljesítmény,Relé1,Relé2,Digit1,Digit2,Digit3,Digit4");  //fejléc kiírása a fileba
        adatfile.close();  // adatfile bezárása
      }
}

void SD_iras()  // SD kártya írása soronként
{
  // adatfile megnyitása
  Serial.println (" SD kártya írás kezdet"); // kiírás a soros portra

  File adatfile = SD.open("adat.csv", FILE_WRITE);

  if (adatfile) 
    {
      adatfile.print(idobelyeg);  //időbélyeg írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      Serial.print(" Időbélyeg "); // időbélyeg kiírása a soros portra
      Serial.println(idobelyeg);
      adatfile.print(homerseklet);  //hőmérséklet írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.print(paratartalom);  //páratartalom írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.print(teljesitmeny);  //teljesítmény írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.print(R1_status);  //Relé1 állapot írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.print(R2_status);  //Relé2 állapot írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.print(D_in_1);  // 1. digitális bemenet állapot írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.print(D_in_2);  // 2. digitális bemenet állapot írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.print(D_in_3);  // 3. digitális bemenet állapot írása az SD kártyára
      adatfile.print(",");  //mezőelválasztó "," írása
      adatfile.println(D_in_4);  // 4. digitális bemenet állapot írása az SD kártyára

      adatfile.close();  //adatfile lezárása
      Serial.println(" SD kártya írás vége"); // kiírás a soros portra
      delay(50);  // 50 ms szünet
    }
    else
    {
      Serial.println(" Adat.csv nem írható");  // ha az adat.csv kátya írható
      lcd.clear();
      lcd.print(" SD hiba ! ! ! !");  // ha nincs meg az adatfile
      Serial.println(" SD hiba ! ! ! !");
      exit(1);  // kilépés 
    }
}

void RTC_inicializalas()  // RTC inicializálása
{
  rtc.begin();
}

void ido_feladatok()  // idővel és időzítéssel kapcsolatos feladatok
{
  DateTime now = rtc.now();

  sprintf(idobelyeg, "%02d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second() );  // időbélyeg előállítása ééé/hh/nn ÓÓ:pp:mp formátumban
  sprintf(ido, "%02d:%02d:%02d" , now.hour(), now.minute(), now.second());  // idő előállítása a kijelzéshez  óó:pp:mp formátumban

  perc = now.minute(); // aktuális perc kiolvasása az RTC-ből
  mp = now.second(); // aktuális másodperc kiolvasása az RTC-ből

  if(perc != perc_seged)  // percváltás detektálása
  {
    perc_valtas = true;
    perc_seged = perc;
  }
  if (mp != (mp_seged))  // 5 másodpercenként váltja az LCD első sorában a kijelzést ciklikusan 
    mp_szamlalo ++;  
    if ( mp_szamlalo == 20 )
      {
        mp_szamlalo=0;
        {
          lcd_valt ++;
          if (lcd_valt > 3)
            lcd_valt = 1;
        }
   }
}
