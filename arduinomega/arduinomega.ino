#include <NewPing.h>
#include <Servo.h>
#include <AFMotor.h>

//hc-sr04 sensor
#define TRIGGER_PIN A2
#define ECHO_PIN A3
#define max_distance 50

//motor
#define MAX_SPEED 70
#define MAX_SPEED_OFFSET 20
#define TURN_SPEED 50  // Define this along with your other #define statements

// IR Sensors
#define LEFT_IR_PIN A13
#define RIGHT_IR_PIN A14
#define PANEL_SENSOR_COUNT 5
#define PANEL_SENSOR_START_PIN A8  // Starting pin for the panel sensors

bool waitForCommand = true;      // Initially, wait for a command
bool followLineEnabled = false;  // Line following is not active initially

// Global variables
unsigned long firstDetectionTime = 0;  // Time of the first black line pattern detection
int blackLineDetections = 0;           // Number of black line pattern detections

enum Destination { NONE,
                   LIBRARY,
                   OFFICE,
                   LECTURE_HALL,
                   CAFETERIA };
enum Action { FOLLOW_LINE,
              TURN_LEFT,
              TURN_RIGHT,
              STOP };

Destination currentDestination = NONE;
Action nextAction = FOLLOW_LINE;


Servo servo;
NewPing sonar(TRIGGER_PIN, ECHO_PIN, max_distance);

AF_DCMotor motor1(1, MOTOR12_1KHZ);
AF_DCMotor motor2(2, MOTOR12_1KHZ);
AF_DCMotor motor3(3, MOTOR34_1KHZ);
AF_DCMotor motor4(4, MOTOR34_1KHZ);

int distance = 0;
int leftDistance;
int rightDistance;
boolean object;

void setup() {
  Serial.begin(9600);   // For debug messages
  Serial3.begin(9600);  // Initialize Serial3 for ESP32 communication
  servo.attach(10);
  servo.write(90);

  pinMode(LEFT_IR_PIN, INPUT);
  pinMode(RIGHT_IR_PIN, INPUT);
  for (int i = 0; i < PANEL_SENSOR_COUNT; i++) {
    pinMode(PANEL_SENSOR_START_PIN + i, INPUT);
  }

  motor1.setSpeed(0);
  motor2.setSpeed(0);
  motor3.setSpeed(0);
  motor4.setSpeed(0);
}


void loop() {
  if (Serial3.available() > 0) {
    receiveAndActOnDirection();  // Process direction commands from the ESP32
  }

  // Perform autonomous behavior only if not waiting for a command
  /*if (!waitForCommand) {
    // Autonomous behavior (e.g., object avoidance)
    if (digitalRead(A0) == 0 && digitalRead(A1) == 0) {
      objectAvoid();
    } else if (digitalRead(A0) == 0 && digitalRead(A1) == 1) {
      Serial.println("TL");
      moveLeft();
    } else if (digitalRead(A0) == 1 && digitalRead(A1) == 0) {
      Serial.println("TR");
      moveRight();
    } else if (digitalRead(A0) == 1 && digitalRead(A1) == 1) {
      Stop();
    }
  }*/
}

void receiveAndActOnDirection() {
  String direction = Serial3.readStringUntil('\n');  // Read the incoming string until newline
  direction.trim();                                  // Trim any whitespace
  direction.toLowerCase();                           // Convert to lowercase to make the comparison case-insensitive

  Serial.print("Direction received: ");
  Serial.println(direction);

  // Handle different destinations
  if (direction.indexOf("library") != -1) {
    delay(1000);
    Serial.println("Moving towards the library...");
    moveToLibrary();
  } else if (direction.indexOf("office") != -1) {
    Serial.println("Moving towards the office...");
    moveToOffice();
  } else if (direction.indexOf("lecture_hall") != -1) {
    Serial.println("Moving towards the lecture hall...");
    moveToLectureHall();
  } else if (direction.indexOf("cafeteria") != -1) {
    Serial.println("Moving towards the cafeteria...");
    moveToCafeteria();
  }
  // You can add more else-if blocks for additional locations
}


void followLine() {
  bool leftSensor = digitalRead(LEFT_IR_PIN);
  bool rightSensor = digitalRead(RIGHT_IR_PIN);

  if (!leftSensor && !rightSensor) {
    // No black line detected by either sensor, continue moving forward
    moveForward(MAX_SPEED);
  } else if (leftSensor && !rightSensor) {
    // Left sensor detects black, suggesting a potential right turn is needed
    turnRight();
  } else if (!leftSensor && rightSensor) {
    // Right sensor detects black, suggesting a potential left turn is needed
    turnLeft();
  } else {
    // Both sensors detect black, indicating a special pattern on the ground
    // Move slightly forward to position the sensor panel over the pattern more accurately
    moveForward(MAX_SPEED);
    delay(200);  // Adjust this delay based on your robot's speed and sensor placement

    Stop();      // Stop to analyze the pattern
    delay(100);  // Allow some time for the robot to stabilize

    // Call detectLinePattern to read and act on the detected pattern
    detectLinePattern();
  }
}

void detectLinePattern() {
  unsigned long pattern = readIRPanelPattern();

  // Immediate reset if no destination is set to avoid unnecessary processing
  if (currentDestination == NONE) {
    resetDetectionVariables();
    return;
  }

  switch (currentDestination) {
    case LIBRARY:
      handleLibraryPath(pattern);
      break;
    case OFFICE:
      handleOfficePath(pattern);
      break;
    case LECTURE_HALL:
      handleLectureHallPath(pattern);
      break;
    case CAFETERIA:
      handleCafeteriaPath(pattern);
      break;
    default:
      // Possibly continue following the line or stop
      break;
  }
}


void handleLibraryPath(unsigned long pattern) {
  int zeroCount = countZerosInPattern(pattern);

  if (zeroCount >= 4) {
    // Correctly check and increment black line detections
    blackLineDetections++;  // Increment detection count on every detection of black line

    // Check if it's time to turn left (after detecting two black lines consecutively)
    if (blackLineDetections >= 1 && nextAction == TURN_LEFT) {
      Serial.println("Turning left towards the library.");
      turnLeft();
      delay(1000);  // Adjust delay based on your turn duration
      // After turning left, we expect the next action to possibly stop if another black line is detected
      nextAction = STOP;
      blackLineDetections = 0;  // Set next action to anticipate stopping
    }
    // After turning, if another black line is detected, stop
    else if (blackLineDetections >= 1 && nextAction == STOP) {
      Serial.println("Destination reached.");
      Stop();
      collectFeedback();
      delay(1000000);
      currentDestination = NONE;  // Reset destination after arrival
      blackLineDetections = 0;    // Reset detections for future navigations
    }
  }
}

void returnFromLibrary() {
    turnAround(); // Perform a 180-degree turn to face back towards the start

    // Reset black line detections and next action for the return journey
    blackLineDetections = 0;
    nextAction = TURN_RIGHT; // Assuming the first action will be to turn right when coming back

    // Flag to indicate that the car is on its return journey
    bool returning = true;

    // Continue the loop until reaching the starting point
    while (returning) {
        unsigned long pattern = readIRPanelPattern();
        int zeroCount = countZerosInPattern(pattern);

        if (zeroCount >= 4) {
            blackLineDetections++;  // Increment detection count on every detection of a black line
            
            if (blackLineDetections >= 1 && nextAction == TURN_RIGHT) {
                Serial.println("Turning right to return.");
                turnRight();
                delay(1000); // Adjust delay based on your turn duration
                nextAction = STOP; // Set to stop if another black line is detected
                blackLineDetections = 0; // Reset for next detection
            } else if (blackLineDetections >= 1 && nextAction == STOP) {
                Serial.println("Starting point reached.");
                Stop();
                returning = false; // End the loop, the car has returned
            }
        }
        delay(100); // Small delay to continuously check for black lines
    }
}


void handleOfficePath(unsigned long pattern) {
  int zeroCount = countZerosInPattern(pattern);

  // Check for significant black line pattern indicating decision points
  if (zeroCount >= 4) {     // Adjust based on the pattern expected for black lines
    blackLineDetections++;  // Increment detection count on every detection of black line

    // First decision point: Turn right after detecting two black lines
    if (blackLineDetections == 2 && nextAction == TURN_RIGHT) {
      Serial.println("Turning right towards the office.");
      turnRight();
      delay(1000);  // Adjust delay based on your turn duration
      // Expect to possibly stop after detecting three more black lines
      nextAction = STOP;  // Set next action to anticipate stopping after three more lines
    }
    // Second decision point: Stop after detecting a total of five black lines
    else if (blackLineDetections == 5 && nextAction == STOP) {
      Serial.println("Office destination reached.");
      Stop();
      currentDestination = NONE;  // Reset destination after arrival
      blackLineDetections = 0;    // Reset detections for future navigations
    }
  }
}

void handleLectureHallPath(unsigned long pattern) {
  int zeroCount = countZerosInPattern(pattern);

  if (zeroCount >= 3) {  // Adjust based on the pattern expected for black lines
    blackLineDetections++;

    // Turn left after detecting three black lines
    if (blackLineDetections == 3 && nextAction == TURN_LEFT) {
      Serial.println("Turning left towards the lecture hall.");
      turnLeft();
      delay(1000);        // Adjust delay based on turn duration
      nextAction = STOP;  // Prepare to stop after two more lines
    }
    // Stop after detecting a total of five black lines
    else if (blackLineDetections == 5 && nextAction == STOP) {
      Serial.println("Lecture hall destination reached.");
      Stop();
      currentDestination = NONE;
      blackLineDetections = 0;
    }
  }
}

void handleCafeteriaPath(unsigned long pattern) {
  int zeroCount = countZerosInPattern(pattern);

  if (zeroCount >= 3) {  // Adjust based on the pattern expected for black lines
    blackLineDetections++;

    // Turn right after detecting one black line
    if (blackLineDetections == 1 && nextAction == TURN_RIGHT) {
      Serial.println("Turning right towards the cafeteria.");
      turnRight();
      delay(1000);        // Adjust delay based on turn duration
      nextAction = STOP;  // Prepare to stop after three more lines
    }
    // Stop after detecting a total of four black lines
    else if (blackLineDetections == 4 && nextAction == STOP) {
      Serial.println("Cafeteria destination reached.");
      Stop();
      currentDestination = NONE;
      blackLineDetections = 0;
    }
  }
}


unsigned long readIRPanelPattern() {
  unsigned long pattern = 0;
  for (int i = 0; i < PANEL_SENSOR_COUNT; i++) {
    pattern |= digitalRead(PANEL_SENSOR_START_PIN + i) << i;
  }
  Serial.print("Read pattern: 0b");
  for (int i = PANEL_SENSOR_COUNT - 1; i >= 0; i--) {
    Serial.print((pattern >> i) & 1);
  }
  Serial.println();
  return pattern;
}


// Function to count zeros in the pattern
int countZerosInPattern(unsigned long pattern) {
  int count = 0;
  for (int i = 0; i < PANEL_SENSOR_COUNT; i++) {
    if ((pattern & (1 << i)) == 0) {  // If the bit at position i is 0
      count++;
    }
  }
  return count;
}


void resetDetectionVariables() {
  // Reset the variables for the next detection cycle
  firstDetectionTime = 0;
  blackLineDetections = 0;
  nextAction = FOLLOW_LINE;  // Set to follow line by default
}

void searchForLine() {
  // Turn in place until one of the sensors detects the line
  while (!digitalRead(LEFT_IR_PIN) && !digitalRead(RIGHT_IR_PIN)) {
    // Turn left until the line is found (you can adjust this behavior)///
    turnLeft();
    delay(100);  // Adjust delay as needed
  }

  // Once the line is found, stop turning and resume line following
  Stop();
  followLine();
}



// Modify adjustRight and adjustLeft to accept dynamicSpeed for finer control
void adjustRight(int speed) {
  motor1.setSpeed(speed / 2);  // Slower turn for finer control
  motor2.setSpeed(speed);
  motor3.setSpeed(speed / 2);
  motor4.setSpeed(speed);
  delay(50);  // Shorter delay for quick adjustments
}

void adjustLeft(int speed) {
  motor1.setSpeed(speed);
  motor2.setSpeed(speed / 2);  // Slower turn for finer control
  motor3.setSpeed(speed);
  motor4.setSpeed(speed / 2);
  delay(50);  // Shorter delay for quick adjustments
}

void moveToLibrary() {
  Serial.println("Navigating to the library...");
  currentDestination = LIBRARY;
  nextAction = TURN_LEFT;  // Preset the next action to handle the path

  bool arrivedAtDestination = false;
  while (!arrivedAtDestination && currentDestination == LIBRARY) {
    if (!objectAvoid()) {
      followLine();
    }

    unsigned long pattern = readIRPanelPattern();
    handleLibraryPath(pattern);

    if (currentDestination == NONE) {
      arrivedAtDestination = true;
      waitForCommand = true;  // Ready for the next command
      break;
    }
  }
  followLineEnabled = false;  // Disable line following if needed
}


void moveToOffice() {
  Serial.println("Navigating to the office...");
  currentDestination = OFFICE;
  nextAction = TURN_RIGHT;  // Assume turning right is the initial action for the Office

  bool arrivedAtDestination = false;
  while (!arrivedAtDestination && currentDestination == OFFICE) {
    if (!objectAvoid()) {
      followLine();
    }

    unsigned long pattern = readIRPanelPattern();
    handleOfficePath(pattern);

    if (currentDestination == NONE) {
      arrivedAtDestination = true;
      Serial.println("Office destination reached.");
      Stop();
      waitForCommand = true;
      collectFeedback();
    }
  }
  followLineEnabled = false;
}


void moveToLectureHall() {
  Serial.println("Navigating to the lecture hall...");
  currentDestination = LECTURE_HALL;
  nextAction = TURN_LEFT;  // Adjust based on your path setup

  bool arrivedAtDestination = false;
  while (!arrivedAtDestination && currentDestination == LECTURE_HALL) {
    if (!objectAvoid()) {
      followLine();
    }

    unsigned long pattern = readIRPanelPattern();
    handleLectureHallPath(pattern);

    if (currentDestination == NONE) {
      arrivedAtDestination = true;
      Serial.println("Lecture Hall destination reached.");
      Stop();
      waitForCommand = true;
      collectFeedback();
    }
  }
  followLineEnabled = false;
}

void moveToCafeteria() {
  Serial.println("Navigating to the cafeteria...");
  currentDestination = CAFETERIA;
  nextAction = TURN_RIGHT;  // Adjust based on your path setup

  bool arrivedAtDestination = false;
  while (!arrivedAtDestination && currentDestination == CAFETERIA) {
    if (!objectAvoid()) {
      followLine();
    }

    unsigned long pattern = readIRPanelPattern();
    handleCafeteriaPath(pattern);

    if (currentDestination == NONE) {
      arrivedAtDestination = true;
      Serial.println("Cafeteria destination reached.");
      Stop();
      waitForCommand = true;
      collectFeedback();
    }
  }
  followLineEnabled = false;
}

void collectFeedback() {
  Serial.println("Destination reached. Prompting for feedback...");
  if (Serial3.availableForWrite()) {
    Serial3.println("destination_reached");  // Command for ESP32
    Stop();
  }
}

int calculateForwardTime(int distance) {
  // Example speed measurement: 20 cm/sec
  float speedCmPerSec = 20.0;  // Adjust this based on your calibration

  // Calculate time to travel the given distance
  float timeSec = distance / speedCmPerSec;

  // Convert time in seconds to milliseconds and return
  return int(timeSec * 1000);  // Convert to milliseconds since delay() uses milliseconds
}


/* void moveForwardFor5Seconds() {
  moveForward(); // Start moving forward
  delay(5000); // Continue moving forward for 5 seconds
  Stop(); // Stop the car
} */


bool objectAvoid() {
  int frontDistance = getDistance();

  if (frontDistance < 15) {
    Stop();                                // Immediately stop the robot
    Serial3.println("obstacle_detected");  // Send message to ESP32 about obstacle detection

    // Wait here until the obstacle is removed
    while (getDistance() < 15) {
      delay(100);  // Check periodically
    }

    Serial3.println("path_clear");  // Send message to ESP32 about path being clear
    return true;                    // Indicate an obstacle was detected and handled
  }
  return false;  // No immediate obstacle detected
}

void turnAround() {
    Serial.println("Initiating 180-degree turn...");
    turnRight();

    while (true) {
        if (digitalRead(LEFT_IR_PIN) == HIGH) {
            break;
        }
    }

    Stop();
    Serial.println("180-degree turn completed. Aligned with the black line.");
}


void slightlyTurnLeft() {
  // Adjust speed for minor turning without a full stop
  motor1.setSpeed(MAX_SPEED / 2);
  motor2.setSpeed(MAX_SPEED);
  delay(100);                  // Small delay to make a slight turn
  motor1.setSpeed(MAX_SPEED);  // Reset speed to normal after turning
}

void slightlyTurnRight() {
  // Adjust speed for minor turning without a full stop
  motor2.setSpeed(MAX_SPEED / 2);
  motor1.setSpeed(MAX_SPEED);
  delay(100);                  // Small delay to make a slight turn
  motor2.setSpeed(MAX_SPEED);  // Reset speed to normal after turning
}

int getDistance() {
  delay(50);
  int cm = sonar.ping_cm();
  if (cm == 0) {
    cm = 100;
  }
  return cm;
}

// Function to smoothly rotate the servo to a specified angle
void smoothServoMove(int fromAngle, int toAngle, int steps) {
  int delayTime = 15;  // Time between steps to ensure smooth movement
  int stepSize = (toAngle - fromAngle) / steps;
  for (int i = 1; i <= steps; i++) {
    servo.write(fromAngle + (stepSize * i));
    delay(delayTime);
  }
}

// Enhanced lookLeft function with reduced and dynamic delay
int lookLeft() {
  int currentAngle = 90;                           // Assuming the servo is initially facing forward
  int targetAngle = 150;                           // Target angle to look left
  smoothServoMove(currentAngle, targetAngle, 10);  // Smoothly move the servo to 150 degrees

  int distance = getAverageDistance(5);  // Take 5 measurements and average them
  servo.write(90);                       // Return to forward position smoothly
  smoothServoMove(targetAngle, currentAngle, 10);
  return distance;
}

// Enhanced lookRight function
int lookRight() {
  int currentAngle = 90;                           // Assuming the servo is initially facing forward
  int targetAngle = 30;                            // Target angle to look right
  smoothServoMove(currentAngle, targetAngle, 10);  // Smoothly move the servo to 30 degrees

  int distance = getAverageDistance(5);  // Take 5 measurements and average them
  servo.write(90);                       // Return to forward position smoothly
  smoothServoMove(targetAngle, currentAngle, 10);
  return distance;
}

// Function to take multiple measurements and average them
int getAverageDistance(int measurements) {
  long totalDistance = 0;
  for (int i = 0; i < measurements; i++) {
    totalDistance += getDistance();
    delay(50);  // Short delay between measurements to allow for sensor recovery
  }
  return totalDistance / measurements;
}


void Stop() {
  motor1.setSpeed(0);
  motor2.setSpeed(0);
  motor3.setSpeed(0);
  motor4.setSpeed(0);
  motor1.run(RELEASE);
  motor2.run(RELEASE);
  motor3.run(RELEASE);
  motor4.run(RELEASE);
  Serial.println("Motors stopped.");
}


void moveForward(int speed) {
  motor1.setSpeed(speed);
  motor2.setSpeed(speed);
  motor3.setSpeed(speed);
  motor4.setSpeed(speed);
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
}

void turnLeft() {
  // Reduce speed for the turn for better control
  int turnSpeed = 100;  // Adjust as necessary
  motor1.setSpeed(turnSpeed);
  motor2.setSpeed(turnSpeed);
  motor3.setSpeed(turnSpeed);
  motor4.setSpeed(turnSpeed);
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(BACKWARD);
  motor4.run(BACKWARD);
  delay(100);  // Short delay to execute the turn, adjust timing as needed
}

void turnRight() {
  // Reduce speed for the turn for better control
  int turnSpeed = 100;  // Adjust as necessary
  motor1.setSpeed(turnSpeed);
  motor2.setSpeed(turnSpeed);
  motor3.setSpeed(turnSpeed);
  motor4.setSpeed(turnSpeed);
  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
  delay(100);  // Short delay to execute the turn, adjust timing as needed
}


void moveBackward() {
  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
  motor3.run(BACKWARD);
  motor4.run(BACKWARD);
}

void turn() {
  if (object == false) {
    Serial.println("turn Right");
    moveLeft();
    delay(700);
    moveForward(MAX_SPEED);
    delay(800);
    moveRight();
    delay(900);
    if (digitalRead(A1) == 1) {
      loop();
    } else {
      moveForward(MAX_SPEED);
    }
  } else {
    Serial.println("turn left");
    moveRight();
    delay(700);
    moveForward(MAX_SPEED);
    delay(800);
    moveLeft();
    delay(900);
    if (digitalRead(A0) == 1) {
      loop();
    } else {
      moveForward(MAX_SPEED);
    }
  }
}

void moveRight() {
  motor1.run(BACKWARD);
  motor2.run(BACKWARD);
  motor3.run(FORWARD);
  motor4.run(FORWARD);
}

void moveLeft() {
  motor1.run(FORWARD);
  motor2.run(FORWARD);
  motor3.run(BACKWARD);
  motor4.run(BACKWARD);
}