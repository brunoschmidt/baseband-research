/*
 * Copyright (c) 2004, National ICT Australia
 */
/*
 * Copyright (c) 2007 Open Kernel Labs, Inc. (Copyright Holder).
 * All rights reserved.
 *
 * 1. Redistribution and use of OKL4 (Software) in source and binary
 * forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 *     (a) Redistributions of source code must retain this clause 1
 *         (including paragraphs (a), (b) and (c)), clause 2 and clause 3
 *         (Licence Terms) and the above copyright notice.
 *
 *     (b) Redistributions in binary form must reproduce the above
 *         copyright notice and the Licence Terms in the documentation and/or
 *         other materials provided with the distribution.
 *
 *     (c) Redistributions in any form must be accompanied by information on
 *         how to obtain complete source code for:
 *        (i) the Software; and
 *        (ii) all accompanying software that uses (or is intended to
 *        use) the Software whether directly or indirectly.  Such source
 *        code must:
 *        (iii) either be included in the distribution or be available
 *        for no more than the cost of distribution plus a nominal fee;
 *        and
 *        (iv) be licensed by each relevant holder of copyright under
 *        either the Licence Terms (with an appropriate copyright notice)
 *        or the terms of a licence which is approved by the Open Source
 *        Initative.  For an executable file, "complete source code"
 *        means the source code for all modules it contains and includes
 *        associated build and other files reasonably required to produce
 *        the executable.
 *
 * 2. THIS SOFTWARE IS PROVIDED ``AS IS'' AND, TO THE EXTENT PERMITTED BY
 * LAW, ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED.  WHERE ANY WARRANTY IS
 * IMPLIED AND IS PREVENTED BY LAW FROM BEING DISCLAIMED THEN TO THE
 * EXTENT PERMISSIBLE BY LAW: (A) THE WARRANTY IS READ DOWN IN FAVOUR OF
 * THE COPYRIGHT HOLDER (AND, IN THE CASE OF A PARTICIPANT, THAT
 * PARTICIPANT) AND (B) ANY LIMITATIONS PERMITTED BY LAW (INCLUDING AS TO
 * THE EXTENT OF THE WARRANTY AND THE REMEDIES AVAILABLE IN THE EVENT OF
 * BREACH) ARE DEEMED PART OF THIS LICENCE IN A FORM MOST FAVOURABLE TO
 * THE COPYRIGHT HOLDER (AND, IN THE CASE OF A PARTICIPANT, THAT
 * PARTICIPANT). IN THE LICENCE TERMS, "PARTICIPANT" INCLUDES EVERY
 * PERSON WHO HAS CONTRIBUTED TO THE SOFTWARE OR WHO HAS BEEN INVOLVED IN
 * THE DISTRIBUTION OR DISSEMINATION OF THE SOFTWARE.
 *
 * 3. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ANY OTHER PARTICIPANT BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Authors: Carl van Schaik 
 */

#include "uart_8250.h"
#include "xscale.h"
#include <l4/kdebug.h>

// FIXME: make framework deal with other variants of the 8250

enum parity { PARITY_NONE, PARITY_EVEN, PARITY_ODD, PARITY_MARK, PARITY_SPACE };


#define DEFAULT_BAUD            115200
#define DEFAULT_SIZE            8
#define DEFAULT_PARITY          PARITY_NONE
#define DEFAULT_STOP            1

#define CAP_FIFO                (1 << 0)
#define CAP_CHAR_TIMEOUT        (1 << 1)
#define CAP_XSCALE              (1 << 2)
#define CAP_INT_OUT2            (1 << 3)

/* Seven or eight bits for parity */
/* One or two stop bits */
int serial_set_params(struct uart_8250 *self, unsigned baud, int data_size,
                      enum parity parity, int stop_bits);

static int
device_setup_impl(struct device_interface *di, struct uart_8250 *device,
                  struct resource *resources)
{
    /* FIXME: This can be autogenerated, so we should do that */
    int i, n_mem = 0;
    for (i = 0; i < 8; i++)
    {
        switch (resources->type)
        {
        case MEMORY_RESOURCE:
            if (n_mem == 0)
                device->main = *resources;
            else
                printf("vtimer: got more memory than expected!\n");
            n_mem++;
            break;
            
        case INTERRUPT_RESOURCE:
        case BUS_RESOURCE:
        case NO_RESOURCE:
            /* do nothing */
            break;
            
        default:
            printf("vtimer: Invalid resource type %d!\n", resources->type);
            break;
        }
        resources++;
    }
    
    device->tx.device = device;
    device->rx.device = device;
    device->tx.ops = stream_ops;
    device->rx.ops = stream_ops;

    device->txen = 0;
    device->writec = 0;

    device->rxen = 0;
    device->readc = 0;

    serial_set_params(device, DEFAULT_BAUD, DEFAULT_SIZE, DEFAULT_PARITY,
                      DEFAULT_STOP);
    return DEVICE_SUCCESS;
}

/* Seven or eight bits for parity */
/* One or two stop bits */
int
serial_set_params(struct uart_8250 *device, unsigned baud, int data_size,
                  enum parity parity, int stop_bits /* , int mode */)
{
#if 0
    int brd = BAUD_RATE_CONSTANT / (16 * baud) - 1;
#endif

    if (data_size < 5 || data_size > 8) {
        /* Invalid data_size */
        return -1;
    }

    if (stop_bits < 1 || stop_bits > 2) {
        /* Invalid # of stop bits */
        return -1;
    }
#if 0
    uart_pe_set(parity != PARITY_NONE);
    uart_oes_set(parity == PARITY_EVEN);
    uart_sbs_set(stop_bits == 1);
    uart_dss_set(data_size == 7);

    uart_brd_hi_set((brd >> 8) & 0xF);
    uart_brd_lo_set(brd & 0xFF);

    // uart_brd_set(brd);
#endif

    /* Acknowledge any pending ints */
    lsr_read();
    rbr_read();
    iir_read();
    msr_read();

    if (UART_CAPABILITIES & CAP_FIFO) {
        fcr_write(FCR_FIFOEN | FCR_FRXRST | FCR_FTXRST | FCR_ITL(0));
    }

    if (UART_CAPABILITIES & CAP_XSCALE) {
        /* Enable the UART */
        ier_set_uue(1);
        /* We don't use DMA or NRZ */
        ier_set_dmae(0);
        ier_set_nrze(0);
    }

    /* Disable interrupts */
    ier_set_rdie(1);
    ier_set_txie(0);
    ier_set_rlsie(0);
    ier_set_msie(0);

    /* Disable loopback */
    mcr_set_loop(0);

    if (UART_CAPABILITIES & CAP_INT_OUT2) {
        /* Enable interrupt to CPU */
        mcr_set_out2(1);
    }

    if (UART_CAPABILITIES & CAP_CHAR_TIMEOUT) {
        ier_set_rtoie(0);
    }

    return 0;
}

static int
device_enable_impl(struct device_interface *di, struct uart_8250 *device)
{
    device->state = STATE_ENABLED;
    return DEVICE_ENABLED;
}

static int
device_disable_impl(struct device_interface *di, struct uart_8250 *device)
{
    /* We don't actually turn off the UART here, as the kernel may still
       be using it */
    device->state = STATE_DISABLED;
    return DEVICE_DISABLED;
}

static int
device_poll_impl(struct device_interface *di, struct uart_8250 *device)
{
    return 0;
}


static void
do_xmit_work(struct uart_8250 *device)
{
    struct stream_interface *si = &device->tx;
    struct stream_pkt *packet = stream_get_head(si);

    if (packet == NULL) {
        return;
    }

    while(device->fifodepth){
        device->fifodepth--;
        /* Place the character into the UART FIFO */
        assert(packet->data);
        thr_write(packet->data[packet->xferred++]);
        assert(packet->xferred <= packet->length);
        if (packet->xferred == packet->length) {
            /* Finished this packet */
            packet = stream_switch_head(si);
            if (packet == NULL) {
                break;
            }
        }
    }
    if (device->fifodepth == 0) {
        ier_set_txie(1);
    }
}


static int 
do_rx_work(struct uart_8250 *device)
{   
    int retval = 0;
    struct stream_interface *si = &device->rx;
    struct stream_pkt *packet = stream_get_head(si);

    /* Receive as many charaters as possible */

    while (packet && lsr_get_dr()){
        uint16_t data = rbr_get_data();
        /* 11 is Ctrl-k */
        if ((data&0xff) == 11 ){
            L4_KDB_Enter("breakin");
            // Dont want to pass the C-k to the client
            continue;
        }

        packet->data[packet->xferred] = (data & 0xff);
        packet->xferred++;
        if (packet->xferred == packet->length) {
            packet = stream_switch_head(si);
            if (packet == NULL) {

                // empty the fifo by dropping the chars if we
                // have no space to put them
                while(lsr_get_dr())
                    rbr_get_data();

                break;
            }
        }
    }

    if (packet){
        ier_set_rdie(1);   /* restart the interrupt */
        if (packet->xferred){
            packet = stream_switch_head(si);
        }
    }

    return retval;
}

/*
 * FIXME: dd_dsl.py should generate these.. 
 */

static int
device_interrupt_impl(struct device_interface *di, struct uart_8250 *device, int irq)
{
    uint32_t r;
    int status=0;
    r = iir_read();

    if (!r&1) {
        printf("Got irq, but none pending?\n");
    }

    r = lsr_read();

    // XXX: fix the irq storm that happens when this is uncommented
    // printf("uart_8250: irq: %d, r: %lx\n", irq, r);

    if (r&0x1){ // RX
        ier_set_rdie(0);
        status = do_rx_work(device);
    }
    if (r&0x20){
        ier_set_txie(0);   /* stop the interrupt */
        device->fifodepth = TX_FIFO_DEPTH;
        do_xmit_work(device);
    }

    return status;
}

static int
stream_sync_impl(struct stream_interface *si, struct uart_8250 *device)
{
    int retval = 0;

    if (si == &device->tx) {
        do_xmit_work(device);
    }
    if (si == &device->rx) {
        retval = do_rx_work(device);
    }
    return retval;
}

/* FIXME: This can be autogenerated! */
static int
device_num_interfaces_impl(struct device_interface *di, struct uart_8250 *device)
{
    return 2;
}

/* FIXME: This can definately be autogenerated */
static struct generic_interface *
device_get_interface_impl(struct device_interface *di, struct uart_8250 *device, int i)
{
    switch (i) {
    case 0:
        return (struct generic_interface *) &device->tx;
    case 1:
        return (struct generic_interface *) &device->rx;
    default:
        return NULL;
    }
}
