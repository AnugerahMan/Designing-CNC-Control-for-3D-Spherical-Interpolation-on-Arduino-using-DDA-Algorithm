#include <StepperPA.h>
#include <Bounce2.h>



#define EN_X 23
#define DIR_X 25
#define CLK_X 27
#define EN_Y 29
#define DIR_Y 31
#define CLK_Y 33
#define EN_Z 35
#define DIR_Z 37
#define CLK_Z 41
#define LS_XMIN 22
#define LS_XMAX 24
#define LS_YMIN 26
#define LS_YMAX 28
#define LS_ZMIN 30
#define LS_ZMAX 32
#define LAMP_RED 38
#define LAMP_YELLOW 36
#define LAMP_GREEN 34
#define SW_EMG 40

//#define RAPID_SPEED 800
//#define DEFAULT_RPM 4000
//#define DEBOUNCE_INTERVAL 10
//#define COORDINATE_INTERVAL 353
//#define RPM_INTERVAL 497

Bounce d_SW_EMG = Bounce();
Bounce d_LS_XMIN = Bounce();
Bounce d_LS_XMAX = Bounce();
Bounce d_LS_YMIN = Bounce();
Bounce d_LS_YMAX = Bounce();
Bounce d_LS_ZMIN = Bounce();
Bounce d_LS_ZMAX = Bounce();

StepperPA StepperX(EN_X, DIR_X, CLK_X);
StepperPA StepperY(EN_Y, DIR_Y, CLK_Y);
StepperPA StepperZ(EN_Z, DIR_Z, CLK_Z);

const unsigned long RAPID_SPEED = 1000;
const unsigned int DEFAULT_RPM = 4000;
const unsigned long DEBOUNCE_INTERVAL = 10;
const unsigned long COORDINATE_INTERVAL = 353;
const unsigned long RPM_INTERVAL = 497;

/* MAIN */
String serialMsg, serial2Msg;
unsigned long timerRealTimeCoordinate = 0;
unsigned long timerRealTimeRpm = 0;
unsigned long timerSerial2 = 0;
/* MANUAL VARS */
bool F_xp, F_xm, F_yp, F_ym, F_zp, F_zm;
unsigned long dlyManual;
/* AUTO VARS */
int currentIndexAuto;
int setPointAuto;
bool withSpindle;
bool F_auto, F_pause, F_autoDone;
bool F_prePause, F_preResume;
bool F_G28XY, F_G28Z, F_G28ZJustDone, F_G28ZM13JustDone;
/* FLAGS */
bool F_emg, F_emgDummy, F_emgBcsOffset, F_kirimSinyalEMG;
bool F_homing, F_reset, F_init, F_EGAuto;
/* DATA */
double workingX = 0, workingY = 0, workingZ = 0;
double lastWorkingX, lastWorkingY, lastWorkingZ;
long workingRpm = 0;
long lastWorkingRpm = 0;
bool F_spindleCWing, F_spindleCCWing;
bool F_spindleRotatingManual;
bool zeroIsSet = false;

void setup() {
  Serial.begin(57600);
  Serial2.begin(115200);  

  pinMode(SW_EMG, INPUT_PULLUP);
  d_SW_EMG.attach(SW_EMG); d_SW_EMG.interval(DEBOUNCE_INTERVAL);

  pinMode(LS_XMIN, INPUT_PULLUP); pinMode(LS_XMAX, INPUT_PULLUP);
  d_LS_XMIN.attach(LS_XMIN); d_LS_XMIN.interval(DEBOUNCE_INTERVAL);
  d_LS_XMAX.attach(LS_XMAX); d_LS_XMAX.interval(DEBOUNCE_INTERVAL);

  pinMode(LS_YMIN, INPUT_PULLUP); pinMode(LS_YMAX, INPUT_PULLUP);
  d_LS_YMIN.attach(LS_XMIN); d_LS_YMIN.interval(DEBOUNCE_INTERVAL);
  d_LS_YMAX.attach(LS_XMAX); d_LS_YMAX.interval(DEBOUNCE_INTERVAL);

  pinMode(LS_ZMIN, INPUT_PULLUP); pinMode(LS_ZMAX, INPUT_PULLUP);
  d_LS_ZMIN.attach(LS_XMIN); d_LS_ZMIN.interval(DEBOUNCE_INTERVAL);
  d_LS_ZMAX.attach(LS_XMAX); d_LS_ZMAX.interval(DEBOUNCE_INTERVAL);

  pinMode(LAMP_RED, OUTPUT); pinMode(LAMP_YELLOW, OUTPUT); pinMode(LAMP_GREEN, OUTPUT);
}

void loop() {
  
    if (!F_auto){digitalWrite(LAMP_RED, LOW);
    digitalWrite(LAMP_GREEN, LOW);
    digitalWrite(LAMP_YELLOW, HIGH);
    }
  /* ---------------------------------------------------------------- */
  /*                            SERIAL HANDLER                        */
  /* ---------------------------------------------------------------- */
  if (!digitalRead(SW_EMG)) { // emergency tidak ditekan - maka baca terus serial, dan manual tidak aktif
    readSerialString(serialMsg, '@', ';');
    
  }

  /* ---------------------------------------------------------------- */
  /*                               FLAG EMG                           */
  /* ---------------------------------------------------------------- */
  if (digitalRead(SW_EMG) && !F_emg) { // emergency ditekan - flag pre emergency belum aktif - bisa karena noise bisa aktual
    F_emg = true;
    F_kirimSinyalEMG = false;
  }
  if (serialMsg.startsWith("EM") && !F_emg) { // emergency pada interface ditekan
    F_emg = true;
    F_kirimSinyalEMG = false;
  }
  if (F_emg) {
    digitalWrite(LAMP_RED, HIGH);
    digitalWrite(LAMP_GREEN, LOW);
    digitalWrite(LAMP_YELLOW, LOW);
    if (!F_EGAuto) { // kalau bukan emergency dari auto
      if (!F_kirimSinyalEMG) { // dan kalao belum kirim sinyal emg, maka kirim
        Serial2.print("?EG;");
        Serial.print("#EG;");
        F_kirimSinyalEMG = true;
      }
    }
    F_EGAuto = false;
    F_homing = false;
    F_reset = false;
    F_auto = false;
  }

  /* ---------------------------------------------------------------- */
  /*                              FLAG RESET                          */
  /* ---------------------------------------------------------------- */
  if (serialMsg.startsWith("RS")) { // flag emergency harus sedang aktif dan tombol emergency tidak ditekan
    if (F_emg && !digitalRead(SW_EMG)) {
      digitalWrite(LAMP_RED, LOW);
      digitalWrite(LAMP_GREEN, LOW);
      digitalWrite(LAMP_YELLOW, HIGH);
      F_reset = true;
      F_emg = false;
      F_auto = false;
      F_homing = false;
    }
    serialMsg = "";
  }

  /* ---------------------------------------------------------------- */
  /*                             FLAG HOMING                          */
  /* ---------------------------------------------------------------- */
  if (serialMsg.startsWith("HM") || serialMsg.startsWith("HI")) { // flag emergency harus tidak aktif
    if (!F_emg) {
      digitalWrite(LAMP_GREEN, HIGH);
      digitalWrite(LAMP_RED, LOW);
      digitalWrite(LAMP_YELLOW, LOW);
      F_homing = true;
      if (serialMsg.startsWith("HI")) {
        F_init = true;
      }
    }
    serialMsg = "";
  }

  /* ---------------------------------------------------------------- */
  /*                      HOMING AND RESET FUNCTION                   */
  /* ---------------------------------------------------------------- */
  if (F_homing || F_reset) {
homingZ:
    if (((F_reset && F_spindleCWing) || (F_reset && F_spindleCCWing)) && workingZ < 0) { // jika reset saat auto, maka
      if ((F_spindleCWing) || (F_spindleCCWing)) {
        Serial2.print("?MT;"); // spindle ON dengan PID di titik akhir for 2 sec dengan dir sebaliknya
      }
      delay(2000);
    }
    while (digitalRead(LS_XMIN) && digitalRead(LS_YMIN) && digitalRead(LS_ZMAX)) { // lakukan moving stepper z saat ls z belum aktif
      interruptSerialHomingReset(F_emg);
      if (!digitalRead(LS_ZMAX) || digitalRead(SW_EMG) || F_emg) {
        if (digitalRead(SW_EMG) || F_emg) {
          F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
        }
        StepperZ.stop(); break;
      }
      else {
        unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 1);
        StepperZ.move(CW, dly); workingZ -= 0.0025;
        if (!F_init) {
          sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                     lastWorkingX, lastWorkingY, lastWorkingZ,
                                     workingX, workingY, workingZ);
        }
      }
    }
    if (!digitalRead(LS_ZMAX)) { // saat ls z sudah aktif
      if (((F_reset && F_spindleCWing) || (F_reset && F_spindleCCWing)) && workingZ > 0) {
        Serial2.print("?MT;"); // stop spindle
        F_spindleCWing = false; F_spindleCCWing = false;
      }
      StepperZ.stop(); // maka stop stepper z
      while (digitalRead(LS_XMIN) && digitalRead(LS_YMIN)) { // lakukan moving stepper x dan y berbarengan saat ls x dan y belum aktif
        if (digitalRead(LS_ZMAX)) { // ls z belum aktif
          goto homingZ;
        }
        interruptSerialHomingReset(F_emg);
        if (!digitalRead(LS_XMIN) || !digitalRead(LS_YMIN) || digitalRead(SW_EMG) || F_emg) { // break loop jika salah satu step ada yang sudah mencapai limitnya
          if (digitalRead(SW_EMG) || F_emg) {
            F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;"); StepperX.stop(); StepperY.stop();
          }
          if (!digitalRead(LS_XMIN)) {
            StepperX.stop();
          }
          if (!digitalRead(LS_YMIN)) {
            StepperY.stop();
          }
          break;
        }
        else {
          unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 2);
          StepperX.move(CW, dly); workingX -= 0.0025;
          StepperY.move(CCW, dly); workingY -= 0.0025;
          if (!F_init) {
            sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                       lastWorkingX, lastWorkingY, lastWorkingZ,
                                       workingX, workingY, workingZ);
          }
        }
      }
      while (digitalRead(LS_XMIN)) {  // lakukan moving stepper x saat ls x belum aktif
        if (digitalRead(LS_ZMAX)) { // ls z belum aktif
          goto homingZ;
        }
        interruptSerialHomingReset(F_emg);
        if (!digitalRead(LS_XMIN) || digitalRead(SW_EMG) || F_emg) {
          if (digitalRead(SW_EMG) || F_emg) {
            F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
          }
          StepperX.stop(); break;
        }
        else {
          unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 1);
          StepperX.move(CW, dly); workingX -= 0.0025;
          if (!F_init) {
            sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                       lastWorkingX, lastWorkingY, lastWorkingZ,
                                       workingX, workingY, workingZ);
          }
        }
      }
      while (digitalRead(LS_YMIN)) { // lakukan moving stepper x saat ls x belum aktif
        if (digitalRead(LS_ZMAX)) { // ls z belum aktif
          goto homingZ;
        }
        //interruptSerialHomingReset(F_emg);
        if (!digitalRead(LS_YMIN) || digitalRead(SW_EMG) || F_emg) {
          if (digitalRead(SW_EMG) || F_emg) {
            F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
          }
          StepperY.stop(); break;
        }
        else {
          unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 1);
          StepperY.move(CCW, dly); workingY -= 0.0025;
          if (!F_init) {
            sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                       lastWorkingX, lastWorkingY, lastWorkingZ,
                                       workingX, workingY, workingZ);
          }
        }
      }
      if (!digitalRead(LS_XMIN) && !digitalRead(LS_YMIN) && !digitalRead(LS_ZMAX)) {
        digitalWrite(LAMP_GREEN, LOW);
        digitalWrite(LAMP_RED, LOW);
        digitalWrite(LAMP_YELLOW, HIGH);
        if (zeroIsSet == false) {
          workingX = 0; workingY = 0; workingZ = 0;
        }
        F_homing = false; F_reset = false; F_auto = false; F_emg = false;
        F_xp = false; F_xm = false; F_yp = false; F_ym = false; F_zp = false; F_zm = false;
        serialMsg = "";
      }
    }
    if (F_reset) {
      F_emgBcsOffset = false;
      F_prePause = false;
      F_preResume = false;
      F_pause = false;
    }
    if (F_init) { // connecting button handler, reset variable
      Serial.print("#0D;");
      workingX = 0; workingY = 0; workingZ = 0;
      workingRpm = 0;
      lastWorkingX = NULL; lastWorkingY = NULL; lastWorkingZ = NULL;
      lastWorkingRpm = NULL;
      zeroIsSet = false;
      F_init = false;
    }
  }
  

  /* ---------------------------------------------------------------- */
  /*                            MANUAL STEPPER                        */
  /* ---------------------------------------------------------------- */
  if (serialMsg.startsWith("X+")) {
    if (digitalRead(LS_XMAX) && !F_emg && !F_homing && !F_reset && !F_auto) {
      digitalWrite(LAMP_YELLOW, LOW); F_xm = false; F_yp = false; F_ym = false; F_zp = false; F_zm = false;
      unsigned long feedrate = serialMsg.substring(2).toInt(); dlyManual = getDelay(feedrate, 1); F_xp = true;
    }
    serialMsg = "";
  }
  while (F_xp && digitalRead(LS_XMAX) && !F_emg && !F_homing && !F_reset && !F_auto) {
    bool F_release = false; interruptSerialManual(F_release);
    if (F_release || !digitalRead(LS_XMAX) || digitalRead(SW_EMG)) {
      if (digitalRead(SW_EMG)) {
        F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
      }
      StepperX.stop(); F_xp = false;
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ); break;
    }
    else {
      for (int j = 0; j < 4; j++) {
        StepperX.move(CCW, dlyManual);
        digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
      }
      workingX += 0.01;
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                 lastWorkingX, lastWorkingY, lastWorkingZ,
                                 workingX, workingY, workingZ);
    }
  }

  if (serialMsg.startsWith("X-")) {
    if (digitalRead(LS_XMIN) && !F_emg && !F_homing && !F_reset && !F_auto) {
      digitalWrite(LAMP_YELLOW, LOW);  F_xp = false; F_yp = false; F_ym = false; F_zp = false; F_zm = false;
      unsigned long feedrate = serialMsg.substring(2).toInt(); dlyManual = getDelay(feedrate, 1); F_xm = true;
    }
    serialMsg = "";
  }
  while (F_xm && digitalRead(LS_XMIN) && !F_emg && !F_homing && !F_reset && !F_auto) {
    bool F_release = false; interruptSerialManual(F_release);
    if (F_release || !digitalRead(LS_XMIN) || digitalRead(SW_EMG)) {

      if (digitalRead(SW_EMG)) {
        F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
      }
      StepperX.stop(); F_xm = false;
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ); break;
    }
    else {
      for (int j = 0; j < 4; j++) {
        digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
        StepperX.move(CW, dlyManual);
      }
      workingX -= 0.01;
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                 lastWorkingX, lastWorkingY, lastWorkingZ,
                                 workingX, workingY, workingZ);
    }
  }

  if (serialMsg.startsWith("Y+")) {
    if (digitalRead(LS_YMAX) && !F_emg && !F_homing && !F_reset && !F_auto) {
      digitalWrite(LAMP_YELLOW, LOW);  F_xm = false; F_xp = false; F_ym = false; F_zp = false; F_zm = false;
      unsigned long feedrate = serialMsg.substring(2).toInt(); dlyManual = getDelay(feedrate, 1); F_yp = true;
    }
    serialMsg = "";
  }
  while (F_yp && digitalRead(LS_YMAX) && !F_emg && !F_homing && !F_reset && !F_auto) {
    bool F_release = false; interruptSerialManual(F_release);
    if (F_release || !digitalRead(LS_YMAX) || digitalRead(SW_EMG)) {
      
      if (digitalRead(SW_EMG)) {
        F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
      }
      StepperY.stop(); F_yp = false;
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ); break;
    }
    else {
      for (int j = 0; j < 4; j++) {
        digitalWrite(LAMP_YELLOW, LOW);
        digitalWrite(LAMP_GREEN, HIGH);
        StepperY.move(CW, dlyManual);
      }
      workingY += 0.01;
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                 lastWorkingX, lastWorkingY, lastWorkingZ,
                                 workingX, workingY, workingZ);
    }
  }

  if (serialMsg.startsWith("Y-")) {
    if (digitalRead(LS_YMIN) && !F_emg && !F_homing && !F_reset && !F_auto) {
      digitalWrite(LAMP_YELLOW, LOW); F_xp = false; F_xm = false; F_yp = false; F_zp = false; F_zm = false;
      unsigned long feedrate = serialMsg.substring(2).toInt(); dlyManual = getDelay(feedrate, 1); F_ym = true;
    }
    serialMsg = "";
  }
  while (F_ym && digitalRead(LS_YMIN) && !F_emg && !F_homing && !F_reset && !F_auto) {
    bool F_release = false; interruptSerialManual(F_release);
    if (F_release || !digitalRead(LS_YMIN) || digitalRead(SW_EMG)) {
      
      if (digitalRead(SW_EMG)) {
        F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
      }
      StepperY.stop(); F_ym = false;
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ); break;
    }
    else {
      for (int j = 0; j < 4; j++) {
        digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
        StepperY.move(CCW, dlyManual);
      }
      workingY -= 0.01;
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                 lastWorkingX, lastWorkingY, lastWorkingZ,
                                 workingX, workingY, workingZ);
    }
  }

  if (serialMsg.startsWith("Z-")) {
    if (digitalRead(LS_ZMIN) && !F_emg && !F_homing && !F_reset && !F_auto) {
      digitalWrite(LAMP_YELLOW, LOW);  F_xp = false; F_xm = false; F_yp = false; F_ym = false; F_zm = false;
      unsigned long feedrate = serialMsg.substring(2).toInt(); dlyManual = getDelay(feedrate, 1); F_zp = true;
    }
    serialMsg = "";
  }
  while (F_zp && digitalRead(LS_ZMIN) && !F_emg && !F_homing && !F_reset && !F_auto) {
    bool F_release = false; interruptSerialManual(F_release);
    if (F_release || !digitalRead(LS_ZMIN) || digitalRead(SW_EMG)) {
      
      if (digitalRead(SW_EMG)) {
        F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
      }
      StepperZ.stop(); F_zp = false;
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ); break;
    }
    else {
      for (int j = 0; j < 4; j++) {
      digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
        StepperZ.move(CCW, dlyManual);
      }
      workingZ -= 0.01;
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                 lastWorkingX, lastWorkingY, lastWorkingZ,
                                 workingX, workingY, workingZ);
    }
  }

  if (serialMsg.startsWith("Z+")) {
    if (digitalRead(LS_ZMAX) && !F_emg && !F_homing && !F_reset && !F_auto) {
      digitalWrite(LAMP_YELLOW, LOW); F_xp = false; F_xm = false; F_yp = false; F_ym = false; F_zp = false;
      unsigned long feedrate = serialMsg.substring(2).toInt(); dlyManual = getDelay(feedrate, 1); F_zm = true;
    }
    serialMsg = "";
  }
  while (F_zm && digitalRead(LS_ZMAX) && !F_emg && !F_homing && !F_reset && !F_auto) {
    bool F_release = false; interruptSerialManual(F_release);
    if (F_release || !digitalRead(LS_ZMAX) || digitalRead(SW_EMG)) {
      
      if (digitalRead(SW_EMG)) {
        F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
      }
      StepperZ.stop(); F_zm = false;
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ); break;
    }
    else {
      for (int j = 0; j < 4; j++) {
        digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
        StepperZ.move(CW, dlyManual);
      }
      workingZ += 0.01;
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                 lastWorkingX, lastWorkingY, lastWorkingZ,
                                 workingX, workingY, workingZ);
    }
  }

  /* ---------------------------------------------------------------- */
  /*                            MANUAL SPINDLE                        */
  /* ---------------------------------------------------------------- */
  if (serialMsg.startsWith("PC")) { // spindle CW with PID
    if (!F_emg && !F_homing && !F_reset && !F_auto) {
      String setPoint = serialMsg.substring(2);
      Serial2.print("?CW" + setPoint + ";"); // ccw aktual
      F_spindleCWing = true;
      F_spindleRotatingManual = true;
    }
    serialMsg = "";
  }
  if (serialMsg.startsWith("PW")) { // spindle CCW with PID
    if (!F_emg && !F_homing && !F_reset && !F_auto) {
      String setPoint = serialMsg.substring(2);
      Serial2.print("?CC" + setPoint + ";"); // CW aktual
      F_spindleCCWing = true;
      F_spindleRotatingManual = true;
    }
    serialMsg = "";
  }
  if (serialMsg.startsWith("SC")) { // spindle CW
    if (!F_emg && !F_homing && !F_reset && !F_auto) {
      String setPoint = serialMsg.substring(2);
      Serial.print(setPoint);
      Serial2.print("?VW" + setPoint + ";"); // CCW AKTUAL
      F_spindleCWing = true;
      F_spindleRotatingManual = true;
    }
    serialMsg = "";
  }
  if (serialMsg.startsWith("SW")) { // spindle CCW
    if (!F_emg && !F_homing && !F_reset && !F_auto) {
      String setPoint = serialMsg.substring(2);
      Serial2.print("?VC" + setPoint + ";"); // CW AKTUAL
      F_spindleCCWing = true;
      F_spindleRotatingManual = true;
    }
    serialMsg = "";
  }
  if (serialMsg.startsWith("SM")) { // spindle OFF
    if (!F_emg && !F_homing && !F_reset && !F_auto) {
      Serial2.print("?MT;");
      F_spindleCWing = false; F_spindleCCWing = false;
      F_spindleRotatingManual = false;
    }
    serialMsg = "";
  }
  if (F_spindleRotatingManual) {
    sendRPM(lastWorkingRpm, workingRpm);
  }

  /* ---------------------------------------------------------------- */
  /*                            MANUAL CONTROL                        */
  /* ---------------------------------------------------------------- */
  if (!F_emg && !F_homing && !F_reset && !F_auto) {
    if (serialMsg.startsWith("SO")) { // set origin
      zeroIsSet = true;
      double originX = workingX, originY = workingY, originZ = workingZ;
      workingX = 0; workingY = 0; workingZ = 0;
      Serial.print("#OS" + String(originX) + "," + String(originY) + "," + String(originZ) + ";");
      serialMsg = "";
    }
    if (serialMsg.startsWith("GO")) { // go to origin

      if (zeroIsSet == true) {
         digitalWrite(LAMP_YELLOW, LOW);
         digitalWrite(LAMP_GREEN, HIGH);
        unsigned long feedrate = RAPID_SPEED;
        long x0 = round(workingX * 100), y0 = round(workingY * 100), z0 = round(workingZ * 100);
        long x1 = 0, y1 = 0, z1 = 0;
        linear3d(x0, y0, z0, x1, y1, z1, feedrate);
        workingX = 0; workingY = 0; workingZ = 0;
        sendRPM(lastWorkingRpm, workingRpm);
        sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                       workingX, workingY, workingZ);
      }
      serialMsg = "";
    }
    if (serialMsg.startsWith("SA")) { // semi-auto absolute
      digitalWrite(LAMP_YELLOW, LOW);
      String params = serialMsg.substring(2);
      long x0 = round(workingX * 100), y0 = round(workingY * 100), z0 = round(workingZ * 100);
      long x1 = round(readGcode(params, ',', 0).toDouble() * 100),
           y1 = round(readGcode(params, ',', 1).toDouble() * 100),
           z1 = round(readGcode(params, ',', 2).toDouble() * 100);
      unsigned long feedrate = readGcode(params, ',', 3).toInt();
      linear3d(x0, y0, z0, x1, y1, z1, feedrate);
      workingX = readGcode(params, ',', 0).toDouble();
      workingY = readGcode(params, ',', 1).toDouble();
      workingZ = readGcode(params, ',', 2).toDouble();
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ);
      serialMsg = "";
    }
    if (serialMsg.startsWith("SI")) { // semi-auto increment
      digitalWrite(LAMP_YELLOW, LOW);
      String params = serialMsg.substring(2);
      double befWorkingX = workingX, befWorkingY = workingY, befWorkingZ = workingZ;
      long x0 = 0, y0 = 0, z0 = 0;
      long x1 = round(readGcode(params, ',', 0).toDouble() * 100),
           y1 = round(readGcode(params, ',', 1).toDouble() * 100),
           z1 = round(readGcode(params, ',', 2).toDouble() * 100);
      unsigned long feedrate = readGcode(params, ',', 3).toInt();
      linear3d(x0, y0, z0, x1, y1, z1, feedrate);
      workingX = befWorkingX + readGcode(params, ',', 0).toDouble();
      workingY = befWorkingY + readGcode(params, ',', 1).toDouble();
      workingZ = befWorkingZ + readGcode(params, ',', 2).toDouble();
      sendRPM(lastWorkingRpm, workingRpm);
      sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                     workingX, workingY, workingZ);
      serialMsg = "";
    }
  }

  /* ---------------------------------------------------------------- */
  /*                           RESET COORDINATE                       */
  /* ---------------------------------------------------------------- */
  if (!digitalRead(LS_XMIN)) {
    if (zeroIsSet == false) {
      workingX = 0; sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                                   workingX, workingY, workingZ);
    }
  }
  if (!digitalRead(LS_YMIN)) {
    if (zeroIsSet == false) {
      workingY = 0; sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                                   workingX, workingY, workingZ);
    }
  }
  if (!digitalRead(LS_ZMIN)) {
    if (zeroIsSet == false) {
      workingZ = 0; sendCoordinate(lastWorkingX, lastWorkingY, lastWorkingZ,
                                   workingX, workingY, workingZ);
    }
  }
  if (!digitalRead(LS_XMIN) && !digitalRead(LS_YMIN) && !digitalRead(LS_ZMIN)) { // ls x, y, z aktif
    digitalWrite(LAMP_YELLOW, HIGH);
  }
  else {
    digitalWrite(LAMP_YELLOW, LOW);
  }

  /* ---------------------------------------------------------------- */
  /*                                 AUTO                             */
  /* ---------------------------------------------------------------- */
  if (serialMsg.startsWith("AB")) { // init auto
    if (!F_emg && !F_homing && !F_reset && !F_auto) {
      digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
      F_auto = true;
      setPointAuto = DEFAULT_RPM;
      currentIndexAuto = 0;
      sendRPM(lastWorkingRpm, workingRpm);
      Serial.print("!" + String(currentIndexAuto) + ";");
      sendRPM(lastWorkingRpm, workingRpm);
    }
    serialMsg = "";
  }
  if (serialMsg.startsWith("WE")) { // G28 HOMING
    if (!F_emg && !F_homing && !F_reset && F_auto) {
       digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
      String params = serialMsg.substring(2);
      if (params.startsWith("ZD")) {
        F_G28Z = true;
      }
      if (params.startsWith("XY")) {
        F_G28XY = true;
      }
      sendRPM(lastWorkingRpm, workingRpm);
    }
    serialMsg = "";
  }
  if (F_G28XY) {
     digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
    while (digitalRead(LS_XMIN) && digitalRead(LS_YMIN)) {
      interruptSerialHomingReset(F_emg);
      if (!digitalRead(LS_XMIN) || !digitalRead(LS_YMIN) || digitalRead(SW_EMG) || F_emg) {
        if (digitalRead(SW_EMG) || F_emg) {
          F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;"); StepperX.stop(); StepperY.stop();
        }
        if (!digitalRead(LS_XMIN)) {
          StepperX.stop();
        }
        if (!digitalRead(LS_YMIN)) {
          StepperY.stop();
        }
        break;
      }
      else {
        unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 2);
        StepperX.move(CW, dly); workingX -= 0.0025;
        StepperY.move(CCW, dly); workingY -= 0.0025;
        sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                   lastWorkingX, lastWorkingY, lastWorkingZ,
                                   workingX, workingY, workingZ);
      }
    }
    while (digitalRead(LS_XMIN)) {
      interruptSerialHomingReset(F_emg);
      if (!digitalRead(LS_XMIN) || digitalRead(SW_EMG) || F_emg) {
        if (digitalRead(SW_EMG) || F_emg) {
          F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
        }
        StepperX.stop(); break;
      }
      else {
        unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 1);
        StepperX.move(CW, dly); workingX -= 0.0025;
        sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                   lastWorkingX, lastWorkingY, lastWorkingZ,
                                   workingX, workingY, workingZ);
      }
    }
    while (digitalRead(LS_YMIN)) {
      interruptSerialHomingReset(F_emg);
      if (!digitalRead(LS_YMIN) || digitalRead(SW_EMG) || F_emg) {
        if (digitalRead(SW_EMG) || F_emg) {
          F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
        }
        StepperY.stop(); break;
      }
      else {
        unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 1);
        StepperY.move(CCW, dly); workingY -= 0.0025;
        sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                   lastWorkingX, lastWorkingY, lastWorkingZ,
                                   workingX, workingY, workingZ);
      }
    }
    if (!digitalRead(LS_XMIN) && !digitalRead(LS_YMIN)) {
      currentIndexAuto++;
      Serial.print("!" + String(currentIndexAuto) + ";");
      F_G28XY = false;
    }
  }
  if (F_G28Z) {
     digitalWrite(LAMP_YELLOW, LOW);
      digitalWrite(LAMP_GREEN, HIGH);
    while (digitalRead(LS_ZMAX)) {
      interruptSerialHomingReset(F_emg);
      if (!digitalRead(LS_ZMAX) || digitalRead(SW_EMG) || F_emg) {
        if (digitalRead(SW_EMG) || F_emg) {
          F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
        }
        StepperZ.stop(); break;
      }
      else {
        unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 1);
        StepperZ.move(CW, dly); workingZ -= 0.0025;
        sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                   lastWorkingX, lastWorkingY, lastWorkingZ,
                                   workingX, workingY, workingZ);
      }
    }
    if (!digitalRead(LS_ZMAX)) {
      currentIndexAuto++;
      Serial.print("!" + String(currentIndexAuto) + ";");
      F_G28Z = false;
      F_G28ZJustDone = true;
    }
  }


  if (serialMsg.startsWith("EP")) { // @ITGn,x0,y0,z0,x1,y1,z1,f,i,j,k,dir;
     digitalWrite(LAMP_YELLOW, LOW);
     digitalWrite(LAMP_GREEN, HIGH);
    if (!F_emg && !F_homing && !F_reset && F_auto) {
      String params = serialMsg.substring(2);
      String modeG = readGcode(params, ',', 0);
      int feedrate = readGcode(params, ',', 7).toInt();
      if (modeG.equals("G0") && feedrate == -1) { // feedrate G0
        feedrate = RAPID_SPEED;
      }
      if (modeG.equals("G0") || modeG.equals("G1")) {
        long x0 = round(readGcode(params, ',', 1).toDouble() * 100),
             y0 = round(readGcode(params, ',', 2).toDouble() * 100);
        long z0 = round(readGcode(params, ',', 3).toDouble() * 100);
        long x1 = round(readGcode(params, ',', 4).toDouble() * 100),
             y1 = round(readGcode(params, ',', 5).toDouble() * 100);
        long z1 = round(readGcode(params, ',', 6).toDouble() * 100);
        if (F_G28ZM13JustDone && (z0 != z1)) {
          z0 = round(workingZ * 100);
          F_G28ZM13JustDone = false;
        }
        if (!digitalRead(SW_EMG)) { // emergency tidak ditekan
          linear3d(x0, y0, z0, x1, y1, z1, feedrate);
          if (!F_emg) { // jika interpolasi selesai bukan karena emergency
            currentIndexAuto++;
            Serial.print("!" + String(currentIndexAuto) + ";");
            
          }
          else { // emergency aktif saat interpolasi
            F_autoDone = true;
          }
        }
        else { // emergency ditekan
          F_autoDone = true;
        }
      }
      sendRPM(lastWorkingRpm, workingRpm);
    }
    serialMsg = "";
  }
  if (serialMsg.startsWith("OC")) { // @ETM3 atau @ETM30 atau @ETSP,3000
    if (!F_emg && !F_homing && !F_reset && F_auto) {
      digitalWrite(LAMP_YELLOW, LOW);
     digitalWrite(LAMP_GREEN, HIGH);
      Serial.print(serialMsg);
      String params = serialMsg.substring(2);
      if (params.startsWith("SP") || params.equals("M3") || params.equals("M4") ||
          params.equals("M13") || params.equals("M14") || params.equals("M5")) {
        if (params.startsWith("SP")) {
          setPointAuto = readGcode(params, ',', 1).toInt();
        }
        if (params.equals("M3") || params.equals("M4") || params.equals("M13") || params.equals("M14")) { // spindle on
          if (params.equals("M3") || params.equals("M13")) { // CW
            Serial2.print("?CC" + String(setPointAuto) + ";");
            F_spindleCWing = true;
            if (F_G28ZJustDone) {
              F_G28ZM13JustDone = true;
              F_G28ZJustDone = false;
            }
          }
          if (params.equals("M4") || params.equals("M14")) { // CCW
            Serial2.print("?CW" + String(setPointAuto) + ";");
            F_spindleCCWing = true;
          }
          delay(2000);
        }
        if (params.equals("M5")) { // spindle off
          Serial2.print("?MT;");
          F_spindleCWing = false; F_spindleCCWing = false;
          delay(2000);
        }
        currentIndexAuto++;
        Serial.print("!" + String(currentIndexAuto) + ";");
      }
      if (params.equals("M2") || params.equals("M30")) { // program end
        while (digitalRead(LS_ZMAX)) {
          interruptSerialHomingReset(F_emg);
          if (!digitalRead(LS_ZMAX) || digitalRead(SW_EMG) || F_emg) {
            if (digitalRead(SW_EMG) || F_emg) {
              F_emg = true; F_kirimSinyalEMG = false; Serial2.print("?EG;"); Serial.print("#EG;");
            }
            StepperZ.stop(); break;
          }
          else {
            unsigned long feedrate = RAPID_SPEED; unsigned long dly = getDelay(feedrate, 1);
            StepperZ.move(CW, dly); workingZ -= 0.0025;
            sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                                       lastWorkingX, lastWorkingY, lastWorkingZ,
                                       workingX, workingY, workingZ);
          }
        }
        if (!digitalRead(LS_ZMAX)) {

          Serial2.print("?MT;");
          digitalWrite(LAMP_YELLOW, HIGH);
          digitalWrite(LAMP_GREEN, LOW);
         
          }
          F_spindleCWing = false; F_spindleCCWing = false;
          F_autoDone = true;
          F_G28Z = false;
          F_G28XY = false;
          delay(2000);
        }
        sendRPM(lastWorkingRpm, workingRpm);
      }
      serialMsg = "";
    }
    if (F_autoDone) {
      digitalWrite(LAMP_GREEN, LOW);
      digitalWrite(LAMP_YELLOW, HIGH);

      if (F_emg) { // complete karena emergency
        if (F_emgBcsOffset) {
          Serial.print("#GD2," + String(workingX) + "," + String(workingY) + "," + String(workingZ) + ";");
          F_emgBcsOffset = false;
        }
        else {
          Serial.print("#GD1," + String(workingX) + "," + String(workingY) + "," + String(workingZ) + ";");
        }
        Serial2.print("?EG;");
        F_EGAuto = true;
      }
      else { // complete
        Serial.print("#GD0," + String(workingX) + "," + String(workingY) + "," + String(workingZ) + ";");

      }
      F_auto = false;
      F_autoDone = false;
    }
  }
