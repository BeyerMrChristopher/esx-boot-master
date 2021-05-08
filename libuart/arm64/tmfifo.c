/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tmfifo.c -- Virtual console over NVIDIA BlueField RSHIM interface.
 */

#include <stdint.h>
#include <io.h>
#include <uart.h>

#define TMFIFO_MSG_CONSOLE  3

#define TILE_TO_HOST_DATA   0xa40
#define TILE_TO_HOST_STATUS 0xa48
#define TILE_TO_HOST_CTL    0xa50
#define SCRATCHPAD1         0xc20

#define FIFO_LENGTH         256

typedef union {
  struct {
    uint8_t type;      /* message type. */
    uint8_t len_hi;    /* payload length, high 8 bits. */
    uint8_t len_lo;    /* payload length, low 8 bits. */
    uint8_t unused[5]; /* reserved, set to 0. */
  };
  uint64_t data;
} tmfifo_msg_header_t;

const tmfifo_msg_header_t tx_header = {
  .type = TMFIFO_MSG_CONSOLE,
  .len_lo = 1,
};

/*-- tmfifo_connected ----------------------------------------------------------
 *
 *      Returns whether the remote end (rshim driver) is present. If the
 *      remote end is not present, the TX FIFO will never empty.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *
 * Results
 *      True if the remote end (rshim driver) is present.
 *----------------------------------------------------------------------------*/
static bool tmfifo_connected(const uart_t *dev)
{
   return io_read64(&dev->io, SCRATCHPAD1) != 0;
}

/*-- tmfifo_full ---------------------------------------------------------------
 *
 *      Returns whether the TX FIFO is full.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *
 * Results
 *      True if TXFIFO if full. False otherwise.
 *----------------------------------------------------------------------------*/
static bool tmfifo_full(const uart_t *dev)
{
   /*
    * Full means we can't stuff another two packets (message header and the
    * actual data).
    */
   return io_read64(&dev->io, TILE_TO_HOST_STATUS) > (FIFO_LENGTH - 2);
}

/*-- tmfifo_putc ---------------------------------------------------------------
 *
 *      Write a character to the TMFIFO console.
 *
 * Parameters
 *      IN dev: pointer to a UART descriptor
 *      IN c:   character to be written
 *----------------------------------------------------------------------------*/
static void tmfifo_putc(const uart_t *dev, char c)
{
   uint16_t timeout;

   if (!tmfifo_connected(dev)) {
      return;
   }

   for (timeout = 0xffff; timeout > 0; timeout--) {
      if (!tmfifo_full(dev)) {
         io_write64(&dev->io, TILE_TO_HOST_DATA, tx_header.data);
         io_write64(&dev->io, TILE_TO_HOST_DATA, c);
         return;
      }
   }
}

/*-- tmfifo_init --------------------------------------------------------------
 *
 *      Prepares a TMFIFO console.
 *
 * Parameters
 *      IN dev:      pointer to a UART descriptor
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int tmfifo_init(uart_t *dev)
{
   if (dev->type != SERIAL_TMFIFO) {
      return ERR_UNSUPPORTED;
   }

   dev->putc = tmfifo_putc;
   dev->flags = UART_USE_AFTER_EXIT_BOOT_SERVICES;
   return ERR_SUCCESS;
}
