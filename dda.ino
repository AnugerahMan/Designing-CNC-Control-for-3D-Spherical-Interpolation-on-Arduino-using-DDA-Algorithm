void linear3d(double x0,double y0,double z0,double x1,double y1,double z1,int fr){
  
   double deltaX = (x1-x0);
   double deltaY = (y1-y0);
   double deltaZ = (z1-z0);  
   double dx = abs(deltaX);
   double dy = abs(deltaY);
   double dz = abs(deltaZ);

   int gx = (x1>x0)? 1: -1;
   int dirx = (x0 == x1) ? 0 : gx; 
   int gy = (y1>y0)? 1: -1;
   int diry = (y0 == y1) ? 0 : gy;
   int gz = (z1>z0)? 1: -1;
   int dirz = (z0 == z1) ? 0 : gz;
 
   double steps0 = (dx > dy)? dx : dy;
   double steps1 = (steps0 > dz)? steps0 : dz;
   
   //Serial.print("steps1 =");
   //Serial.println(steps1);
   
   double x_inc = deltaX/steps1;  
   double y_inc = deltaY/steps1;
   double z_inc = deltaZ/steps1;
   sublinear3d(x0,x1,y0,y1,z0,z1,dirx,diry,dirz,x_inc,y_inc,z_inc,fr);
     
 }
 
 void sublinear3d(double xa,double xb,double ya,double yb,double za,double zb,int dirx,int diry,
                    int dirz,double incx,double incy,double incz,int fr){

                  
         int k;
         double adder_X = xa; double float_to_round_X; 
         double adder_Y = ya; double float_to_round_Y; 
         double adder_Z = za; double float_to_round_Z; 
     while(xa != xb || ya != yb || za != zb) { //|| ya != yb || za != zb || ya != yb || za != zb 

         
         d_SW_EMG.update(); // break loop
         if(digitalRead(SW_EMG) && !F_emg) {  
          Serial.print("break emergency");
          F_emg = true;
           break;
          }

         interruptSerialAuto(F_emgDummy,F_prePause,F_preResume);
         d_SW_EMG.update();
         if (digitalRead(SW_EMG) || F_emgDummy || F_emgBcsOffset && !F_emg) {
            Serial.print("#EG;");
            F_emgDummy = false;
            F_emg = true;
            break;
          }
          
         if(F_prePause){
            if(F_auto && !F_pause){
              F_pause = true;
            }
           F_prePause = false;
         }

         if(F_preResume){
            if(F_auto && F_pause){
              F_pause = false;
            }
            F_preResume = false;
         }

          if(!F_pause){
              if(xa != xb ){
                unsigned long outerdly = getDelay(fr, 1);
                 // n_axis = n_axis + 1;             
                  adder_X += incx; //          double adder_Y = ya; double float_to_round_Y;
                  float_to_round_X = round(adder_X);
                  if(float_to_round_X != xa){
                    increment("X",dirx,outerdly);
                    xa = float_to_round_X;
                    workingX = workingX + (0.01*dirx);
                  }        
               }
              if(ya != yb){ 
                unsigned long outerdly = getDelay(fr, 1);
                  //n_axis = n_axis + 1;
                  adder_Y += incy;
                  float_to_round_Y = round(adder_Y);
                  if(float_to_round_Y != ya){
                    increment("Y",diry,outerdly);
                    ya = float_to_round_Y;
                    workingY = workingY + (0.01*diry);
                  }
               }
              if(za != zb){ 
                unsigned long outerdly = getDelay(fr, 1);
                //  n_axis = n_axis + 1;
                  adder_Z += incz;
                  float_to_round_Z = round(adder_Z);
                  if(float_to_round_Z != za){
                    increment("Z",dirz,outerdly);
                    za = float_to_round_Z;
                    workingZ = workingZ + (0.01*dirz); 
                  }
                  
               }
              sendCoordinateWithInterval(timerRealTimeCoordinate, COORDINATE_INTERVAL,
                               lastWorkingX, lastWorkingY, lastWorkingZ,
                               workingX, workingY, workingZ);
             
            }                                      
  }
}
