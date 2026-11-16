/*
 * Copyright (C) 2009 - 2022 Xilinx, Inc.
 * Copyright (C) 2022 - 2024 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>

#include "xparameters.h"

#include "netif/xadapter.h"

#include "platform.h"
#include "platform_config.h"
#if defined (__arm__) || defined(__aarch64__)
#include "xil_printf.h"
#endif

#include "lwip/tcp.h"
#include "xil_cache.h"

#include "ff.h"
#include "bmp.h"
#include "protocol.h"
#include <string.h>

#if LWIP_IPV6==1
#include "lwip/ip.h"
#else
#if LWIP_DHCP==1
#include "lwip/dhcp.h"
#endif
#endif

/* defined by each RAW mode application */
void print_app_header();
int start_application();
int transfer_data();
void tcp_fasttmr(void);
void tcp_slowtmr(void);

/* missing declaration in lwIP */
void lwip_init();

#if LWIP_IPV6==0
#if LWIP_DHCP==1
extern volatile int dhcp_timoutcntr;
err_t dhcp_start(struct netif *netif);
#endif
#endif

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;
static struct netif server_netif;
struct netif *echo_netif;

#if LWIP_IPV6==1
void print_ip6(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf(" %x:%x:%x:%x:%x:%x:%x:%x\n\r",
		   IP6_ADDR_BLOCK1(&ip->u_addr.ip6),
		   IP6_ADDR_BLOCK2(&ip->u_addr.ip6),
		   IP6_ADDR_BLOCK3(&ip->u_addr.ip6),
		   IP6_ADDR_BLOCK4(&ip->u_addr.ip6),
		   IP6_ADDR_BLOCK5(&ip->u_addr.ip6),
		   IP6_ADDR_BLOCK6(&ip->u_addr.ip6),
		   IP6_ADDR_BLOCK7(&ip->u_addr.ip6),
		   IP6_ADDR_BLOCK8(&ip->u_addr.ip6));

}
#else
void print_ip(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
		   ip4_addr3(ip), ip4_addr4(ip));
}

void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}
#endif

#if defined (__arm__) && !defined (ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
int ProgramSi5324(void);
int ProgramSfpPhy(void);
#endif
#endif

#ifdef XPS_BOARD_ZCU102
#if defined (XPAR_XIICPS_0_DEVICE_ID) || defined (XPAR_XIICPS_0_BASEADDR)
int IicPhyReset(void);
#endif
#endif

static FATFS card;
static uint8_t continutFisier[65536];
static uint8_t continutImagine[7340032];
static uint8_t imagineConvertita[7340032];
static uint8_t pachet[sizeof(antet_t) + sizeof(continutFisier) + sizeof(continutImagine)];

void setDataToSend(const uint8_t *data, uint32_t len);


int readFile(const char* file_name, uint8_t* buffer, uint32_t max_len)
{
    FIL fil;
    if(f_open(&fil, file_name, FA_READ) != FR_OK)
    {
        return -1;
    }
    uint32_t fileLength = f_size(&fil);
    if(fileLength > max_len)
    {
        f_close(&fil);
        return -2;
    }
    UINT ct;
    if(f_read(&fil, buffer, fileLength, &ct) != FR_OK || ct != fileLength)
    {
        f_close(&fil);
        return -3;
    }
    f_close(&fil);
    return (int)fileLength;
}

int writeFile(const char *file_name, const uint8_t *buffer, uint32_t len)
{
    FIL fil;
    if(f_open(&fil, file_name, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        return -1;
    }
    UINT octetiScrisi;
    if(f_write(&fil, buffer, len, &octetiScrisi) != FR_OK || octetiScrisi != len)
    {
        f_close(&fil);
        return -1;
    }
    f_close(&fil);
    return len;
}

void proceseazaRaspuns(const uint8_t *date, uint32_t len)
{
    if(len < sizeof(antet_t))
    {
        xil_printf("Nu exista un antet intreg\r\n");
        return;        
    }    
    antet_t antet;
    memcpy(&antet, date, sizeof(antet_t));
    if(antet.magic != MAGIC_SSC || sizeof(antet_t) + antet.lenText + antet.lenImagine != len)
    {
        xil_printf("Pachet invalid\r\n");
        return;
    }

    const uint8_t *dateText = date + sizeof(antet_t), *dateImagine = date + sizeof(antet_t) + antet.lenText;
    for(uint32_t i = 0; i < antet.lenText; i++)
    {
        xil_printf("%c", dateText[i]);
    }
    xil_printf("\r\n");
    int outText = writeFile("0:/OUT.TXT", dateText, antet.lenText);
    int outImagine = writeFile("0:/OUT.BMP", dateImagine, antet.lenImagine);
    xil_printf("OUT.TXT salvat: %d octeti\r\n", outText);
    xil_printf("OUT.BMP salvat: %d octeti\r\n", outImagine);
}   

int main()
{
#if LWIP_IPV6==0
	ip_addr_t ipaddr, netmask, gw;

#endif
	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	echo_netif = &server_netif;
#if defined (__arm__) && !defined (ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
	ProgramSi5324();
	ProgramSfpPhy();
#endif
#endif

	/* Define this board specific macro in order perform PHY reset on ZCU102 */
#ifdef XPS_BOARD_ZCU102
	if (IicPhyReset()) {
		xil_printf("Error performing PHY reset \n\r");
		return -1;
	}
#endif

	init_platform();

    FRESULT res = f_mount(&card, "0:/", 1);
    if(res != FR_OK)
    {
        xil_printf("Cardul nu s-a montat %d\r\n", res);
    }
    else
    {
        xil_printf("Cardul s-a montat\r\n");

        int octetiText = readFile("0:/TEXT.TXT", continutFisier, sizeof(continutFisier));
        if(octetiText < 0)
        {
            xil_printf("Fisierul text nu s-a citit\r\n");
        }

        int octetiImagine = readFile("0:/IMG.BMP", continutImagine, sizeof(continutImagine));
        if(octetiImagine < 0)
        {
            xil_printf("Fisierul imagine nu s-a citit\r\n");
        }

        xil_printf("Din fisierul text s-au citit %d bytes\r\n", octetiText);
        xil_printf("Din fisierul imagine s-au citit %d bytes\r\n", octetiImagine);

        if(octetiImagine > 0)
        {
            bmp_info_t infoFisier;
            if(bmp_parse(continutImagine, octetiImagine, &infoFisier) < 0)
                xil_printf("Fisierul imagine nu s-a parsat\r\n");
            else
            {
                bmp_to_rgb(continutImagine, &infoFisier, imagineConvertita);
                xil_printf("Imagine %d x %d\r\n", infoFisier.width, infoFisier.height);
            }
        }

        if(octetiText > 0 && octetiImagine > 0)
        {
            antet_t antet;
            antet.magic = MAGIC_SSC;
            antet.lenText = octetiText;
            antet.lenImagine = octetiImagine;

            memcpy(pachet, &antet, sizeof(antet_t));
            memcpy(pachet + sizeof(antet_t), continutFisier, octetiText);
            memcpy(pachet + sizeof(antet_t) + octetiText, continutImagine, octetiImagine);

            int total = sizeof(antet_t) + octetiText + octetiImagine;
            setDataToSend(pachet, total);
        }
    }

#if LWIP_IPV6==0
#if LWIP_DHCP==1
	ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;
#else
	/* initialize IP addresses to be used */
	IP4_ADDR(&ipaddr,  192, 168,   1, 10);
	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   1,  1);
#endif
#endif
	print_app_header();

	lwip_init();

#if (LWIP_IPV6 == 0)
	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(echo_netif, &ipaddr, &netmask,
		       &gw, mac_ethernet_address,
		       PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}
#else
	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(echo_netif, NULL, NULL, NULL, mac_ethernet_address,
		       PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}
	echo_netif->ip6_autoconfig_enabled = 1;

	netif_create_ip6_linklocal_address(echo_netif, 1);
	netif_ip6_addr_set_state(echo_netif, 0, IP6_ADDR_VALID);

	print_ip6("\n\rBoard IPv6 address ", &echo_netif->ip6_addr[0].u_addr.ip6);

#endif
	netif_set_default(echo_netif);

#ifndef SDT
	/* now enable interrupts */
	platform_enable_interrupts();
#endif

	/* specify that the network if is up */
	netif_set_up(echo_netif);

#if (LWIP_IPV6 == 0)
#if (LWIP_DHCP==1)
	/* Create a new DHCP client for this interface.
	 * Note: you must call dhcp_fine_tmr() and dhcp_coarse_tmr() at
	 * the predefined regular intervals after starting the client.
	 */
	dhcp_start(echo_netif);
	dhcp_timoutcntr = 240;

	while (((echo_netif->ip_addr.addr) == 0) && (dhcp_timoutcntr > 0)) {
		xemacif_input(echo_netif);
	}

	if (dhcp_timoutcntr <= 0) {
		if ((echo_netif->ip_addr.addr) == 0) {
			xil_printf("DHCP Timeout\r\n");
			xil_printf("Configuring default IP of 192.168.1.10\r\n");
			IP4_ADDR(&(echo_netif->ip_addr),  192, 168,   1, 10);
			IP4_ADDR(&(echo_netif->netmask), 255, 255, 255,  0);
			IP4_ADDR(&(echo_netif->gw),      192, 168,   1,  1);
		}
	}

	ipaddr.addr = echo_netif->ip_addr.addr;
	gw.addr = echo_netif->gw.addr;
	netmask.addr = echo_netif->netmask.addr;
#endif

	print_ip_settings(&ipaddr, &netmask, &gw);

#endif
	/* start the application (web server, rxtest, txtest, etc..) */
	start_application();

	/* receive and process packets */
	while (1) {
		if (TcpFastTmrFlag) {
			tcp_fasttmr();
			TcpFastTmrFlag = 0;
		}
		if (TcpSlowTmrFlag) {
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;
		}
		xemacif_input(echo_netif);
		transfer_data();
	}

	/* never reached */
	cleanup_platform();

	return 0;
}
