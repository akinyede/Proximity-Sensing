#include <ArduinoBLE.h>
#include <SimpleKalmanFilter.h>
// Bluetooth® Low Energy Battery Service
BLEService humanTagService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// Bluetooth® Low Energy Battery Level Characteristic
BLEStringCharacteristic humanTagChar("f897177b-aee8-4767-8ecc-cc694fd5fcef",  // standard 16-bit characteristic UUID
                                     BLERead | BLENotify, 512);               // remote clients will be able to get notifications if this characteristic changes

long previousMillis = 0;  // last time the battery level was checked, in ms
float calculated_distance = 0;
int SCAN_DURATION = 50000;
int buzzer = D7;
bool foundBeacon = false;
SimpleKalmanFilter simpleKalmanFilter(2, 2, 0.01);
// Define the MedianFilter class
class MedianFilter {
public:
  // Constructor to initialize the window size
  MedianFilter(int window_size) {
    this->window_size = window_size;
  }
  // Method to add a new value to the buffer
  void add_value(float value) {
    buffer[buffer_index++] = value;
    if (buffer_index >= window_size) {
      buffer_index = 0;
    }
  }
  // Method to get the median of the values in the buffer
  float get_filtered_value() {
    // Sort the buffer array
    for (int i = 0; i < window_size - 1; i++) {
      for (int j = i + 1; j < window_size; j++) {
        if (buffer[j] < buffer[i]) {
          float temp = buffer[i];
          buffer[i] = buffer[j];
          buffer[j] = temp;
        }
      }
    }
    // Calculate and return the median
    if (window_size % 2 == 0) {
      return (buffer[window_size / 2 - 1] + buffer[window_size / 2]) / 2.0;
    } else {
      return buffer[window_size / 2];
    }
  }
private:
  int window_size;
  float buffer[100];  // Define a buffer to hold the values
  int buffer_index = 0;
};
// Define the KalmanFilter class
class KalmanFilter {
public:
  // Constructor to initialize the parameters
  KalmanFilter(float process_variance, float measurement_variance, float estimated_measurement_variance) {
    this->process_variance = process_variance;
    this->measurement_variance = measurement_variance;
    this->estimated_measurement_variance = estimated_measurement_variance;
    this->posteri_estimate = 0.0;
    this->posteri_error_estimate = 1.0;
  }
  // Method to update the filter with a new measurement
  float update(float measurement) {
    // Prediction update
    float priori_estimate = posteri_estimate;
    float priori_error_estimate = posteri_error_estimate + process_variance;
    // Measurement update
    float blending_factor = priori_error_estimate / (priori_error_estimate + estimated_measurement_variance);
    posteri_estimate = priori_estimate + blending_factor * (measurement - priori_estimate);
    posteri_error_estimate = (1 - blending_factor) * priori_error_estimate;
    return posteri_estimate;
  }
private:
  float process_variance;
  float measurement_variance;
  float estimated_measurement_variance;
  float posteri_estimate;
  float posteri_error_estimate;
};
// Function to estimate the distance based on RSSI values
float estimate_distance(float RSSI_median, float RSSI_refer) {
  float ratio = RSSI_median / RSSI_refer;
  float distance = 0.0; // Initialize distance to 0.0

  // Check if RSSI_median is greater than RSSI_refer
  if (RSSI_median > RSSI_refer) {
    // Use the first formula when RSSI_median is greater than RSSI_refer
    distance = 1.37 * pow(ratio, 6.854) - 0.372;
  } else {
    // Use the second formula when RSSI_median is less than or equal to RSSI_refer
    distance = 0.979 * pow(ratio, 7.489) + 0.183;
  }

  return distance;
}

MedianFilter median_filter(10);
void setup() {
  Serial.begin(115200);  // initialize serial communication
  pinMode(buzzer, OUTPUT);
  // while (!Serial)
  //   ;
  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1)
      ;
  }
  /* Set a local name for the Bluetooth® Low Energy device
     This name will appear in advertising packets
     and can be used by remote devices to identify this Bluetooth® Low Energy device
     The name can be changed but maybe be truncated based on space left in advertisement packet
  */
  BLE.setLocalName("H-TAG-1");
  BLE.setAdvertisedService(humanTagService);        // add the service UUID
  humanTagService.addCharacteristic(humanTagChar);  // add the battery level characteristic
  BLE.addService(humanTagService);                  // Add the battery service
                                                    // humanTagChar.writeValue(oldBatteryLevel);        // set initial value for this characteristic
  /* Start advertising Bluetooth® Low Energy.  It will start continuously transmitting Bluetooth® Low Energy
     advertising packets and will be visible to remote Bluetooth® Low Energy central devices
     until it receives a new connection */
  // start advertising
  // BLE.scan();
  BLE.advertise();
  Serial.println("Bluetooth® device active, waiting for connections...");
  BLE.scan();
  //printDeviceAddress();
}


void loop() {
  static unsigned long lastScanTime = 0;
  static unsigned long lastFalseTime = 0;
  const unsigned long scanInterval = 100;      // Scan interval in milliseconds (adjust as needed)
  const unsigned long timeoutDuration = 3000;  // Timeout duration in milliseconds (adjust as needed)

  // Check if it's time to perform a BLE scan
  if (millis() - lastScanTime >= scanInterval) {
    // Initiate BLE scanning
    BLE.scan();
    lastScanTime = millis();  // Update last scan time
  }

  // Check for available BLE devices
  BLEDevice peripheral = BLE.available();

  // Flag to track if the desired peripheral is found
  bool foundBeacon = false;

  // Handle each discovered peripheral
  if (peripheral) {
    // Check if the peripheral matches the desired criteria (e.g., local name)
    if (peripheral.localName() == "LE BEACONN") {
      foundBeacon = true;
      lastFalseTime = 0;  // Reset the timer if the beacon is found
      // Process the peripheral
      Serial.println("FOUND BEACON");
      median_filter.add_value(peripheral.rssi());
      float filtered_rssi = median_filter.get_filtered_value();
      float measured_value = peripheral.rssi() + random(-100, 100) / 100.0;
      float estimated_value = simpleKalmanFilter.updateEstimate(measured_value);
      calculated_distance = calculateDistance(estimated_value);
      

      if (estimate_distance(filtered_rssi, -42) < 5) {
        digitalWrite(buzzer, HIGH);
      } else {
        Serial.println("OUT OF RANGE");
        digitalWrite(buzzer, LOW);
      }
    }




   


  }


   // Update the last false time if beacon is not found
    if (!foundBeacon) {
       //digitalWrite(buzzer, LOW);
       //Serial.println("BEACON NOT FOUND");
      if (lastFalseTime == 0) {
        lastFalseTime = millis();  // Record the start time of continuous false
      } else if (millis() - lastFalseTime >= timeoutDuration) {
        // If continuous false for the specified duration, trigger action
        Serial.println("Beacon not found for 3 seconds, ITS DISCONNECTED!");
         digitalWrite(buzzer, LOW);
        // Perform action here
      }
    } else {
      // Reset the timer if the beacon is found
      lastFalseTime = 0;
    }
}



void checkPeripheralProximity() {
  // check if a peripheral has been discovered
  //Serial.println("CHECK IF PERIPHERAL HAS BEEN DISCOVERED!");
  BLEDevice peripheral = BLE.available();
  // if (peripheral) {
  Serial.println("device detected!");



  if (peripheral.localName() == "LE BEACONN") {
    Serial.println(peripheral.localName());
    Serial.println("FOUND THE BRIDGE");
    //peripheral.rssi();
    //Serial.println((peripheral.rssi()));
    median_filter.add_value(peripheral.rssi());
    float filtered_rssi = median_filter.get_filtered_value();
    // Process the filtered RSSI value further as needed
    // Update the Kalman filter with the filtered RSSI value
    // Notify central device about the filtered RSSI value
    //humanTagChar.writeValue(String(filtered_rssi));
    Serial.println(estimate_distance(filtered_rssi, -60));
    if (estimate_distance(filtered_rssi, -60) < 5) {
      digitalWrite(buzzer, HIGH);
    } else {
      digitalWrite(buzzer, LOW);
    }
    BLE.scan();
    delay(500);
  } else if (peripheral.localName() != "LE BEACONN") {

    Serial.println("DISCONNECTED---->");
    digitalWrite(buzzer, LOW);
  }

}
float calculateDistance(int r) {
  //float distance = pow(10, (-60 - r) / (10 * 2.4));
  float distance = pow(10, (-50 - r) / (10 * 4));
  return round(distance * 10000.0) / 10000.0;  // Round to 4 decimal places
}