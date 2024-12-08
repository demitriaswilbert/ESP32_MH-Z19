#include "main.h"
const char* wifi_status(wl_status_t status)
{
    switch (status)
    {
        case WL_NO_SHIELD:
            return "NO SHIELD";
        case WL_IDLE_STATUS:
            return "IDLE";
        case WL_NO_SSID_AVAIL:
            return "NO SSID";
        case WL_SCAN_COMPLETED:
            return "SCANNED";
        case WL_CONNECTED:
            return "CONN";
        case WL_CONNECT_FAILED:
            return "FAILED";
        case WL_CONNECTION_LOST:
            return "LOST";
        case WL_DISCONNECTED:
            return "DISCONN";
        default:
            return "UNKNOWN";
    }
}
void print_wifi_config(Stream& st)
{
    st.printf("Connected  : %s\n"
              " - SSID    : %s\n"
              " - localIP : %s\n"
              " - Gateway : %s\n"
              " - Subnet  : %s\n"
              " - DNS 0   : %s\n"
              " - DNS 1   : %s\n"
              , WiFi.macAddress().c_str()
              , WiFi.SSID().c_str()
              , WiFi.localIP().toString().c_str()
              , WiFi.gatewayIP().toString().c_str()
              , WiFi.subnetMask().toString().c_str()
              , WiFi.dnsIP(0).toString().c_str()
              , WiFi.dnsIP(1).toString().c_str()
    );
}