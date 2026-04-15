#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

//�߳����ȼ�
#ifndef TCPIP_THREAD_PRIO
#define TCPIP_THREAD_PRIO		5	//�����ں���������ȼ�Ϊ5
#endif
#undef  DEFAULT_THREAD_PRIO
#define DEFAULT_THREAD_PRIO		2


#define SYS_LIGHTWEIGHT_PROT    1		//Ϊ1ʱʹ��ʵʱ����ϵͳ������������,�����ؼ����벻���жϴ��
#define NO_SYS                  0  		//ʹ��UCOS����ϵͳ
#define MEM_ALIGNMENT           4  		//ʹ��4�ֽڶ���ģʽ
#define MEM_SIZE                16000 	//�ڴ��heap��С
#define MEMP_NUM_PBUF           20 		//MEMP_NUM_PBUF:memp�ṹ��pbuf����,���Ӧ�ô�ROM���߾�̬�洢�����ʹ�������ʱ,���ֵӦ�����ô�һ��
#define MEMP_NUM_UDP_PCB        6		//MEMP_NUM_UDP_PCB:UDPЭ����ƿ�(PCB)����.ÿ�����UDP"����"��Ҫһ��PCB.
#define MEMP_NUM_TCP_PCB        10		//MEMP_NUM_TCP_PCB:ͬʱ���������TCP����
#define MEMP_NUM_TCP_PCB_LISTEN 6		//MEMP_NUM_TCP_PCB_LISTEN:�ܹ�������TCP��������
#define MEMP_NUM_TCP_SEG        15		//MEMP_NUM_TCP_SEG:���ͬʱ�ڶ����е�TCP������
#define MEMP_NUM_SYS_TIMEOUT    8		//MEMP_NUM_SYS_TIMEOUT:�ܹ�ͬʱ�����timeout����

//pbufѡ��
#define PBUF_POOL_SIZE          20		//PBUF_POOL_SIZE:pbuf�ڴ�ظ���
#define PBUF_POOL_BUFSIZE       512		//PBUF_POOL_BUFSIZE:ÿ��pbuf�ڴ�ش�С

#define LWIP_TCP                1  		//ʹ��TCP
#define TCP_TTL                 255		//����ʱ��

#undef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ         0 		//��TCP�����ݶγ�������ʱ�Ŀ���λ,���豸���ڴ��С��ʱ�����ӦΪ0

#undef TCPIP_MBOX_SIZE
#define MAX_QUEUE_ENTRIES               16
#define TCPIP_MBOX_SIZE         MAX_QUEUE_ENTRIES   //tcpip�������߳�ʱ����Ϣ�����С

#undef DEFAULT_TCP_RECVMBOX_SIZE
#define DEFAULT_TCP_RECVMBOX_SIZE       MAX_QUEUE_ENTRIES  

#undef DEFAULT_ACCEPTMBOX_SIZE
#define DEFAULT_ACCEPTMBOX_SIZE         MAX_QUEUE_ENTRIES  


#define TCP_MSS                 (1500 - 40)	  	//���TCP�ֶ�,TCP_MSS = (MTU - IP��ͷ��С - TCP��ͷ��С
#define TCP_SND_BUF             (4*TCP_MSS)		//TCP���ͻ�������С(bytes).
#define TCP_SND_QUEUELEN        (2* TCP_SND_BUF/TCP_MSS)	//TCP_SND_QUEUELEN: TCP���ͻ�������С(pbuf).���ֵ��СΪ(2 * TCP_SND_BUF/TCP_MSS)
#define TCP_WND                 (2*TCP_MSS)		//TCP���ʹ���
#define LWIP_ICMP               1 	//ʹ��ICMPЭ��
#define LWIP_DHCP               1	//ʹ��DHCP
#define LWIP_UDP                1 	//ʹ��UDP����
#define UDP_TTL                 255 //UDP���ݰ�����ʱ��
#define LWIP_STATS 0
#define LWIP_PROVIDE_ERRNO 1


//֡У���ѡ�STM32F4x7����ͨ��Ӳ��ʶ��ͼ���IP,UDP��ICMP��֡У���
#define CHECKSUM_BY_HARDWARE //����CHECKSUM_BY_HARDWARE,ʹ��Ӳ��֡У��
#ifdef CHECKSUM_BY_HARDWARE
  //CHECKSUM_GEN_IP==0: Ӳ������IP���ݰ���֡У���
  #define CHECKSUM_GEN_IP                 0
  //CHECKSUM_GEN_UDP==0: Ӳ������UDP���ݰ���֡У���
  #define CHECKSUM_GEN_UDP                0
  //CHECKSUM_GEN_TCP==0: Ӳ������TCP���ݰ���֡У���
  #define CHECKSUM_GEN_TCP                0 
  //CHECKSUM_CHECK_IP==0: Ӳ����������IP���ݰ�֡У���
  #define CHECKSUM_CHECK_IP               0
  //CHECKSUM_CHECK_UDP==0: Ӳ����������UDP���ݰ�֡У���
  #define CHECKSUM_CHECK_UDP              0
  //CHECKSUM_CHECK_TCP==0: Ӳ����������TCP���ݰ�֡У���
  #define CHECKSUM_CHECK_TCP              0
#else
  //CHECKSUM_GEN_IP==1: ��������IP���ݰ�֡У���
  #define CHECKSUM_GEN_IP                 1
  // CHECKSUM_GEN_UDP==1: ��������UDOP���ݰ�֡У���
  #define CHECKSUM_GEN_UDP                1
  //CHECKSUM_GEN_TCP==1: ��������TCP���ݰ�֡У���
  #define CHECKSUM_GEN_TCP                1
  // CHECKSUM_CHECK_IP==1: ������������IP���ݰ�֡У���
  #define CHECKSUM_CHECK_IP               1
  // CHECKSUM_CHECK_UDP==1: ������������UDP���ݰ�֡У���
  #define CHECKSUM_CHECK_UDP              1
  //CHECKSUM_CHECK_TCP==1: ������������TCP���ݰ�֡У���
  #define CHECKSUM_CHECK_TCP              1
#endif



#define LWIP_NETCONN                    1 	//LWIP_NETCONN==1:ʹ��NETCON����(Ҫ��ʹ��api_lib.c)
#define LWIP_SOCKET                     1	//LWIP_SOCKET==1:ʹ��Sicket API(Ҫ��ʹ��sockets.c)
#define LWIP_COMPAT_MUTEX               1
#define LWIP_SO_RCVTIMEO                1 	//ͨ������LWIP_SO_RCVTIMEOʹ��netconn�ṹ����recv_timeout,ʹ��recv_timeout���Ա��������߳�

//�й�ϵͳ��ѡ��
#define TCPIP_THREAD_STACKSIZE          1000	//�ں������ջ��С
#define DEFAULT_UDP_RECVMBOX_SIZE       2000
#define DEFAULT_THREAD_STACKSIZE        512

//LWIP����ѡ��
#define LWIP_DEBUG                    	 0	 //�ر�DEBUGѡ��
#define ICMP_DEBUG                      LWIP_DBG_OFF //����/�ر�ICMPdebug

#endif /* __LWIPOPTS_H__ */

