/**********************************************************************************************************
** �ļ���		:mqtt_app.h
** ����			:maxlicheng<licheng.chn@outlook.com>
** ����github	:https://github.com/maxlicheng
** ���߲���		:https://www.maxlicheng.com/	
** ��������		:2018-08-08
** ����			:mqtt�������ͷ�ļ�
************************************************************************************************************/
#ifndef _MQTT_APP_H_
#define _MQTT_APP_H_
#include "sys.h"

//�û���Ҫ�����豸��Ϣ�������º궨���е���Ԫ������
#define PRODUCT_KEY    	"a1Yjxb6GjGk"															//�����ư䷢�Ĳ�ƷΨһ��ʶ��11λ���ȵ�Ӣ������������
#define DEVICE_NAME    	"mqtt_test"																//�û�ע���豸ʱ���ɵ��豸Ψһ��ţ�֧��ϵͳ�Զ����ɣ�Ҳ��֧���û������Զ����ţ���Ʒά����Ψһ
#define DEVICE_SECRET  	"vfW2KtmvfGy9AcBwNY9h4wksJifwt2Lf"				//�豸��Կ����DeviceName�ɶԳ��֣�������һ��һ�ܵ���֤����

//#define PRODUCT_SECRET 	"a1L5lKy2Cpn"														//�����ư䷢�Ĳ�Ʒ������Կ��ͨ����ProductKey�ɶԳ��֣�������һ��һ�ܵ���֤����


//���º궨��̶�������Ҫ�޸�
#define HOST_NAME  			PRODUCT_KEY".iot-as-mqtt.cn-shanghai.aliyuncs.com"															//����������
#define HOST_PORT 			1883																																						//�����������˿ڣ��̶�1883
#define CONTENT				"clientId"DEVICE_NAME"deviceName"DEVICE_NAME"productKey"PRODUCT_KEY"timestamp789"	//�����¼������
#define CLIENT_ID			DEVICE_NAME"|securemode=3,signmethod=hmacsha1,timestamp=789|"											//�ͻ���ID
#define USER_NAME			DEVICE_NAME"&"PRODUCT_KEY																													//�ͻ����û���
//#define PASSWORD			"AA6A749E740A3019D58090FF3ADC57B9DB4B380E"																			//�ͻ��˵�¼passwordͨ��hmac_sha1�㷨�õ�����Сд������
#define DEVICE_PUBLISH		"/sys/"PRODUCT_KEY"/"DEVICE_NAME"/thing/event/property/post"									//
#define DEVICE_SUBSCRIBE	"/sys/"PRODUCT_KEY"/"DEVICE_NAME"/thing/service/property/set"									//�����豸����

//��������TOPIC�ĺ궨�岻��Ҫ�û��޸ģ�����ֱ��ʹ��
//IOT HUBΪ�豸��������TOPIC��update�����豸������Ϣ��error�����豸��������get���ڶ�����Ϣ
//#define TOPIC_UPDATE         "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
//#define TOPIC_ERROR          "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
//#define TOPIC_GET            "/"PRODUCT_KEY"/"DEVICE_NAME"/get"

void mqtt_thread(void);
void HexToStr(uint8_t *pbDest, const uint8_t *pbSrc, int nLen);													//��ֵת16�����ַ���
void getPassword(const char *device_secret, const char *content, char *password);						//�û������ȡ
//u32 PublishData(float temp, float humid, unsigned char *payloadbuf);

#endif



