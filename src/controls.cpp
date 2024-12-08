#include <main.h>
#include <vector>
// tx_str and rx_str data

static bool enable_backspace_clear = true;
static bool enable_enter = false;

// control variables
static String rx_str = String();

inline bool isEnd(bool enable, char c) {
    return (enable && (c == '\n' || c == '\r')) || c == '\033';
}

typedef struct {
    String prefix;
    size_t param_len;
    void (*func)(String in_str, Stream& st);
} command_t;

int process_command(String& in, Stream& st, command_t* ctl) {
    if (!ctl->param_len) {
        if (ctl->prefix == in) {
            ctl->func("", st);
            return 1;
        }
    } else {
        if (in.startsWith(ctl->prefix) && in.length() > ctl->prefix.length() &&
            in.length() < (ctl->prefix.length() + ctl->param_len)) {

            ctl->func(in.substring(ctl->prefix.length()), st);
            return 1;
        }
    }
    return 0;
}

static std::vector<command_t> commands = {
    {"KB bkspc", 0,
     [](String in_str, Stream& st) {
         enable_backspace_clear = !enable_backspace_clear;
         st.println(enable_backspace_clear ? "Backspace clears RX buffer"
                                           : "Backspace is normal key");
     }},
    {"KB enter", 0,
     [](String in_str, Stream& st) {
         enable_enter = !enable_enter;
         st.println(enable_enter ? "Enter is also a terminating key"
                                 : "Enter is normal key");
     }},
    {"KB delay ", 10,
     [](String in_str, Stream& st) {
         in_str.trim();
         float tmp_delay = in_str.toFloat();
         tmp_delay = tmp_delay > 180000 ? 180000 : tmp_delay;
         tmp_delay = tmp_delay < 20 ? 20 : tmp_delay;

         if (rgb_period_queue == NULL) return;
         xQueueSend(rgb_period_queue, &tmp_delay, 100);

        //  st.printf("Delay: %f ms\n", tmp_delay);
     }},
    {"KB delay", 0,
     [](String in_str, Stream& st) {
         float tmp_delay;
         tmp_delay = rgb_period;
         st.printf("Delay: %f ms\n", tmp_delay);
     }},
    {"KB baseval ", 10,
     [](String in_str, Stream& st) {
         in_str.trim();
         int tmp_baseval = in_str.toInt();
         tmp_baseval = tmp_baseval > 255 ? 255 : tmp_baseval;
         tmp_baseval = tmp_baseval < 0 ? 0 : tmp_baseval;

         if (rgb_period_queue == NULL) return;
         if (xSemaphoreTake(rgb_period_queue, 0xfffffffful) == pdTRUE) {
             base_val = tmp_baseval;
             for (int i = 0; i < 3; i++)
                 color[i] = base_val;

             rgb_levels = 768 - (3 * base_val);
             if (rgb_levels == 0) rgb_levels = 1;
             col_index = 0;
             xSemaphoreGive(rgb_period_queue);
         }

         st.printf("Key baseval: %d ms\n", tmp_baseval);
     }},
    {"KB wifiid ", 10,
     [](String in_str, Stream& st) {
         in_str.trim();
         size_t tmp_baseval = in_str.toInt();
         xQueueSend(wifi_switch_target, &tmp_baseval, 0);
         st.printf("WiFi id: %d ms\n", tmp_baseval);
     }},
    {"KB baseval", 0,
     [](String in_str, Stream& st) {
         int tmp_baseval;
         if (rgb_period_queue == NULL) return;
         if (xSemaphoreTake(rgb_period_queue, 0xfffffffful) == pdTRUE) {
             tmp_baseval = base_val;
             xSemaphoreGive(rgb_period_queue);
         }
         st.printf("baseval: %d ms\n", tmp_baseval);
     }},
    {"KB commands", 0,
     [](String in_str, Stream& st) {
         size_t i = 0;
         for (auto& cmd : commands) {
             st.printf("%3d. param length: %3d, Prefix: \"%s\"\n", i++,
                       cmd.param_len, cmd.prefix.c_str());
         }
     }},

};

/**
 * @brief Process received character from stream
 *
 * @param c
 */
void process_char(Stream& st, char c) {

    /** @brief Clear received string */
    if (c == '\b' && enable_backspace_clear) {
        st.printf("Cleared: %lu\n", rx_str.length());
        rx_str = String();
        return;
    }

    /**
     * @brief received Escape char (\033)
     *
     * start processing received string
     * when received Escape char
     *
     */
    if (isEnd(enable_enter, c)) {
        st.println();
        for (auto& i : commands) {
            if (process_command(rx_str, st, &i)) {
                rx_str = String();
                return;
            }
        }
        st.printf("Received: %lu bytes\n", rx_str.length());
        rx_str = String();
    } else {
        rx_str += (char)c;
        st.write(c);
    }
}

/**
 * @brief wrapper function for process_char
 *
 * loops process_char on Stream::available()
 *
 * @param st stream to process
 */
void process_string(Stream& st, const char* buf) {
    const char* p = buf;
    while (*p) {
        process_char(st, *p++);
    }
}

/**
 * @brief wrapper function for process_char
 *
 * loops process_char on Stream::available()
 *
 * @param st stream to process
 */
void process_char_loop(Stream& st) {
    while (st.available()) {
        process_char(st, st.read());
    }
}