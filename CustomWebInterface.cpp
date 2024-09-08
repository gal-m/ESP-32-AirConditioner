#include "CustomWebInterface.h"

// Create WebServer on port 80
WebServer webServer(80);

void setupWebInterface(IRController& irController) {
  webServer.begin();

  // Root endpoint serving the form
  webServer.on("/", HTTP_GET, [&irController]() {
    String content = "<!DOCTYPE html><html lang='en'><head>";
    content += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    content += "<style>";
    content += "body { font-family: Arial, sans-serif; background-color: #f4f4f9; color: #333; margin: 0; padding: 20px; }";
    content += "h1 { color: #0078D7; text-align: center; }";
    content += "p { text-align: center; }";
    content += "h2 { text-align: center; }";
    content += "form { max-width: 600px; margin: auto; padding: 20px; background: white; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
    content += "label { font-weight: bold; display: block; margin-top: 10px; }";
    content += "input, select { width: 100%; padding: 8px; margin-top: 5px; margin-bottom: 20px; border: 1px solid #ccc; border-radius: 4px; }";
    content += "input[type='checkbox'] { width: auto; }";
    content += "input[type='submit'] { background-color: #0078D7; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }";
    content += "input[type='submit']:hover { background-color: #005a9e; }";
    content += "button { background-color: #e60023; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; }";
    content += "button:hover { background-color: #c5001d; }";
    content += "footer { text-align: center; padding-top: 20px; font-size: 12px; color: #888; }";
    content += "</style>";

    // Add JavaScript to refresh the page automatically
    // content += "<script>setInterval(function(){ location.reload(); }, 10000);</script>";  // Refresh every 10 seconds

    content += "</head><body>";

    content += "<h1>IR Controller Management</h1>";

    // Get the saved protocol
    String savedProtocol = irController.getProtocol();

    // Protocol selection form
    std::vector<String> protocols = irController.getIdentifiedProtocols();
    content += "<form action='/setProtocol' method='POST'>";
    content += "<label for='protocol'>Select Protocol</label>";
    content += "<select id='protocol' name='protocol'>";

    // Mark the saved protocol as selected
    for (const String& proto : protocols) {
      content += "<option value='" + proto + "'";
      if (proto == savedProtocol) {
        content += " selected";  // Mark the saved protocol as selected
      }
      content += ">" + proto + "</option>";
    }

    content += "</select>";
    content += "<input type='submit' value='Set Protocol'>";
    content += "</form>";

    // Button to delete all protocols
    content += "<form action='/deleteAllProtocols' method='POST'>";
    content += "<button type='submit'>Delete All Protocols</button>";
    content += "</form>";

    // Drying settings form
    bool dryingEnabled = irController.isDryingBeforeShutdownEnabled();
    int dryingDelay = irController.getDryingDelayMinutes();
    content += "<h2>Drying Before Shutdown</h2>";
    content += "<form action='/setDryingMode' method='POST'>";
    content += "<label for='dryingEnabled'>Enable Drying</label>";
    content += "<input type='checkbox' id='dryingEnabled' name='dryingEnabled'";
    if (dryingEnabled) {
      content += " checked";
    }
    content += ">";
    content += "<label for='dryingDelay'>Drying Delay (minutes)</label>";
    content += "<input type='number' id='dryingDelay' name='dryingDelay' min='1' max='60' value='" + String(dryingDelay) + "'>";
    content += "<input type='submit' value='Save'>";
    content += "</form>";

    content += "<footer>&copy; 2024 IR Controller Management</footer>";
    content += "</body></html>";

    webServer.send(200, "text/html", content);
  });

  // Endpoint for setting the protocol
  webServer.on("/setProtocol", HTTP_POST, [&irController]() {
    if (webServer.hasArg("protocol")) {
      String protocol = webServer.arg("protocol");
      irController.setProtocol(protocol);
      irController.saveProtocol(protocol.c_str());
    }
    webServer.send(200, "text/html", "<html><body><h1>Protocol set successfully.</h1><br><a href='/'>Go back</a></body></html>");
  });

  // Endpoint for deleting all protocols
  webServer.on("/deleteAllProtocols", HTTP_POST, [&irController]() {
    irController.deleteIdentifiedProtocols();
    webServer.send(200, "text/html", "<html><body><h1>All protocols deleted successfully.</h1><br><a href='/'>Go back</a></body></html>");
  });

  // Endpoint for updating drying settings
  webServer.on("/setDryingMode", HTTP_POST, [&irController]() {
    bool dryingEnabled = webServer.hasArg("dryingEnabled");
    int dryingDelay = webServer.hasArg("dryingDelay") ? webServer.arg("dryingDelay").toInt() : 0;
    irController.enableDryingBeforeShutdown(dryingEnabled, dryingDelay);
    irController.saveDryingSettings();
    webServer.send(200, "text/html", "<html><body><h1>Drying settings updated.</h1><br><a href='/'>Go back</a></body></html>");
  });
}

void webServerLoop() {
  webServer.handleClient();
}
