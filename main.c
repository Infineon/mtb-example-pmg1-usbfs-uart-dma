/******************************************************************************
* File Name: main.c
*
* Description: This code example will demonstrate USB to UART and vice versa data transfer using DMA.
* The USB will be configured as CDC Class. The data received via the USB OUT endpoint will be transferred using DMA To UART TX buffer.
* The UART Tx and UART Rx pins will be connected. Data received via UART RX buffer is transferred to USB IN endpoint via DMA.
* This code example shows how to use DMA between two peripherals.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/


/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_usb_dev.h"
#include "cy_usb_dev_cdc.h"
#include "cycfg_usbdev.h"

#include "usb_uart_dma.h"
#include "cy_trigmux.h"

/*******************************************************************************
 * Macros
 ********************************************************************************/
#define USB_BUFFER_SIZE (16u)
#define PING_PONG_BUF_SIZE (8u)
#define USB_COM_PORT    (0u)
#define USB_EP_3_OUT    (3u)
#define USB_EP_2_IN     (2u)

/*******************************************************************************
 * Function Prototypes
 ********************************************************************************/
static void usb_high_isr(void);
static void usb_medium_isr(void);
static void usb_low_isr(void);
static void dma_isr(void);
static void uart_isr(void);
void handle_error(void);

/*******************************************************************************
 * Global Variables
 ********************************************************************************/

/* USB Interrupt Configuration */
const cy_stc_sysint_t usb_high_interrupt_cfg =
{
        .intrSrc = (IRQn_Type) usb_interrupt_hi_IRQn,
        .intrPriority = 0U,
};
const cy_stc_sysint_t usb_medium_interrupt_cfg =
{
        .intrSrc = (IRQn_Type) usb_interrupt_med_IRQn,
        .intrPriority = 1U,
};
const cy_stc_sysint_t usb_low_interrupt_cfg =
{
        .intrSrc = (IRQn_Type) usb_interrupt_lo_IRQn,
        .intrPriority = 2U,
};

/* DMA Interrupt Configuration */
const cy_stc_sysint_t dma_intr_cfg =
{
    .intrSrc = (IRQn_Type)cpuss_interrupt_dma_IRQn,
    .intrPriority = 3U,
};

/* UART Interrupt Configuration */
const cy_stc_sysint_t UART_INT_cfg =
{
    .intrSrc = (IRQn_Type)CYBSP_UART_IRQ,
    .intrPriority = 3U,
};

/* USBDEV context variables */
cy_stc_usbfs_dev_drv_context_t usb_drvContext;
cy_stc_usb_dev_context_t usb_devContext;
cy_stc_usb_dev_cdc_context_t usb_cdcContext;

/* UART context variables */
cy_stc_scb_uart_context_t uart_Context;

/* Array containing the data bytes received from host */
uint8_t tx_buffer[USB_BUFFER_SIZE];
uint8_t rx_buffer_ping[8];
uint8_t rx_buffer_pong[8];

/* Flag for error status of DMA, UART, USB */
bool dma_chan_10_error;
bool dma_chan_0_error;
bool dma_chan_1_error;
bool dma_chan_9_error;
bool cdc_error;
bool uart_error;

/* Variable for USBFS Device Driver return codes.*/
cy_en_usbfs_dev_drv_status_t dev_drv_status;

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 *  This is the main function. It initializes the USB Device block
 *  and enumerates as a CDC device. It is constantly checking for data
 *  received from host and echos it back .
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 *******************************************************************************/
int main(void)
{

    cy_rslt_t result;
    cy_en_usb_dev_status_t status;
    cy_en_scb_uart_status_t uart_status;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize the USB device */
    status = Cy_USB_Dev_Init(CYBSP_USB_HW, &CYBSP_USB_config, &usb_drvContext, &usb_devices[0], &usb_devConfig, &usb_devContext);
    if (status != CY_USB_DEV_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize the CDC Class */
    status = Cy_USB_Dev_CDC_Init(&usb_cdcConfig, &usb_cdcContext, &usb_devContext);
    if (status != CY_USB_DEV_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize and enable UART operation */
    uart_status = Cy_SCB_UART_Init(CYBSP_UART_HW, &CYBSP_UART_config, &uart_Context);
    if (uart_status != CY_SCB_UART_SUCCESS )
    {
        CY_ASSERT(0);
    }
    Cy_SCB_UART_Enable(CYBSP_UART_HW);

    /* Configure UART_TX_DMA and UART_RX_DMA channels for operation */
    configure_tx_dma(tx_buffer, (void *) &(CYBSP_UART_HW->TX_FIFO_WR));
    configure_rx_dma((void *) &(CYBSP_UART_HW->RX_FIFO_RD), rx_buffer_ping, rx_buffer_pong);

    /* Initialize interrupts */
    Cy_SysInt_Init(&usb_high_interrupt_cfg, &usb_high_isr);
    Cy_SysInt_Init(&usb_medium_interrupt_cfg, &usb_medium_isr);
    Cy_SysInt_Init(&usb_low_interrupt_cfg, &usb_low_isr);
    Cy_SysInt_Init(&dma_intr_cfg,   &dma_isr);
    Cy_SysInt_Init(&UART_INT_cfg, &uart_isr);

    /* Enable interrupts */
    NVIC_EnableIRQ(usb_high_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_medium_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_low_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(dma_intr_cfg.intrSrc);
    NVIC_EnableIRQ(UART_INT_cfg.intrSrc);

    /* Make device appear on the bus. This function call is blocking,
     * it waits until the device enumerates */
    Cy_USB_Dev_Connect(true, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);

    /* Enable Interrupt for DMA channels. Must come after Cy_USB_Dev_Connect */
    /* CY_DMAC_INTR_CHAN_0:
     * Interrupt for DMA Channel 0 between user SRAM TX Buffer and UART TX FIFO
     *
     * CY_DMAC_INTR_CHAN_1:
     * Interrupt for DMA Channel 1 between UART RX FIFO and user SRAM RX Ping and Pong Buffers
     *
     * CY_DMAC_INTR_CHAN_10:
     * Interrupt for DMA Channel 10 between Out EP3 in USBFS Block and EP3 Buffer in Driver SRAM buffer
     *
     */
    Cy_DMAC_SetInterruptMask(DMAC, CY_DMAC_INTR_CHAN_1 | CY_DMAC_INTR_CHAN_10 | CY_DMAC_INTR_CHAN_0);


    for (;;)
    {
        /* Check if there are any errors */
        if (uart_error | cdc_error | dma_chan_10_error | dma_chan_0_error | dma_chan_1_error)
        {
            /* If error condition turn on LED3 and halt processor */
            handle_error();
        }

    }

}

/*******************************************************************************
* Function Name: dma_isr
********************************************************************************
*
* Summary:
*  Interrupt Handler for DMA channels 10, 1, and 0.
*
*  DMA Channel 10:
*  Initiates data transfer from driver SRAM Endpoint buffer (OUT) to user SRAM tx_buffer.
*  Triggers UART_TX_DMA DMA channel to initiate data transfer to UART TX FIFO.
*
*  DMA Channel 1:
*  If current active descriptor is pong, initiate data transfer from ping buffer to driver SRAM Endpoint buffer (IN).
*  If current active descriptor is ping, initiate data transfer from pong buffer instead.
*  Note that this is because upon DMA transfer completion, the active descriptor is flipped if flipping is enabled.
*
*  DMA Channel 0:
*  Handles data transfer from SRAM tx_buffer to UART TX FIFO. DMA Channel 0 is triggered by DMA Channel 10 data transfer completion.
*
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void dma_isr(void)
{

    cy_en_dmac_descriptor_t descriptor;
    cy_en_dmac_response_t dmac_response;

    /* Stores number of data bytes received from host */
    uint32_t ep_out_num_bytes;

    /* Get interrupt source. */
    uint32_t dma_intr_src = Cy_DMAC_GetInterruptStatusMasked(DMAC);

    /* Check if interrupt was triggered for DMAC Channel 10 */
    if (dma_intr_src & CY_DMAC_INTR_CHAN_10)
    {
        /* Initiate DMA data transfer from Driver SRAM Endpoint buffer (out) to tx_buffer.
         * Max number of bytes transferable in a single descriptor is 16 bytes.
         * Number of bytes actually transferred is stored in ep_out_num_bytes */
        dev_drv_status = Cy_USBFS_Dev_Drv_ReadOutEndpoint(CYBSP_USB_HW, 3, tx_buffer, USB_BUFFER_SIZE, &ep_out_num_bytes, &usb_drvContext);

        /* Status is checked to ensure data was transferred successfully. */
        if (dev_drv_status != CY_USBFS_DEV_DRV_SUCCESS)
        {
            dma_chan_10_error = true;
        }

        /* Set data size of UART_TX_DMA descriptor based on the number of actual transferred bytes */
        Cy_DMAC_Descriptor_SetDataCount(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, ep_out_num_bytes);

        /* Validate the PING descriptor */
        Cy_DMAC_Descriptor_SetState(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, true);

        /* Enable TxDma channel */
        Cy_DMAC_Channel_Enable(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL);

        /* Clear DMAC Channel 10 Interrupt */
        Cy_DMAC_ClearInterrupt(DMAC, CY_DMAC_INTR_CHAN_10);
    }

    /* Check if interrupt was triggered for DMAC Channel 1 */
    if (dma_intr_src & CY_DMAC_INTR_CHAN_1)
    {

        /* Find what the current active descriptor is for UART_RX_DMA channel
         * Note that upon DMA transfer completion, the descriptor is flipped if flipping is enabled. */
        descriptor = Cy_DMAC_Channel_GetCurrentDescriptor(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL);

        /* Check if the UART_RX_DMA channel response is successful for current transfer */
        dmac_response = Cy_DMAC_Descriptor_GetResponse(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL,
                       ((descriptor == CY_DMAC_DESCRIPTOR_PING) ? CY_DMAC_DESCRIPTOR_PONG : CY_DMAC_DESCRIPTOR_PING));

        if(dmac_response == CY_DMAC_DONE)
        {

            /* Check if the COM port (CDC data interface) is ready to transmit data */
            if (1u == Cy_USB_Dev_CDC_IsReady(USB_COM_PORT, &usb_cdcContext))
            {

                /* If active descriptor is pong that means current transfer is ping and that we wish to transfer data from ping buffer.
                 * Note that this is because upon DMA transfer completion, the active descriptor is flipped if flipping is enabled. */
                if (descriptor == CY_DMAC_DESCRIPTOR_PONG)
                {
                    /* Initiate DMA data transfer from ping buffer to Driver SRAM Endpoint buffer (in). */
                    dev_drv_status = Cy_USBFS_Dev_Drv_LoadInEndpoint(CYBSP_USB_HW, 2, rx_buffer_ping, PING_PONG_BUF_SIZE, &usb_drvContext);
                }
                else
                {
                    /* Initiate DMA data transfer from pong buffer to Driver SRAM Endpoint buffer (in). */
                    dev_drv_status = Cy_USBFS_Dev_Drv_LoadInEndpoint(CYBSP_USB_HW, 2, rx_buffer_pong, PING_PONG_BUF_SIZE, &usb_drvContext);
                }

                /* Status is checked to ensure data was transferred successfully. */
                if (dev_drv_status != CY_USBFS_DEV_DRV_SUCCESS)
                {
                    dma_chan_9_error = true;
                }
            }
            /* If COM port is not ready, error flag is raised. */
            else
            {
                cdc_error = true;
            }
        }
        /* If current transfer did not return done response, error flag is raised */
        else
        {
            dma_chan_1_error = true;
        }

        /* Clear DMAC Channel 1 Interrupt */
        Cy_DMAC_ClearInterrupt(DMAC, CY_DMAC_INTR_CHAN_1);
    }

    /* Check if interrupt was triggered for DMAC Channel 0 */
    if (dma_intr_src & CY_DMAC_INTR_CHAN_0)
    {

        /* Check if UART_TX_DMA channel response is successful for current transfer. Note that
         * current descriptor is set to invalid after completion. */
        dmac_response = Cy_DMAC_Descriptor_GetResponse(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING);
        if (dmac_response != CY_DMAC_DONE && dmac_response != CY_DMAC_INVALID_DESCR)
        {
            dma_chan_0_error = true;
        }

        /* Clear DMAC Channel 0 Interrupt */
        Cy_DMAC_ClearInterrupt(DMAC, CY_DMAC_INTR_CHAN_0);
    }


}

/*******************************************************************************
* Function Name: uart_isr
********************************************************************************
*
* Summary:
* Handles Rx Overflow, Rx Underflow, and Tx Overflow conditions. These conditions
* must never occur. Also handles a Tx Done condition which triggers the
* re-enable of USB Endpoint 3 (out).
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void uart_isr(void)
{
    /* Get RX and TX interrupt sources */
    uint32_t rx_intr_src =  Cy_SCB_UART_GetRxFifoStatus(CYBSP_UART_HW);
    uint32_t tx_intr_src = Cy_SCB_UART_GetTxFifoStatus(CYBSP_UART_HW);

    if (tx_intr_src & CY_SCB_UART_TX_DONE)
    {
        /* After UART TX is done, re-enable USB Endpoint 3 */
        Cy_USBFS_Dev_Drv_EnableOutEndpoint(CYBSP_USB_HW, USB_EP_3_OUT, &usb_drvContext);
    }

    if (rx_intr_src & CY_SCB_UART_RX_OVERFLOW)
    {
        uart_error = true;
    }
    if (rx_intr_src & CY_SCB_UART_RX_UNDERFLOW)
    {
        uart_error = true;
    }
    if (tx_intr_src & CY_SCB_UART_TX_OVERFLOW)
    {
        uart_error = true;
    }

    Cy_SCB_UART_ClearRxFifoStatus(CYBSP_UART_HW, rx_intr_src);
    Cy_SCB_UART_ClearTxFifoStatus(CYBSP_UART_HW, tx_intr_src);
}


/***************************************************************************
 * Function Name: usb_high_isr
 ********************************************************************************
 * Summary:
 *  This function processes the high priority USB interrupts.
 *
 ***************************************************************************/
static void usb_high_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USB_HW, Cy_USBFS_Dev_Drv_GetInterruptCauseHi(CYBSP_USB_HW), &usb_drvContext);
}

/***************************************************************************
 * Function Name: usb_medium_isr
 ********************************************************************************
 * Summary:
 *  This function processes the medium priority USB interrupts.
 *
 ***************************************************************************/
static void usb_medium_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USB_HW, Cy_USBFS_Dev_Drv_GetInterruptCauseMed(CYBSP_USB_HW), &usb_drvContext);
}

/***************************************************************************
 * Function Name: usb_low_isr
 ********************************************************************************
 * Summary:
 *  This function processes the low priority USB interrupts.
 *
 **************************************************************************/
static void usb_low_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USB_HW, Cy_USBFS_Dev_Drv_GetInterruptCauseLo(CYBSP_USB_HW), &usb_drvContext);
}

/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* User defined error handling function
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void handle_error(void)
{
    /* Disable all interrupts. */
   __disable_irq();

   /* Turn on LED3 indicating Error condition */
   Cy_GPIO_Write(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN, 0);

   /* Halt processor */
   CY_ASSERT(0);
}


/* [] END OF FILE */
