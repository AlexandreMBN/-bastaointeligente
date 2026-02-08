#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// FreeRTOS mode (MUST be 0 for sys architecture)
#define NO_SYS                      0
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// Network config
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_DHCP                   1
#define LWIP_TIMEVAL_PRIVATE        0

// FreeRTOS adjustments
#define SYS_LIGHTWEIGHT_PROT        1
#define MEM_ALIGNMENT               4

// Thread config  
#define TCPIP_THREAD_STACKSIZE      2048
#define TCPIP_THREAD_PRIO           (configMAX_PRIORITIES - 2)
#define TCPIP_MBOX_SIZE             16
#define DEFAULT_TCP_RECVMBOX_SIZE   16
#define DEFAULT_UDP_RECVMBOX_SIZE   16
#define DEFAULT_RAW_RECVMBOX_SIZE   16
#define DEFAULT_ACCEPTMBOX_SIZE     16

// TCP connections
#ifndef MEMP_NUM_TCP_PCB
#define MEMP_NUM_TCP_PCB 16
#endif

// Reduce dead connection time
#ifndef TCP_MSL
#define TCP_MSL 500UL
#endif

// CYW43 specific
#define CYW43_TASK_STACK_SIZE       2048
#define CYW43_TASK_PRIORITY         (configMAX_PRIORITIES - 3)

#endif
