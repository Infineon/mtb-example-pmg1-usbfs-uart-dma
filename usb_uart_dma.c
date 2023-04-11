/******************************************************************************
* File Name: usb_uart_dma.c
*
* Description: This file contains all the function prototypes required for proper
*              operation of UART/DMA for this CE
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

#include "cy_pdl.h"
#include "cybsp.h"
#include "usb_uart_dma.h"

/*******************************************************************************
*            Forward declaration
*******************************************************************************/
extern void handle_error(void);


/*******************************************************************************
* Function Name: configure_tx_dma
********************************************************************************
*
* Summary:
* Configures DMA Tx channel for operation.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void configure_tx_dma(uint8_t *rxBuffer, uint8_t *txBuffer)
{
    cy_en_dmac_status_t dmac_init_status;

    /* Initialize PING descriptor */
    dmac_init_status = Cy_DMAC_Descriptor_Init(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, &UART_TX_DMA_ping_config);
    if (dmac_init_status != CY_DMAC_SUCCESS)
    {
        handle_error();
    }

    /* Initialize UART_TX_DMA channel */
    dmac_init_status = Cy_DMAC_Channel_Init(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, &UART_TX_DMA_channel_config);
    if (dmac_init_status != CY_DMAC_SUCCESS)
    {
        handle_error();
    }

    /* Set source and destination for PING descriptor */
    Cy_DMAC_Descriptor_SetSrcAddress(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, (void *) rxBuffer);
    Cy_DMAC_Descriptor_SetDstAddress(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, (void *) txBuffer);

    /* Validate the PING descriptor */
    Cy_DMAC_Descriptor_SetState(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, true);

    /* Set PING descriptor as current descriptor for UART_TX_DMA channel */
    Cy_DMAC_Channel_SetCurrentDescriptor(UART_TX_DMA_HW, UART_TX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING);

    /* Enable DMAC block */
    Cy_DMAC_Enable(UART_TX_DMA_HW);
}


/*******************************************************************************
* Function Name: configure_rx_dma
********************************************************************************
*
* Summary:
* Configures DMA Rx channel for operation.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void configure_rx_dma(uint8_t *rxBuffer, uint8_t *txBuffer_a, uint8_t *txBuffer_b)
{
    cy_en_dmac_status_t dmac_init_status;

    /* Initialize PING descriptor */
    dmac_init_status = Cy_DMAC_Descriptor_Init(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, &UART_RX_DMA_ping_config);
    if (dmac_init_status != CY_DMAC_SUCCESS)
    {
        handle_error();
    }

    /* Initialize PONG descriptor */
    dmac_init_status = Cy_DMAC_Descriptor_Init(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PONG, &UART_RX_DMA_pong_config);
    if (dmac_init_status != CY_DMAC_SUCCESS)
    {
        handle_error();
    }

    /* Initialize UART_RX_DMA channel */
    dmac_init_status = Cy_DMAC_Channel_Init(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, &UART_RX_DMA_channel_config);
    if (dmac_init_status != CY_DMAC_SUCCESS)
    {
        handle_error();
    }

    /* Set source and destination for PING descriptor */
    Cy_DMAC_Descriptor_SetSrcAddress(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, (void *) rxBuffer);
    Cy_DMAC_Descriptor_SetDstAddress(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, (void *) txBuffer_a);

    /* Set source and destination for PONG descriptor */
    Cy_DMAC_Descriptor_SetSrcAddress(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PONG, (void *) rxBuffer);
    Cy_DMAC_Descriptor_SetDstAddress(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PONG, (void *) txBuffer_b);

    /* Validate the PING and PONG descriptors */
    Cy_DMAC_Descriptor_SetState(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING, true);
    Cy_DMAC_Descriptor_SetState(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PONG, true);

    /* Set PING descriptor as current descriptor for UART_RX_DMA channel */
    Cy_DMAC_Channel_SetCurrentDescriptor(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL, CY_DMAC_DESCRIPTOR_PING);

    /* Enable DMAC block */
    Cy_DMAC_Enable(UART_RX_DMA_HW);

    /* Enable UART_RX_DMA channel */
    Cy_DMAC_Channel_Enable(UART_RX_DMA_HW, UART_RX_DMA_CHANNEL);
}
