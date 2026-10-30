#include "adc.h"
#include "config_system.h"
#include "console.h"
#include "drive.h"
#include "fw_header.h"
#include "graph.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>

static void info_cb(const char *req, int len, int *ret)
{
	for(uint32_t i = 0; i < FW_COUNT; i++)
	{
		_console_print("====== FW %s (%d) =====\n", ((const char *[]){"PREBOOT", "BOOT", "APP"})[i], g_fw_info[i].locked);
		if(g_fw_info[i].locked) break;
		_console_print("\tAddr: x%x | Size: %ld\n", g_fw_info[i].addr, g_fw_info[i].size);
		_console_print("\tVer: %ld.%ld.%ld\n", g_fw_info[i].ver_major, g_fw_info[i].ver_minor, g_fw_info[i].ver_patch);
		if(g_fw_info[i].field_product_ptr) _console_print("\tProd: %s\n", g_fw_info[i].field_product_ptr);
		if(g_fw_info[i].field_product_name_ptr) _console_print("\tName: %s\n", g_fw_info[i].field_product_name_ptr);
		const char *s = fw_fields_find_by_key_helper(&g_fw_info[i], "build_ts");
		if(s) _console_print("\tBuild: %s\n", s);
	}
	_console_print("UID: x%08lx.x%08lx.x%08lx\n", g_uid[0], g_uid[1], g_uid[2]);
	_console_print("Time: %ld\n", system_time_ms);
	_console_print("TIM1 ARR: %ld\n", TIM1->ARR);
	_console_print("kp ki: %.9f %.9f\n", ctrl.foc.i_reg.kp, ctrl.foc.i_reg.ki);
	_console_print("DT: %.9f\n", ctrl.pwm_dt);
}

static void save_cb(const char *req, int len, int *ret)
{
	int sts = config_write_storage();
	_console_print("saved: %d\n", sts);
}

static void adc_cb(const char *req, int len, int *ret)
{
	_console_print("I: %.4f %.4f\n", g_adc_val.i[0], g_adc_val.i[1]);
	_console_print("HALL: %d %d\n", g_adc_val.hall_raw[0], g_adc_val.hall_raw[1]);
	_console_print("Ub: %.2f\n", g_adc_val.ub);
	_console_print("Tm: %.1f Tinv: %.1f Tcpu: %.1f\n", g_adc_val.tmtr, g_adc_val.tinv, g_adc_val.tcpu);
}

static void dt_cb(const char *req, int len, int *ret)
{
	_console_print("DT prev: %d\n", TIM1->BDTR & 0xFF);
	int v = atoi(req);
	drv_upd_deadtime_ns(v);
	_console_print("DT now: %d\n", TIM1->BDTR & 0xFF);
}

static void ki_cb(const char *req, int len, int *ret)
{
	float kp, ki;

	if(sscanf(req, "%f %f", &kp, &ki) != 2)
	{
		_console_print("Wrong input!\n");
		return;
	}

	platform_make_safe();
	ctrl.foc.i_reg.kp = kp;
	ctrl.foc.i_reg.ki = ki;
	_console_print("kp ki: %.9f %.9f\n", ctrl.foc.i_reg.kp, ctrl.foc.i_reg.ki);
	cplx_pi_reg_reset(&ctrl.foc.i_reg);
}

static void kv_cb(const char *req, int len, int *ret)
{
	float kp, ki;

	if(sscanf(req, "%f %f", &kp, &ki) != 2)
	{
		_console_print("Wrong input!\n");
		return;
	}

	platform_make_safe();
	ctrl.vel_reg.kp = kp;
	ctrl.vel_reg.ki = ki;
	_console_print("kp ki: %.9f %.9f\n", ctrl.vel_reg.kp, ctrl.vel_reg.ki);
}

static void hall_calib_cb(const char *req, int len, int *ret)
{
	float current;
	if(sscanf(req, "%f", &current) != 1)
	{
		_console_print("Current not set!\n");
		return;
	}
	drive_reset_master_to();
	platform_watchdog_reset();
	drv_err_rst();

	delay_ms(10);

	drv_set_ol(current, 0, 0, 450, 1000);

	for(int i = 0; i < 20; i++)
	{
		delay_ms(50);
		drive_reset_master_to();
		platform_watchdog_reset();
	}

	ctrl.hall_smpl_cnt = 0;
	ctrl.hall[0].sum = ctrl.hall[1].sum = 0;
	ctrl.hall[0].max = ctrl.hall[1].max = 0;
	ctrl.hall[0].min = ctrl.hall[1].min = 4095;

	for(int i = 0; i < 20; i++)
	{
		delay_ms(50);
		drive_reset_master_to();
		platform_watchdog_reset();
	}

	hall_acc_t hall[2];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	memcpy(hall, ctrl.hall, sizeof(hall));
#pragma GCC diagnostic pop
	uint32_t hall_smpl_cnt = ctrl.hall_smpl_cnt;

	drive_reset_master_to();
	platform_watchdog_reset();

	drv_set_off();

	_console_print("Count: %ld\n", hall_smpl_cnt);
	for(int i = 0; i < 2; i++)
	{
		float avg = (float)hall[i].sum / (float)hall_smpl_cnt;
		_console_print("H%d: %ld %ld | AVG %.2f | %+ld %+ld\n", i, hall[i].min, hall[i].max, avg, hall[i].min - (int)avg, hall[i].max - (int)avg);
		ctrl.hall_cal[i].span = (hall[i].max - hall[i].min) / 2;
		ctrl.hall_cal[i].avg = avg;
	}

	return;

exit_hall_detect:
	_console_print("Err detected: x%x\n", ctrl.err);
}

static void unwrap_encoder_fast(float new_angle, float *prev_unwrapped)
{
	float diff = new_angle - *prev_unwrapped;

	if(diff > M_PI)
	{
		diff -= 2 * M_PI;
	}
	else if(diff < -M_PI)
	{
		diff += 2 * M_PI;
	}

	*prev_unwrapped += diff;
}

static void ang_calib_cb(const char *req, int len, int *ret)
{
	float current;
	if(sscanf(req, "%f", &current) != 1)
	{
		_console_print("Current not set!\n");
		return;
	}
	drive_reset_master_to();
	platform_watchdog_reset();
	drv_err_rst();

	delay_ms(10);

	for(int i = 0; i < 1000; i++)
	{
		drv_set_ol(0, (float)i * current / 1000.0f, 0, 0, 0);
		drive_reset_master_to();
		platform_watchdog_reset();
		if(ctrl.err) goto exit_hall_detect;
		delay_ms(1);
	}

	float foc_rot = 0, foc_rot_prev = 0;
	float mot_rot_start = ctrl.angle_enc, mot_rot_prev = ctrl.angle_enc;

	for(int i = 0; i < 45; i++) // CW
	{
		for(int j = 0; j < 360; j++)
		{
			drv_set_ol(0, current, DEG2RAD_f(j), 0, 0);
			drive_reset_master_to();
			platform_watchdog_reset();
			if(ctrl.err) goto exit_hall_detect;
			delay_ms(2);

			foc_rot += utils_norm_angle_rad(DEG2RAD_f(j) - foc_rot_prev);
			foc_rot_prev = DEG2RAD_f(j);
			unwrap_encoder_fast(ctrl.angle_enc, &mot_rot_prev);
		}
	}
	ctrl.pp = roundf(foc_rot / (mot_rot_prev - mot_rot_start));
	_console_print("CW: %.3f %.3f :: PP: %.3f [set to %.0f]\n", foc_rot, mot_rot_prev - mot_rot_start, foc_rot / (mot_rot_prev - mot_rot_start), ctrl.pp);

	foc_rot = foc_rot_prev = 0;
	mot_rot_start = ctrl.angle_enc, mot_rot_prev = ctrl.angle_enc;

	for(int i = 0; i < 15; i++) // CCW
	{
		for(int j = 360; j >= 0; j--)
		{
			drv_set_ol(0, current, DEG2RAD_f(j), 0, 0);
			drive_reset_master_to();
			platform_watchdog_reset();
			if(ctrl.err) goto exit_hall_detect;
			delay_ms(2);

			foc_rot += utils_norm_angle_rad(DEG2RAD_f(j) - foc_rot_prev);
			foc_rot_prev = DEG2RAD_f(j);
			unwrap_encoder_fast(ctrl.angle_enc, &mot_rot_prev);
		}
	}

	ctrl.pp = roundf(foc_rot / (mot_rot_prev - mot_rot_start));
	_console_print("CCW: %.3f %.3f :: PP: %.3f [set to %.0f]\n", foc_rot, mot_rot_prev - mot_rot_start, foc_rot / (mot_rot_prev - mot_rot_start), ctrl.pp);

	int wrong_pd = 0;

	ctrl.pp=6;

	float off_s = 0, off_c = 0;
	for(int i = 0; i < 3; i++) // CW
	{
		for(int j = 0; j < 360; j++)
		{
			drv_set_ol(0, current, DEG2RAD_f(j), 0, 0);
			drive_reset_master_to();
			platform_watchdog_reset();
			if(ctrl.err) goto exit_hall_detect;
			delay_ms(5);

			float v[2] = {DEG2RAD_f(j), ctrl.angle_enc};

			float off = utils_norm_angle_rad(v[0] - fmodf(v[1] * ctrl.pp, 2.0f * M_PI));
			off = wrong_pd == 1 ? -off : off;
			off_s += sin(off);
			off_c += cos(off);
		}
	}
	float off0 = atan2(off_s, off_c);
	_console_print("CW: %.3f %.3f :: OFF: %.3f\n", off_s, off_c, off0);

	off_s = off_c = 0;
	for(int i = 0; i < 3; i++) // CCW
	{
		for(int j = 360; j >= 0; j--)
		{
			drv_set_ol(0, current, DEG2RAD_f(j), 0, 0);
			drive_reset_master_to();
			platform_watchdog_reset();
			if(ctrl.err) goto exit_hall_detect;
			delay_ms(5);

			float v[2] = {DEG2RAD_f(j), ctrl.angle_enc};

			float off = utils_norm_angle_rad(v[0] - fmodf(v[1] * ctrl.pp, 2.0f * M_PI));
			off = wrong_pd == 1 ? -off : off;
			off_s += sin(off);
			off_c += cos(off);
		}
	}
	float off1 = atan2(off_s, off_c);
	_console_print("CCW: %.3f %.3f :: OFF: %.3f\n", off_s, off_c, off1);

	ctrl.enc_offset = (off0 + off1) / 2;

	drv_set_off();

	return;

exit_hall_detect:
	_console_print("Err detected: x%x\n", ctrl.err);
}

const console_cmd_t console_cmd[] = {
	{"info", info_cb},
	{"adc", adc_cb},
	{"save", save_cb},
	{"dt", dt_cb},
	{"ki", ki_cb},
	{"kv", kv_cb},
	{"hall_calib", hall_calib_cb},
	{"ang_calib", ang_calib_cb},
};
const uint32_t console_cmd_sz = sizeof(console_cmd) / sizeof(console_cmd[0]);