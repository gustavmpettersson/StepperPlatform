#define DEBUG //Uncomment to get debugging in serial monitor
//#define USE_SLEEP  //Auto sleep after every move (not recommended, will lose synchronisation)
#define distFactor 1.6 //steps per µm

#define maxSpeedum 1000.0 //µm per sec
#define accelFactor 8 //is multiplied with maxSpeed to get acceleration in µm per sec2

#define stepsFromEnd 0 //Define home position this many steps from reference switches

#define antiBacklash 0 //Always move this many µm in the positive direction

#define LIGHT_ONLY_AT_SHUTTER //Only use the light when taking photo. If uncommented light on when system not sleeping.

#define LIGHT_DELAY 500 //milliseconds before/after shutter to keep light on

#define USE_CAMERA //After completed move do camera control task


