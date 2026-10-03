#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

void app_report_reset_cause(void);   /* prints a message if the last reset was the watchdog */
void app_led_task(void);             /* none of these return */
void app_logger_task(void);
void app_sensor_task(void);
void app_watchdog_task(void);

#ifdef __cplusplus
}
#endif

#endif
