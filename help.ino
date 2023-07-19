String readGcode(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void readSerialString(String &str, char prefix, char suffix) {
  if (Serial.available()) {
    String msg = Serial.readStringUntil(suffix);
    Serial.flush();
    if (msg.startsWith(String(prefix))) {
      msg.remove(0, 1);
      str = msg;
    }
    else {
      str = "";
    }
    msg = "";
  }
}

bool digitalReadDeb(Bounce deb) {
  return deb.read();
}

void sendCoordinate(double &lwx, double &lwy, double &lwz,
                    double &wx, double &wy, double &wz) {
  if ((lwx != wx) || (lwy != wy) || (lwz != wz)) {
  /*  if (zeroIsSet == false) {
      if (wx < 0) {
        wx = 0;
      }
      if (wy < 0) {
        wy = 0;
      }
      if (wz < 0) {
        wz = 0;
      }
    }*/
    Serial.print("#CC" + String(wx) + "," + String(wy) + "," + String(wz) + ";");
    lwx = wx; lwy = wy; lwz = wz;
  }
}

void sendCoordinateWithInterval(unsigned long &tim, unsigned long intrvl,
                                double &lwx, double &lwy, double &lwz,
                                double &wx, double &wy, double &wz) {
  if (millis() - tim >= intrvl) {
    tim = millis();
    sendCoordinate(lwx, lwy, lwz, wx, wy, wz);
  }
}

void readRPM(long &lwrpm, long &wrpm) {
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil(';');
    Serial2.flush();
    if (msg.startsWith(String('&'))) {
      msg.remove(0, 1);
      if (msg.startsWith("CR")) {
        long newRpm = msg.substring(2).toInt();
        if (newRpm != lwrpm) {
          wrpm = newRpm;
        }
      }
    }
    msg = "";
  }
}

void sendRPM(long &lwrpm, long &wrpm) {
  readRPM(lwrpm, wrpm);
  if (millis() - timerRealTimeRpm >= RPM_INTERVAL) {
    timerRealTimeRpm = millis();
    if (wrpm != lwrpm) {
      Serial.print("#FF" + String(wrpm) + ";");
      lwrpm = wrpm;
    }
  }
}

unsigned long getDelay(unsigned long feedrate, int n) {
  unsigned long del;
  if (n == 1) {
    del = round((round((-2.471957294 * pow(10, -11) * feedrate * feedrate) - (25000 * feedrate) + 64500000) / (feedrate * 400)) / 2);
  }
  if (n == 2) {
    del = round(((round(11.11111111 * feedrate * feedrate - 46111.11111 * feedrate + 87277777.78) / (feedrate * 400)) / 2) / n);
  }
  if (n == 3) {
    del = round(((round(11.11111111 * feedrate * feedrate - 56111.11111 * feedrate + 108277777.8) / (feedrate * 400)) / 2) / n);
  }
  if (n == 4) {//for increment
    float jo = feedrate / (60*400);
    float ko = 1/jo;
    float del = ko/2; 
  }
  return del;
}

void interruptSerialManual(bool &f_rl) {
  if (Serial.available()) {
    String m = Serial.readStringUntil(';');
    Serial.flush();
    if (m.startsWith("@")) {
      m.remove(0, 1);
      if (m.startsWith("RL")) {
        f_rl = true;
      }
      m = "";
    }
  }
}

void interruptSerialHomingReset(bool &f_em) {
  if (Serial.available()) {
    String m = Serial.readStringUntil(';');
    Serial.flush();
    if (m.startsWith("@")) {
      m.remove(0, 1);
      if (m.startsWith("EM")) {
        f_em = true;
      }
      m = "";
    }
  }
}

void interruptSerialAuto(bool &f_ed, bool &f_pp, bool &f_pr) {
  if (Serial.available()) {
    String m = Serial.readStringUntil(';');
    Serial.flush();
    if (m.startsWith("@")) {
      m.remove(0, 1);
      if (m.startsWith("EM")) {
        f_ed = true;
      }
      if (m.startsWith("PS")) {
        f_pp = true;
      }
      if (m.startsWith("RM")) {
        f_pr = true;
      }
      m = "";
    }
  }
}
