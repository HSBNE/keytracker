
void handleRoot() {
  String color="green";
  String message = "<h2>HSBNE Key Tracker</h2><h3>I'm alive and monitoring for non excellent behaviour! ;)</h3>";
  String mins;
                    
  for (int i=0; i<monitoredDevices; i++) {
    color="black";
    mins = String((now-deviceRemoved[i])/60000);
    if (deviceRemoved[i]>0) color="red";
    message+= "<p style=\"color:"+color+"\">"+monitoredDeviceNames[i];
    if (deviceRemoved[i]>0) {
      message+=" is removed ("+mins+" minutes)";
    } else {
      message+=" is present.";
    }
    message+="</p>";
  }
  message+= "<br>JSON status is available <a href=\"http://keytracker.local/status\">here</a>.";
  message+= "<br>Get MQTT status messages by subscribing to <b>keytracker/status</b> on server "+String(MQTT_SERVER)+".";
  message+= "<br><br>This page refreshes every 10 seconds<br>";
  message+= "<a href=\"http://keytracker.local/help\">help</a>";
  message+= "<meta http-equiv=\"refresh\" content=\"10\" />";

  server.send(200, "text/html", message);
}

void handleStatus() {
  String message = asJson();
  server.send(200, "application/json", message);
}

void handleHelp() {
  String message = "<h2>HSBNE KeyTracker Help</h2>";
  message+=        "<br>Yes, you need help.</br>";
  server.send(200, "text/html", message);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

