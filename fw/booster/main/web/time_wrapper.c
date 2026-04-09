#include "time_wrapper.h"

void (*cb_time_sync_cmplt)(void) = NULL;

static void sntp_notification(struct timeval *tv)
{
	struct tm timeinfo;
	localtime_r(&tv->tv_sec, &timeinfo);
	if(timeinfo.tm_year > (1970 - 1900))
	{
		if(cb_time_sync_cmplt) cb_time_sync_cmplt();
	};
}

void time_wrapper_init(void (*time_sync_cmplt)(void))
{
	cb_time_sync_cmplt = time_sync_cmplt;
	setenv("TZ", "MSK-3", 1);
	tzset();
	esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
	esp_sntp_setservername(0, "pool.ntp.org");
	esp_sntp_setservername(1, "time.nist.gov");
	esp_sntp_setservername(2, "time.google.com");
	esp_sntp_setservername(3, "time.windows.com");
	esp_sntp_set_time_sync_notification_cb(sntp_notification);
	esp_sntp_init();
}
