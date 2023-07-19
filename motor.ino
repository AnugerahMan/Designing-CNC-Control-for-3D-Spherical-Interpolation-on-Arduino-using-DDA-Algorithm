void increment(String axis,int stepp, int fr){
 
  if(axis == "X"){
    boolean dir = (stepp == 1) ? CCW : CW;
    for(int i = 0; i < 4; i++){ 
      StepperX.move(dir,fr);
    }
  }

  if(axis == "Y"){
    boolean dir = (stepp == 1) ? CW : CCW;
    for(int i = 0; i < 4; i++){
      StepperY.move(dir,fr);
    }
  }

  if(axis == "Z"){
    boolean dir = (stepp == 1) ? CW : CCW;
    for(int i = 0; i < 4; i++){
      StepperZ.move(dir,fr);
    }
  } 
}
