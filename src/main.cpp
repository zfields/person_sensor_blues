#include <Arduino.h>
#include <Notecard.h>

#include "person_sensor.h"

#define LOOP_DELAY 1000

#ifndef RELEASE
#define usbSerial Serial
HardwareSerial stlink_serial(PIN_VCP_RX, PIN_VCP_TX);

#define LOG_VIA_STLINK
#ifdef LOG_VIA_STLINK
#define logSerial stlink_serial
#else
#define logSerial usbSerial
#endif // LOG_VIA_STLINK
#endif // RELEASE

#define PRODUCT_UID "com.zakoverflow.test"

// This is the unique Product Identifier for your device
#ifndef PRODUCT_UID
#define PRODUCT_UID "" // "com.my-company.my-name:my-project"
#pragma message "PRODUCT_UID is not defined in this example. Please ensure your Notecard has a product identifier set before running this example or define it in code here. More details at https://dev.blues.io/tools-and-sdks/samples/product-uid"
#endif
#define my_product_id PRODUCT_UID

Notecard notecard;

// the setup function runs once when you press reset or power the board
void setup()
{
#ifndef RELEASE
  logSerial.begin(115200);
  const size_t usb_timeout_ms = 3000;
  for (const size_t start_ms = millis(); !logSerial && (millis() - start_ms) < usb_timeout_ms;)
      ;
  notecard.setDebugOutputStream(logSerial);
#endif // RELEASE

  notecard.begin();

  J *req = notecard.newRequest("hub.set");
  JAddStringToObject(req, "product", my_product_id);
  JAddStringToObject(req, "mode", "continuous");
  JAddStringToObject(req, "sn", "Person Sensor");
  notecard.sendRequestWithRetry(req, 5);
}

// the loop function runs over and over again forever
void loop()
{
  person_sensor_results_t results = {};
  // Perform a read action on the I2C address of the sensor to get the
  // current face information detected.
  if (!person_sensor_read(&results)) {
    notecard.logDebug("No person sensor results found on the i2c bus");
    delay(LOOP_DELAY);
    return;
  }

  // Log Results
  notecard.logDebugf("********\n%d faces found\n", results.num_faces);
  for (int i = 0; i < results.num_faces; ++i) {
    const person_sensor_face_t* face = &results.faces[i];
    notecard.logDebugf("Face #%d: %d confidence, (%d, %d), (%d, %d), %s\n",
                       (i + 1),
                       face->box_confidence,
                       face->box_left,
                       face->box_top,
                       face->box_right,
                       face->box_bottom,
                       (face->is_facing ? "facing" : "not facing")
    );
  }

  // Send results to Notehub
  if (results.num_faces > 0) {
    if (J *req = notecard.newRequest("note.add"))
    {
      JAddStringToObject(req, "file", "sensors.qo");
      JAddBoolToObject(req, "sync", true);
      if (J *body = JAddObjectToObject(req, "body"))
      {
        JAddNumberToObject(body, "num_faces", results.num_faces);
        if (results.num_faces > 0) {
          J *faces = JAddArrayToObject(body, "faces");
          for (int i = 0; i < results.num_faces; ++i) {
            const person_sensor_face_t* detected_face = &results.faces[i];
            J *face = JCreateObject();
            JAddNumberToObject(face, "box_confidence", detected_face->box_confidence);
            JAddNumberToObject(face, "box_left", detected_face->box_left);
            JAddNumberToObject(face, "box_top", detected_face->box_top);
            JAddNumberToObject(face, "box_right", detected_face->box_right);
            JAddNumberToObject(face, "box_bottom", detected_face->box_bottom);
            JAddBoolToObject(face, "is_facing", detected_face->is_facing);
            JAddItemToArray(faces, face);
          }
        }
      }
      notecard.sendRequest(req);
    }
  }

  delay(LOOP_DELAY);
}
