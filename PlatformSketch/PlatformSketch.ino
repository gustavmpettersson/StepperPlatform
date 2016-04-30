#include <AccelStepper.h>
#include "BoardSetup_V1_0.cpp"
#include "Settings.cpp"

#define maxSpeed maxSpeedum*distFactor //steps per sec
#define maxAccel maxSpeed*accelFactor //steps per sec2

//Debug printing
#ifdef DEBUG
  #define PRINT Serial.println
#else
  #define PRINT
#endif

//Define some steppers and the pins the will use
AccelStepper stepperX(1,xStepPin,xDirPin);
AccelStepper stepperY(1,yStepPin,yDirPin);

//System move states
#define WAITING 0
#define PRIMARY 1
#define SECONDARY 2
#define HOMING 3
#define HOME_SECONDARY 4

//Reference switches
#define hitXRef !digitalRead(xRefPin) //True if we hit the x min endstop
#define hitYRef !digitalRead(yRefPin) //True if we hit the y min endstop
boolean oldXRef = hitXRef; //Used to detect rising and falling edge
boolean oldYRef = hitYRef; //Used to detect rising and falling edge

//System variables
long setX = 0;
long setY = 0;
boolean homedX = true;
boolean homedY = true;
boolean goingToHome = false;
int systemState = WAITING; //Initially we do nothing!

void setup() {  
  Serial.begin(9600);

  //Pin use definitions
  pinMode(xSleepPin,OUTPUT);
  pinMode(ySleepPin,OUTPUT);
  pinMode(xRefPin,INPUT);
  pinMode(yRefPin,INPUT);
  pinMode(xMinPin,INPUT_PULLUP);
  pinMode(xMaxPin,INPUT_PULLUP);
  pinMode(yMinPin,INPUT_PULLUP);
  pinMode(yMaxPin,INPUT_PULLUP);
  pinMode(stopButtonPin,INPUT_PULLUP);
  pinMode(cameraPin,OUTPUT);
  pinMode(lightPin,OUTPUT);
  pinMode(LEDPin,OUTPUT);

  //Prepare system
  digitalWrite(cameraPin, LOW);
  digitalWrite(lightPin, LOW);
  digitalWrite(LEDPin, HIGH);
  sleepMotors();
  delay(1000);

  //Prepare serial
  while (!Serial){} //Wait for serial port to open
  Serial.println("<ARDUINO IS LIVE>");
  PRINT("<<<IN DEBUG MODE>>>");
  PRINT("<<<-------------------------------------------------->>>");
  digitalWrite(LEDPin,LOW);
}

/*this function is run contionously as the arduino is running*/
void loop() {
    /* Homing procedure */
    if (systemState == HOMING) {
      if (hitXRef && !homedX){ //Found home x
        stepperX.setCurrentPosition(0); //define as zero
        stepperX.moveTo(0); //stop
        homedX = true;
      }
      if (hitYRef && !homedY) { //Found home y
        stepperY.setCurrentPosition(0); //define as zero
        stepperY.moveTo(0); //stop
        homedY = true;
      }
      if (homedX && homedY) {
        PRINT("HOMED PRIMARY");
        systemState = HOME_SECONDARY;
        digitalWrite(LEDPin,HIGH);
        stepperY.setCurrentPosition(0);
        stepperY.setCurrentPosition(0);
        setMove(1000,1000,true);
        homedX = false;
        homedY = false;
      }
    }

    else if (systemState == HOME_SECONDARY) {
      if (!hitXRef && !homedX) { //Just got off the reference switch
        homedX = true;
        stepperX.setCurrentPosition(-stepsFromEnd); //define home
        stepperX.moveTo(-stepsFromEnd); //stop
        setX = 0;
        PRINT("HOMED X");
      }
      if (!hitYRef && !homedY) { //Just got off the reference switch
        homedY = true;
        stepperY.setCurrentPosition(-stepsFromEnd); //define home
        stepperY.moveTo(-stepsFromEnd); //stop
        setY = 0;
        PRINT("HOMED Y");
      }
      if (homedX && homedY) {
        Serial.println("HOMED");
        systemState = SECONDARY;
        goingToHome = true;
        digitalWrite(LEDPin,HIGH);
        setMove(0,0,true); //Go to our newly defined home
      }
      
    }

    /*read the endstops, stop if hit*/
    if (hitEndstop()) { 
      Serial.println("HIT ENDSTOP, STOPPING");
      stop();
    }
    
    /*read the emergency stop button, if pressed stop; else go*/
    else if (!digitalRead(stopButtonPin)) { //Emergency stop button pressed, set desired position to current
      Serial.println("STOP BUTTON PRESSED!");
      stop();
    }
    
    /*else continue as normal*/
    else{
      
      /*if we are running and in a move state, keep running*/
      if ( (stepperX.isRunning() || stepperY.isRunning())
         && (systemState == PRIMARY || systemState == SECONDARY || systemState == HOMING || systemState == HOME_SECONDARY) ) {  
        stepAll();
      }
      
      /*if we are no longer running, but still in PRIMARY state, we must just have finished the primary move; start secondary*/
      else if (systemState == PRIMARY) {  //If we just stopped our primary move
        systemState = SECONDARY;
        digitalWrite(LEDPin,HIGH);
        setMove(setX,setY,true); //Now move to the desired position
        PRINT("Doing sencondary move");
      }

      /*if we are no longer running, but still in SECONDAY state, we must just have finished the move, stop running*/
      else if (systemState == SECONDARY) { //If we just stopped our secondary move
        systemState = WAITING;
        digitalWrite(LEDPin,LOW);
        #ifdef USE_SLEEP
        delay(100);
        sleepMotors();
        #endif
        Serial.println("MOVED");
        PRINT("<<<-------------------------------------------------->>>");
        if (true) { //(!goingToHome) { //Do not take photo after homing
          #ifdef USE_CAMERA
          digitalWrite(lightPin,HIGH);
          delay(LIGHT_DELAY);
          digitalWrite(cameraPin,HIGH);
          delay(100);
          digitalWrite(cameraPin,LOW);
          #ifdef LIGHT_ONLY_AT_SHUTTER
          delay(LIGHT_DELAY);
          digitalWrite(lightPin,LOW);
          #endif
          #endif
        }
        goingToHome = false;
      }

      /*if we are not in any of the above states we must be WAITING, so see if there is any serial commands*/
      else if (Serial.available()) { //If we have got serial data to us, and are not currently running
        delay(100); //Wait to make sure the whole message has been buffered
        String input = Serial.readString(); //Save message
        PRINT("Received string:");
        PRINT(input);
        String split[5];
        int index_end;
        int index_start = 0;
        int i = 0;
        //Split message to its parts (space separated)
        while (input.indexOf(" ",index_start) >0) {  //While there is another space available in the string
          index_end = input.indexOf(" ",index_start); //Save the value of it
          split[i++] = input.substring(index_start,index_end); //Add the substring to split
          index_start = index_end+1; //Change the starting index
        }
        split[i] = input.substring(index_start);  //When there is no more spaces, add the rest (from last space to end)
        
        #ifdef DEBUG
        PRINT("Parsed string to:");
        for (String str : split){
          PRINT(str);
        }
        PRINT("End parse.");
        #endif
  
        newCommand(split); 
      }
    }
} // end loop

/*this stops the motors and then sleeps*/
void stop() {
  setMove(stepperX.currentPosition()/distFactor, stepperY.currentPosition()/distFactor,false);
  sleepMotors();
  systemState = WAITING;
  digitalWrite(LEDPin,LOW);
  Serial.println("SYSTEM WAS STOPPED!");
  while (true) {
    //FREEZE THE SYSTEM SOMETHING IS WRONG
  }
}

/*disables motor outputs*/
void sleepMotors() {
  digitalWrite(xSleepPin,LOW);
  digitalWrite(ySleepPin,LOW);
  digitalWrite(lightPin,LOW);
}

/*reenables motors*/
void wakeMotors() {
  digitalWrite(xSleepPin,HIGH);
  digitalWrite(ySleepPin,HIGH);
  #ifndef LIGHT_ONLY_AT_SHUTTER
  digitalWrite(lightPin,HIGH);
  #endif
}

/*acts on the new command sent via serial*/
void newCommand(String split[]) {
  if (split[0].equalsIgnoreCase("MOVE")) {  //We got a move command!
    setX = split[1].toInt();
    setY = split[2].toInt();
    setMove(setX-antiBacklash, setY-antiBacklash,true);
    systemState = PRIMARY; //Set state to do primary move
    digitalWrite(LEDPin,HIGH);
    Serial.println("MOVING");
  }
  else if (split[0].equalsIgnoreCase("INCR")) { //Do a small incremental move
    setX += split[1].toInt();
    setY += split[2].toInt();
    setMove(setX, setY,true);
    systemState = SECONDARY; //Set state to do secondary move
    digitalWrite(LEDPin,HIGH);
    Serial.println("INCREMENTING");
  }
  else if (split[0].equalsIgnoreCase("SLEEP")) { //Time to sleep motors
    sleepMotors();
    Serial.println("SLEEPING");
    PRINT("<<<-------------------------------------------------->>>");
    digitalWrite(lightPin,LOW);
  }
  else if (split[0].equalsIgnoreCase("HOME")) { //We got a home command
    wakeMotors();
    digitalWrite(LEDPin,HIGH);
    if (hitXRef) { //We are already on the reference switch, must move away!
      stepperX.move(10000);
      stepperX.setMaxSpeed(1000);
      stepperX.setAcceleration(8000);
      PRINT("MOVING OFF X REFERENCE");
      while (hitXRef) {
        if (hitEndstop() || !digitalRead(stopButtonPin)) {
          stop();
        }
        stepperX.run();
      }
    }

    if (hitYRef) { //We are already on the reference switch, must move away!
      stepperY.move(10000);
      stepperY.setMaxSpeed(1000);
      stepperY.setAcceleration(8000);
      PRINT("MOVING OFF Y REFERENCE");
      while (hitYRef) {
         if (hitEndstop() || !digitalRead(stopButtonPin)) {
          stop();
        }
        stepperY.run();
      }
    }
    
    setMove(-35000,-35000,true); //Run both motors untill they hit the reference (or max 30mm if something is broken)
    systemState = HOMING;
    homedX = false;
    homedY = false;
    Serial.println("HOMING");
    PRINT("<<<-------------------------------------------------->>>");
  }  
  else { //Did not understand the message
    Serial.println("UNDEFINED");
  }
}

/*calculates and sets the motors desired position in steps; also calculates speeds to move in synch*/
void setMove(long x_um, long y_um, boolean wake) {
  if (wake) {
    wakeMotors();
  }

  PRINT("SET MOVE TO (um):");
  PRINT(x_um);
  PRINT(y_um);
  
  long x = (long)(((float)x_um)*distFactor+0.5); //Round to integer number of steps
  long y = (long)(((float)y_um)*distFactor+0.5);

  PRINT("SET MOVE TO (steps):");
  PRINT(x);
  PRINT(y);
  
  long deltaX = x-stepperX.currentPosition(); //How much to move in x (steps)
  long deltaY = y-stepperY.currentPosition(); //How much to move in y (steps)
  
  float norm = sqrt( (float)deltaX*(float)deltaX + (float)deltaY*(float)deltaY);

  float maxSpeedX = (float)abs(deltaX)/norm *maxSpeed;
  float maxSpeedY = (float)abs(deltaY)/norm *maxSpeed;
  if (maxSpeedX < 500) { maxSpeedX = 500;} //Minimum speed set to 500
  if (maxSpeedY < 500) { maxSpeedY = 500;}
  stepperX.setMaxSpeed(maxSpeedX);
  stepperY.setMaxSpeed(maxSpeedY);
  stepperX.setAcceleration(maxSpeedX*accelFactor);
  stepperY.setAcceleration(maxSpeedY*accelFactor);

  PRINT("Speed set to (steps):");
  PRINT(maxSpeedX);
  PRINT(maxSpeedY);

  stepperX.moveTo(x);
  stepperY.moveTo(y);
}

/*does another step, if one is waiting*/
boolean stepAll() {
  stepperX.run();
  stepperY.run();
}

boolean hitReference() {
  boolean toReturn = false;
  //Test X min
  if ( !oldXRef && hitXRef ) { //Endstop just changed to high! (it was hit)
    oldXRef = true;
    toReturn = true;
    PRINT("HIT X REFERENCE");
  } else if ( oldXRef && !hitXRef ) { //Endstop just changed to low! (we moved away)
    oldXRef = false;
    PRINT("LEFT X REFERENCE");
  }
  //Test Y min
  if ( !oldYRef && hitYRef ) { //Endstop just changed to high! (it was hit)
    oldYRef = true;
    toReturn = true;
    PRINT("HIT Y REFERENCE");
  } else if ( oldYRef && !hitYRef ) { //Endstop just changed to low! (we moved away)
    oldYRef = false;
    PRINT("LEFT Y REFERENCE");
  }
  return toReturn;
}

boolean hitEndstop() {
  boolean hit = (digitalRead(xMinPin) || digitalRead(xMaxPin)) || (digitalRead(yMinPin) || digitalRead(yMaxPin));
  if (hit) {
    Serial.println("HIT ENDSTOP, STOPPING");
    stop();
  }
  return hit;
}





