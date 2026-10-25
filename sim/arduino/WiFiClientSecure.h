#pragma once

// Host stand-in: TLS is handled by curl in the simulator's HTTPClient.

class WiFiClientSecure {
public:
  void setInsecure() {}
};
