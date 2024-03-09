#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 2     // Digital pin connected to the DHT sensor 
#define DHTTYPE    DHT11     // DHT 11

DHT_Unified dht(DHTPIN, DHTTYPE);

const float messages_per_min = 0.1;
const long interval = 60000/messages_per_min;       // Interval for updating output (10 minutes in milliseconds)
unsigned long previousMillis = 0-interval;   // To get a first reading instantly

int dataPointCounter = 0;           // Counter for data points

void setup() {
  Serial.begin(9600);
  Serial.println(F("DHT11 Unified Sensor Example"));
  dht.begin();
  Serial.println("i T RH AH");
}

void loop() {
  unsigned long currentMillis = millis();  // Get the current time

  // Check if it's time to update the output
  if (currentMillis - previousMillis >= interval) {
    // Save the last time the output was updated
    previousMillis = currentMillis;

    // Read temperature and humidity
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    float temperature = event.temperature;
    dht.humidity().getEvent(&event);
    float humidity = event.relative_humidity;
    float absoluteHumidity = calculateAbsoluteHumidity(temperature, humidity);

    Serial.print(dataPointCounter);
    Serial.print(" ");
    Serial.print(temperature);
    Serial.print(" ");
    Serial.print(humidity);
    Serial.print(" ");
    Serial.println(absoluteHumidity);

    // Increment data point counter
    dataPointCounter++;
  }
}

// Function to calculate absolute humidity in g/m³
float calculateAbsoluteHumidity(float temperature, float humidity) {
  float absoluteHumidity = 1000*humidity*(100*6.112*exp(17.62*temperature/(temperature+243.12)))/(100*461.5*(temperature+273.15));
  return absoluteHumidity;
}
