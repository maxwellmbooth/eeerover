#include "webserver.h"

void send_response(WiFiClient& client, const char* content_type, const char* body) {
  client.println("HTTP/1.1 200 OK");
  client.print("Content-Type: ");
  client.println(content_type);
  client.println("Connection: close");
  client.println();
  client.print(body);
  client.flush();
  client.stop();
}