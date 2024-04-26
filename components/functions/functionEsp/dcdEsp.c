#include "dcdEsp.h"
#include "esp_log.h"
#include "fnEsp.h"
#include "mediator.h"


#define CMD_GET_VERSION      	0x01
#define CMD_GET_PIN_STATE	    0x02
#define CMD_SET_PIN_STATE	    0x03
#define CMD_GET_PIN_MODE	    0x04
#define CMD_SET_PIN_MODE	    0x05
#define CMD_GET_AP_SSID  	    0x06
#define CMD_SET_AP_SSID  	    0x07
#define CMD_GET_AP_PASS  	    0x08
#define CMD_SET_AP_PASS  	    0x09
#define CMD_GET_AP_HIDE     	0x0A
#define CMD_SET_AP_HIDE     	0x0B
#define CMD_GET_STA_SETTINGS  	0x0C
#define CMD_SET_STA_SETTINGS  	0x0D
#define CMD_RESET_CONFIG  	    0x0E
#define CMD_REBOOT  		    0x0F
#define CMD_SLEEP  		        0x10
#define CMD_SLEEP_EN            0x11
#define CMD_SLEEP_DIS           0x12
#define CMD_GET_MAC_STA         0x13
#define CMD_GET_MAC_AP          0x14
#define CMD_SET_PROD            0x41

typedef struct {
	uint8_t			id;
	method_type_t	type;
	uint8_t			req_len;
	int 			(*fn)(uint8_t * buf, ...);
}dcd_cmd_t;

static dcd_cmd_t _cmd [] = {
		{CMD_GET_VERSION,		TypeGet, FUNC_MIN_HEADER_SIZE, FnEsp_getVersion},
		{CMD_GET_PIN_STATE, 	TypeArgs, FUNC_MIN_HEADER_SIZE, FnEsp_getPinState},
		{CMD_SET_PIN_STATE, 	TypeArgs, FUNC_MIN_HEADER_SIZE, FnEsp_setPinState},
		{CMD_GET_PIN_MODE, 		TypeArgs, FUNC_MIN_HEADER_SIZE, NULL},
		{CMD_SET_PIN_MODE, 		TypeArgs, FUNC_MIN_HEADER_SIZE, FnEsp_setPinMode},
		{CMD_GET_AP_SSID, 		TypeGet, FUNC_MIN_HEADER_SIZE, FnEsp_getApSsid},
		{CMD_SET_AP_SSID, 		TypeSet, FUNC_MIN_HEADER_SIZE, FnEsp_setApSsid},
		{CMD_GET_AP_PASS, 		TypeGet, FUNC_MIN_HEADER_SIZE, FnEsp_getApPass},
		{CMD_SET_AP_PASS, 		TypeSet, FUNC_MIN_HEADER_SIZE, FnEsp_setApPass},
		{CMD_GET_AP_HIDE, 		TypeGet, FUNC_MIN_HEADER_SIZE, FnEsp_getApHidden},
		{CMD_SET_AP_HIDE, 		TypeSet, FUNC_MIN_HEADER_SIZE, FnEsp_setApHidden},
		{CMD_GET_STA_SETTINGS, 	TypeGet, FUNC_MIN_HEADER_SIZE, FnEsp_getStaSettings},
		{CMD_SET_STA_SETTINGS, 	TypeSet, FUNC_MIN_HEADER_SIZE, FnEsp_setStaSettings},
		{CMD_RESET_CONFIG, 		TypeSet, FUNC_MIN_HEADER_SIZE, FnEsp_resetApSettings},
		{CMD_GET_MAC_STA, 		TypeGet, FUNC_MIN_HEADER_SIZE, FnEsp_getStaMac},
		{CMD_GET_MAC_AP, 		TypeGet, FUNC_MIN_HEADER_SIZE, FnEsp_getApMac},
		{CMD_REBOOT,			TypeSet, FUNC_MIN_HEADER_SIZE, FnEsp_systemReset},
		{CMD_SLEEP, 			TypeSet, FUNC_MIN_HEADER_SIZE, NULL},
		{CMD_SLEEP_EN, 			TypeSet, FUNC_MIN_HEADER_SIZE, NULL},
		{CMD_SLEEP_DIS, 		TypeSet, FUNC_MIN_HEADER_SIZE, NULL},
		{CMD_SET_PROD, 			TypeSet, FUNC_MIN_HEADER_SIZE, NULL}
};

#define TAG (__FILE__)

extern int DcdEsp_receive(uint8_t const * in_buf, const size_t in_len, uint8_t * out_buf, const size_t out_len)
{
//	uint8_t ie_len = in_data[0];
	uint8_t topic =  in_buf[1];
	uint8_t method = in_buf[2];

	if(in_len < FUNC_MIN_HEADER_SIZE)
		return Mtor_nAck(topic, method, RESP_INFO_ELEMENT_ERROR, -1, out_buf);

	out_buf[0] = FUNC_MIN_HEADER_SIZE;
	out_buf[1] = topic;
	out_buf[2] = method;
	ESP_LOG_BUFFER_HEXDUMP(TAG, in_buf, in_len, ESP_LOG_DEBUG);
	ESP_LOGI(TAG, "%d: %0.2x %d ", __LINE__, topic, method);
	for(int i = 0; i < sizeof(_cmd) / sizeof(dcd_cmd_t); i++){
		if(_cmd[i].id == method && _cmd[i].fn != NULL){
			if(in_buf[0] < _cmd[i].req_len)
				return Mtor_nAck(topic, method, RESP_INFO_ELEMENT_ERROR, -1, out_buf);

			int resp_len;
			if(_cmd[i].type == TypeGet)
				resp_len = _cmd[i].fn(&out_buf[3], out_len - FUNC_MIN_HEADER_SIZE);
			else if(_cmd[i].type == TypeSet)
				resp_len = _cmd[i].fn((uint8_t *)&in_buf[3], in_len - FUNC_MIN_HEADER_SIZE);
			else{
				ESP_LOGD(TAG, "%d: %0.2x %0.2x %0.2x %0.2x ", \
							__LINE__, (uint8_t *)&in_buf[3], in_len - FUNC_MIN_HEADER_SIZE, &out_buf[3], out_len - FUNC_MIN_HEADER_SIZE);
				resp_len = _cmd[i].fn((uint8_t *)&in_buf[3], in_len - FUNC_MIN_HEADER_SIZE, &out_buf[3], out_len - FUNC_MIN_HEADER_SIZE);
			}

			if(resp_len == ESP_FAIL)
				return Mtor_nAck(topic, method, RESP_PARAMETER_ERROR, -1, out_buf);

			out_buf[0] += (resp_len & 0xFF);
			return (out_buf[0] + (resp_len & 0xC0000000));
			//return (_cmd[i].type == TypeSet) ? Mtor_nAck(topic, method, RESP_OK, -1, out_buf) : out_buf[0];
		}
	}

	return Mtor_nAck(topic, method, RESP_ATTR_NOT_FOUND, -1, out_buf);
}
