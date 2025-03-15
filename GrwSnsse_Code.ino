#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>  // Include the ESP32Servo library

// WiFi credentials
const char* ssid = "Muhtashim";             // Replace with your WiFi name
const char* password = "butcher568";     // Replace with your WiFi password

// MQTT Broker settings
const char* mqtt_server = "broker.emqx.io";  // Your provided EMQX broker address
const int mqtt_port = 1883;                  // MQTT port
const char* mqtt_user = "";                  // Username (if needed)
const char* mqtt_password = "";              // Password (if needed)

// DHT setup
#define DHTPIN 4     // ESP32 GPIO pin connected to the DHT sensor
#define DHTTYPE DHT22 // DHT11 or DHT22 sensor type
DHT dht(DHTPIN, DHTTYPE);

// Define pin numbers
const int trigPin = 15;    // Trigger pin (connected to the Ultrasonic sensor's Trigger)
const int echoPin = 2;    // Echo pin (connected to the Ultrasonic sensor's Echo)

// Define moisture sensor pin numbers
const int soilSensorPin = 34;  // Define Soil moisture pin input

// Define servo control pin (GPIO pin with PWM support)
const int servoPin = 17;   // Pin for the servo motor (using pin 18 for ESP32)
Servo myServo;             // Create a Servo object

// Define relay control pins (GPIO pins)
const int soilrelayPin = 19;    // Change this to the actual GPIO pin where the relay is connected
const int tempRelayPin = 21; // Pin for the second relay (used for temperature range control)

// Define threshold for distance measurement
float thresholdDistance = 10.0; // Set threshold distance (in cm)

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Function to connect to WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

// Function to reconnect to MQTT Broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  // Start serial communication at 9600 baud rate
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  dht.begin();

  // Set up the analog input pin for soil moisture
  pinMode(soilSensorPin, INPUT);

  // Set the Trigger and Echo pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Set the relay pins as output
  pinMode(soilrelayPin, OUTPUT);
  pinMode(tempRelayPin, OUTPUT);

  // Initialize both relays as OFF
  digitalWrite(soilrelayPin, HIGH);      // Ensure relay is off at the start
  digitalWrite(tempRelayPin, HIGH);  // Ensure temperature relay is off at the start

  // Attach the servo to the servo pin
  myServo.attach(servoPin);

  // Ensure the servo starts at 0 degrees
  myServo.write(0);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read temperature and humidity data
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Check if the data was successfully read
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    // Print temperature and humidity data to the serial monitor
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print("°C, Humidity: ");
    Serial.print(h);
    Serial.println("%");

    // Create character buffers to publish the data
    char temperatureString[8];
    char humidityString[8];
    dtostrf(t, 1, 2, temperatureString);
    dtostrf(h, 1, 2, humidityString);

    // Publish temperature and humidity data to MQTT topics
    client.publish("dht/temperature", temperatureString);
    client.publish("dht/humidity", humidityString);
  }

  // Read the analog value from the soil sensor
  int soilMoistureValue = analogRead(soilSensorPin);

  // Check soil moisture level and control the relay
  if (soilMoistureValue > 1600) {
    Serial.println("Soil is dry - Turning ON Water Pump!");
    Serial.print("Soil Moisture Value: ");
    Serial.println(soilMoistureValue);
    digitalWrite(soilrelayPin, LOW);  // Turn on relay
    client.publish("relay/soil", "ON"); // Publish relay status
    delay(3000);
    digitalWrite(soilrelayPin, HIGH);   // Turn off relay (assuming relay is active-high)
  } else {
    Serial.println("Soil moisture is in a good range.");
    client.publish("relay/soil", "OFF"); // Publish relay status
  }

  // Publish soil moisture data to MQTT
  char moistureString[8];
  itoa(soilMoistureValue, moistureString, 10); // Convert integer to string
  client.publish("soil/moisture", moistureString);

  // Temperature relay control based on temperature value
  if (t > 25.0) { // If the temperature is greater than 25°C
    Serial.println("It is too hot! Turning ON temp relay.");
    digitalWrite(tempRelayPin, LOW);  // Turn on the second relay
    client.publish("relay/temperature", "ON"); // Publish relay status
  } else {
    digitalWrite(tempRelayPin, HIGH); // Turn off the second relay
    client.publish("relay/temperature", "OFF"); // Publish relay status
  }

  // Send a pulse to trigger the ultrasonic sensor
  digitalWrite(trigPin, LOW);  
  delayMicroseconds(2);  // Short delay to ensure the sensor resets
  digitalWrite(trigPin, HIGH);  
  delayMicroseconds(10);  // 10 microsecond pulse
  digitalWrite(trigPin, LOW);  

  // Measure the time it takes for the echo to return
  long duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance (in cm)
  float distance = duration * 0.0344 / 2;  // speed of sound is 0.0344 cm/μs, divide by 2 for round trip
  
  // Print the result to the Serial Monitor
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  // Check if the distance is less than the threshold (10 cm)
  if (distance < thresholdDistance) {
    // If distance is less than 10 cm, open the servo to 160 degrees
    Serial.println("Opening servo to 160 degrees!");
    myServo.write(160);  // Move the servo to 160 degrees
    delay(10000);        // Wait for 10 seconds
    myServo.write(0);    // Move the servo back to 0 degrees
    Serial.println("Closing servo to 0 degrees!");
  }

  // Delay for a short period before the next loop
  delay(2000); // 2 seconds delay to prevent excessive serial printing
}

