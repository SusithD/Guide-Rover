#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Audio.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define I2S_DOUT 25
#define I2S_BCLK 27
#define I2S_LRC 26
#define IR_SENSOR_PIN 33  // IR sensor pin

const char* ssid = "YOur Wifi SSID";
const char* password = "Your WIFI Password";

bool needFeedback = false; // Global flag to indicate feedback request
bool guidanceActive = false; // Indicates if guidance is currently active


WebServer server(80);
Audio audio;
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Adjust the I2C address if needed

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  pinMode(IR_SENSOR_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(100);

  lcd.init();       // Initialize the LCD
  lcd.backlight();  // Turn on the backlight

  server.on("/", handleRoot);
  server.on("/campus_info", handleCampusInfo);
  server.on("/directions", handleDirections);
  server.on("/fun_facts", handleFunFacts);
  server.on("/get_directions", HTTP_POST, handleDirectionRequest);
  server.on("/submit_review", HTTP_POST, handleSubmitReview);
  server.on("/speak", handleSpeak);
  // Route for the actual feedback form
  server.on("/feedback", HTTP_GET, handleFeedbackForm);
  server.begin();
}

void loop() {
  server.handleClient();
  audio.loop();
  detectAndGreetVisitor();
  checkForCommandsFromArduino();
}

void detectAndGreetVisitor() {
  static bool greeted = false;
  if (digitalRead(IR_SENSOR_PIN) == HIGH) {
    if (!greeted) {
      String message = "Hello and Welcome to NIBM! Please Check the display for IP to Access the Web Interface.";
      audio.connecttospeech(message.c_str(), "en");
      displayIPOnLCD();
      greeted = true;
    }
  } else {
    greeted = false;
  }
}


void checkForCommandsFromArduino() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "obstacle_detected") {
      handleObstacleDetected();
    } else if (command == "path_clear") {
      handlePathClear();
    } else if (command == "destination_reached") {
      handleDestinationReached();
    }
  }
}

void displayIPOnLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);  // Start at the first character of the first line
  lcd.print("Welcome to NIBM!");
  lcd.setCursor(0, 1);  // Start at the first character of the second line
  lcd.print(WiFi.localIP().toString());
}

void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>NIBM Robot Interface</title>
      <style>
        body { font-family: 'Arial', sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }
        .container { max-width: 600px; margin: auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1, h2 { color: #333; }
        ul { list-style-type: none; padding: 0; }
        li { margin-bottom: 10px; }
        a { color: #337ab7; text-decoration: none; }
        a:hover { text-decoration: underline; }
        form { margin-top: 20px; }
        select, button { padding: 10px; width: 100%; margin-bottom: 20px; border-radius: 5px; border: 1px solid #ddd; }
        button { background-color: #5cb85c; color: white; border: none; cursor: pointer; }
        button:hover { background-color: #4cae4c; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Welcome to NIBM Campus!</h1>
        
        <h2>Get More Details About the Campus</h2>
        <ul>
          <li><a href='/campus_info'>Campus Information</a></li>
          <li><a href='/fun_facts'>Fun Facts</a></li>
        </ul>

        <h2>Get Directions</h2>
        <form action="/get_directions" method="POST">
          <label for="destination">Choose a destination:</label>
          <select id="destination" name="destination">
            <option value="library">Library</option>
            <option value="office">Office</option>
            <option value="lecture_hall">Lecture Hall</option>
            <option value="cafeteria">Cafeteria</option>
            <!-- Add more destinations as needed -->
          </select>
          <button type="submit">Get Directions</button>
        </form>

        <h2>Contact Us</h2>
        <p>For more information or inquiries, please email us at <a href='mailto:info@nibm.lk'>info@nibm.lk</a> or call us at +94 11 2 321 431.</p>

        <h2>Helpful Resources</h2>
        <ul>
          <li><a href='https://www.nibm.lk'>Official NIBM Website</a></li>
          <li><a href='/downloads'>Downloadable Materials</a></li>
        </ul>
      </div>
    </body>
    </html>
  )";
  server.send(200, "text/html", html);
}

void guideToLocation(String location) {
    guidanceActive = true;
    Serial.println("Guide to location requested: " + location);

    String message = "Guiding to " + location + ". Please follow me.";
    audio.connecttospeech(message.c_str(), "en");
    Serial.println("Audio command sent");

    String htmlResponse = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Guidance Confirmation</title>
        <style>
            body { font-family: 'Arial', sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }
            .container { max-width: 600px; margin: auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
            h1 { color: #333; }
            p { margin: 20px 0; }
            a, button { color: #337ab7; text-decoration: none; padding: 10px; margin-top: 20px; background-color: #5cb85c; color: white; border: none; cursor: pointer; border-radius: 5px; }
            a:hover, button:hover { background-color: #4cae4c; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>Guidance to )" + location + R"(</h1>
            <p>Guiding to )" + location + R"(. Please follow the instructions being announced.</p>
            <button onclick="window.location.href='/feedback';">Leave a Review</button>
            <a href='/'><button type="button">Return Home</button></a>
        </div>
        <script>
            // If you need JavaScript for any reason
        </script>
    </body>
    </html>
    )";

    server.send(200, "text/html", htmlResponse);
}


void handleDestinationReached() {
  Serial.println("Handling destination reached. Redirecting to feedback form...");
  
  // Play a sound or message using the audio system to notify locally
  audio.connecttospeech("You have reached your destination. Please leave a review.", "en");
  
  // Redirect user to the feedback form
  //server.sendHeader("Location", "/feedback", true); // HTTP 303 redirect
  //server.send(303);
}

void handleObstacleDetected() {
  audio.connecttospeech("Obstacle detected. Please wait.", "en");
}

void handlePathClear() {
  audio.connecttospeech("Path is clear. Continuing navigation.", "en");
}

// Call this in the main loop or in a specific route handler to reset the feedback flag after it has been handled
void resetFeedbackFlag() {
  needFeedback = false;
}

void redirectToFeedbackForm() {
  String feedbackFormHTML = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Feedback Form</title>
    </head>
    <body>
      <h2>Your destination is here. Please provide feedback.</h2>
      <form action="/submit_feedback" method="POST">
        <textarea name="feedback" rows="4" cols="50" placeholder="Enter your feedback here..."></textarea>
        <input type="submit" value="Submit Feedback">
      </form>
      <a href="/">Return to Home</a>
    </body>
    </html>
  )";

  server.send(200, "text/html", feedbackFormHTML);
  resetFeedbackFlag(); // Reset the flag after serving the feedback form
}

void handleFeedbackForm() {
  String feedbackFormHTML = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Leave a Review</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          background-color: #f0f0f0;
          padding: 20px;
          margin: 0;
        }
        .container {
          max-width: 800px;
          margin: auto;
          background: white;
          padding: 20px;
          border-radius: 5px;
          box-shadow: 0 2px 4px rgba(0,0,0,0.2);
        }
        h2 {
          color: #333;
        }
        textarea, input[type="submit"] {
          width: 100%;
          padding: 10px;
          margin-top: 5px;
          border-radius: 5px;
          border: 1px solid #ddd;
          box-sizing: border-box;
        }
        input[type="submit"] {
          background-color: #4CAF50;
          color: white;
          border: none;
          cursor: pointer;
        }
        input[type="submit"]:hover {
          background-color: #45a049;
        }
        a {
          display: inline-block;
          margin-top: 20px;
          text-decoration: none;
          color: #4CAF50;
        }
        a:hover {
          text-decoration: underline;
        }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>Leave a Review</h2>
        <form action="/submit_review" method="POST">
          <label for="review">Your Review:</label>
          <textarea id="review" name="review" rows="4" cols="50" placeholder="Enter your review here..."></textarea><br>
          <input type="submit" value="Submit Review">
        </form>
        <a href="/">Return to Home</a>
      </div>
    </body>
    </html>
  )";

  server.send(200, "text/html", feedbackFormHTML);
}

void handleSubmitReview() {
  if (server.hasArg("review")) {
    String review = server.arg("review");
    Serial.println("Received review: " + review);
    // Thank the user with a more detailed audio feedback
    String thankYouMessage = "Thank you for submitting your review. Your feedback is valuable to us. Please return to the home page for more options or to end your session.";
    audio.connecttospeech(thankYouMessage.c_str(), "en");
    // Optionally, add a delay here if you find that the redirect happens before the audio finishes playing
    // delay(5000); // Adjust based on the length of your audio message

    // Redirect back to home or a confirmation page with a friendly message
    String redirectHTML = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Review Submitted</title>
      <meta http-equiv="refresh" content="10;url=/" />
      <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: #f4f4f4; }
        .container { text-align: center; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Thank You!</h1>
        <p>Your review has been successfully submitted.</p>
        <p>You will be redirected to the home page shortly.</p>
        <p><a href="/">Click here if you are not redirected.</a></p>
      </div>
    </body>
    </html>
    )";
    server.send(200, "text/html", redirectHTML);
  } else {
    server.send(400, "text/html", "<h1>Error</h1><p>Review not provided. Please go back and try again.</p>");
  }
}


void handleCampusInfo() {
  String info = "NIBM is a leading business management institute in Sri Lanka offering world-class education. Explore more about our programs, achievements, and how you can become part of our community.";

  // Updated HTML content with enhanced button and link styles, flexbox for button alignment, and dynamic speak functionality
  String htmlResponse = "<!DOCTYPE html>"
                        "<html lang=\"en\">"
                        "<head>"
                        "<meta charset=\"UTF-8\">"
                        "<title>Campus Info</title>"
                        "<style>"
                        "body {font-family: Arial, sans-serif; background-color: #f0f0f0; padding: 20px;}"
                        ".container {max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.2);}"
                        "button {background-color: #4CAF50; color: white; padding: 15px 20px; margin-right: 10px; border: none; cursor: pointer; border-radius: 5px;}"
                        "button:hover {background-color: #45a049;}"
                        "a, a:visited {color: #4CAF50; text-decoration: none; padding: 10px 15px; background-color: transparent; border: 2px solid #4CAF50; border-radius: 5px; transition: background-color 0.3s, color 0.3s;}"
                        "a:hover, a:active {background-color: #4CAF50; color: white;}"
                        ".button-container {display: flex; justify-content: center; gap: 10px; flex-wrap: wrap; margin-top: 20px;}"
                        "</style>"
                        "<script>"
                        "function speak(section) {"
                        "  fetch('/speak?section=' + section)"
                        "  .then(response => response.text())"
                        "  .then(data => console.log(data))"
                        "  .catch(error => console.error('Fetch error:', error));"
                        "}"
                        "</script>"
                        "</head>"
                        "<body>"
                        "<div class=\"container\">"
                        "<h1>Campus Information</h1>"
                        "<p id=\"info\">" + info + "</p>"
                        "<div class=\"button-container\">"
                        "<button onclick=\"speak('about')\">About Us</button>"
                        "<button onclick=\"speak('programs')\">Our Programs</button>"
                        "<button onclick=\"speak('admissions')\">Admissions</button>"
                        "<button onclick=\"speak('contact')\">Contact Information</button>"
                        "<a href='/' class='button'>Return Home</a>"
                        "</div>"
                        "</div>"
                        "</body>"
                        "</html>";

  server.send(200, "text/html", htmlResponse);
}

void handleSpeak() {
  if (!server.hasArg("section")) {
    server.send(400, "text/plain", "Missing section argument");
    return;
  }

  String section = server.arg("section");
  String infoToSpeak;

  // Determine which section to speak
  if (section == "about") {
    infoToSpeak = "NIBM is a leading business management institute in Sri Lanka, established in 1968...";
  } else if (section == "programs") {
    infoToSpeak = "NIBM offers a range of degrees and professional courses in fields such as Information Technology, Business Management, and Engineering.";
  } else if (section == "contact") {
    infoToSpeak = "For more information, visit our website or contact our admissions office.";
  } else {
    server.send(404, "text/plain", "Section not found");
    return;
  }

  // Trigger the speech output
  audio.connecttospeech(infoToSpeak.c_str(), "en");

  // Send a response back to the client
  server.send(200, "text/plain", "Speaking section: " + section);
}



void handleDirections() {
  String directions = "For administrative offices, go straight and turn left. For lecture halls, turn right.";
  server.send(200, "text/plain", "Sending Directions to Speaker...");
  audio.connecttospeech(directions.c_str(), "en");
}

void handleDirectionRequest() {
    if (server.hasArg("destination")) {
        String destination = server.arg("destination");
        guideToLocation(destination); // This function might show additional info or guide the user on the ESP32's web interface
        

        // Convert destination to a command format recognized by the Arduino
        String command;
        if (destination == "library") {
            command = "cmd_library";
        } else if (destination == "office") {
            command = "cmd_office";
        } else if (destination == "lecture_hall") {
            command = "cmd_lecture_hall";
        } else if (destination == "cafeteria") {
            command = "cmd_cafeteria";
        } else {
            // If the destination does not match any known locations, you can set a default action or send an error
            command = "cmd_unknown";
        }

        // Debug output to Serial monitor
        Serial.print("Direction Requested: ");
        Serial.println(destination);

        // Send the command to Arduino
        if (Serial2.availableForWrite()) {
            Serial2.println(command); // Sends specific command to Arduino
            Serial.println("Command sent to Arduino: " + command);

            // Acknowledge the request to the user with enhanced feedback
            String responseHTML = "<!DOCTYPE html><html><body>Guiding to " + destination + ". Please follow the robot.<br><a href='/'>Return Home</a></body></html>";
            server.send(200, "text/html", responseHTML);
        } else {
            // Handle Serial communication error
            server.send(500, "text/html", "<!DOCTYPE html><html><body>Error in sending command to the robot. Please try again.<br><a href='/'>Return Home</a></body></html>");
        }
    } else {
        // Handle missing destination argument
        server.send(400, "text/plain", "Bad Request: Missing destination argument");
    }
}

// Assume we have an array of fun facts
const char* funFacts[] = {
  "Did you know? NIBM was established in 1968 and has been at the forefront of business education in Sri Lanka.",
  "Fun Fact: NIBM offers a range of degrees and professional courses in fields such as Information Technology, Business Management, and Engineering.",
  "Interesting Fact: NIBM has partnerships with several international universities, providing students with opportunities for global exposure."
};
int funFactIndex = 0; // Index to keep track of which fun fact to show


void handleFunFacts() {
  // Select the fun fact based on the current index
  String fact = funFacts[funFactIndex];

  // Play the selected fun fact through the speaker
  audio.connecttospeech(fact.c_str(), "en");

  // Increment the index for the next fun fact, wrapping around if necessary
  funFactIndex = (funFactIndex + 1) % (sizeof(funFacts) / sizeof(funFacts[0]));

  // Prepare the HTML response with the embedded CSS
  String htmlResponse = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Fun Facts</title><style>body {font-family: Arial, sans-serif; background-color: #f0f0f0; padding: 20px; margin: 0;}.container {max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.2);}.h2 {color: #333;}.textarea, input[type=\"submit\"] {width: 100%; padding: 10px; margin-top: 5px; border-radius: 5px; border: 1px solid #ddd; box-sizing: border-box;}input[type=\"submit\"] {background-color: #4CAF50; color: white; border: none; cursor: pointer;}input[type=\"submit\"]:hover {background-color: #45a049;}a {display: inline-block; margin-top: 20px; text-decoration: none; color: #4CAF50;}a:hover {text-decoration: underline;}</style></head><body><div class=\"container\"><h1>Fun Fact</h1><p>" + fact + "</p><a href='/fun_facts'>Tell me another fun fact!</a><br><a href='/'>Return Home</a></div></body></html>";

  server.send(200, "text/html", htmlResponse);
}

