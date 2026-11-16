#include <stdio.h>
#include "lwip/err.h"
#include "lwip/tcp.h"
#include "xil_printf.h"
#include "lwip/ip_addr.h"
#include <string.h>
#include <stdint.h>
#include "xiltimer.h"

static uint8_t ecou[7340032];
static uint32_t octetiReceived = 0;
static const uint8_t *dataToSend = NULL;
static uint32_t lenToSend = 0;
static uint32_t octetiSend = 0;
static XTime startTime, finishTime;

static void trimiteBucata(struct tcp_pcb *tpcb)
{
    uint32_t ramas = lenToSend - octetiSend;
    if(ramas == 0)
        return;
    
    uint32_t loc = tcp_sndbuf(tpcb);
    if(loc == 0)
        return;
    
    uint32_t bucata;
    if(ramas <= loc)
        bucata = ramas;
    else
        bucata = loc;
    
    err_t eroareSend = tcp_write(tpcb, dataToSend + octetiSend, bucata, TCP_WRITE_FLAG_COPY);
    if(eroareSend != ERR_OK)
    {
        xil_printf("Eroare la trimitere %d\r\n", eroareSend);
    }
    else
    {
        octetiSend += bucata;
    }
    tcp_output(tpcb);
}

err_t dataSent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    trimiteBucata(tpcb);
    return ERR_OK;    
}

void setDataToSend(const uint8_t *data, uint32_t len){
    dataToSend = data;
    lenToSend = len;
}

err_t connectionStarted(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    xil_printf("Conectat la Jetson\r\n");

    if(dataToSend == NULL || lenToSend == 0)
    {
        xil_printf("Nu am date de trimis\r\n");
        return ERR_OK;
    }
    XTime_GetTime(&startTime);
    trimiteBucata(tpcb);

    return ERR_OK;
}

err_t receiveMessage(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if(p == NULL)
    {
        xil_printf("Jetson-ul a inchis conexiunea\r\n");
        tcp_close(tpcb);
        return ERR_OK;        
    }
    struct pbuf* aux = p;
    while(aux)
    {
        if(octetiReceived + aux->len <= sizeof(ecou))
        {
            memcpy(ecou + octetiReceived, aux->payload, aux->len);
            octetiReceived += aux->len;  
        }          
        aux = aux->next;
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    if(octetiReceived == lenToSend)
    {
        XTime_GetTime(&finishTime);        
        if(memcmp(ecou, dataToSend, lenToSend) == 0)
        {
            uint64_t durataMicrosec = (finishTime - startTime) * 1000000 / COUNTS_PER_SECOND;
            xil_printf("Ecou OK, %d octeti\r\n", lenToSend);
            xil_printf("Timp dus-intors %d microsecunde\r\n", (uint32_t)durataMicrosec);
        }
        else
            xil_printf("Ecou eroare\r\n");
        tcp_close(tpcb);
    }
    return ERR_OK;
}

void connectionFailed(void *arg, err_t err)
{
    xil_printf("Eroare la conexiune %d\r\n", err);
}

void print_app_header()
{
    xil_printf("----- Client TCP Zynq -> Jetson -----\r\n");
}

int start_application()
{
    struct tcp_pcb *connectionPointer = tcp_new();
    if(connectionPointer == NULL)
    {
        xil_printf("Nu mai e memorie pentru tcp\r\n");
        return -1;
    }
    tcp_err(connectionPointer, connectionFailed);
    tcp_recv(connectionPointer, receiveMessage);
    tcp_sent(connectionPointer, dataSent);

    ip_addr_t addressConnection;
    IP4_ADDR(&addressConnection, 192, 168, 1, 20);
    err_t connected = tcp_connect(connectionPointer, &addressConnection, 5000, connectionStarted);
    if(connected != ERR_OK)
    {
        xil_printf("Eroare la pornirea conexiunii %d\r\n", connected);
        tcp_abort(connectionPointer);
        return -1;
    }
    else
        xil_printf("Conectare la 192.168.1.20:5000\r\n");
    return 0;
}

int transfer_data()
{
    return 0;
}