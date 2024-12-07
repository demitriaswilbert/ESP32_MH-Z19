#include "main.h"
const char* WiFi_Status(wl_status_t status)
{
    switch (status)
    {
        case 255:
            return "NOSHIELD";
        case 0:
            return "IDLE";
        case 1:
            return "NOSSID";
        case 2:
            return "SCANNED";
        case 3:
            return "CONN";
        case 4:
            return "CONNFAIL";
        case 5:
            return "CONNLOST";
        case 6:
            return "DISCONN";
        default:
            return "UNKNOWN";
    }
}
void print_wifi_config(Stream& st)
{
    st.printf("Connected      : %s\n"
              " - localIP     : %s\n"
              " - SSID        : %s\n",
              WiFi.macAddress().c_str(), WiFi.localIP().toString().c_str(),
              WiFi.SSID().c_str());
}