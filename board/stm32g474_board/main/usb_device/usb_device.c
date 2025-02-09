/*!
 ******************************************************************************
 * @file           : usb_device.c
 * @author         : Sicris Rey Embay
 ******************************************************************************
 */
#include "stdbool.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "timers.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "bsp/board_api.h"
#include "main.h"
#include "cli.h"

#define USB_DEVICE_STACK_SIZE           (384)
#define USB_CLASS_STACK_SIZE            (256)
#define EVENT_CDC_AVAILABLE_BIT         (0x00000001)
#define EVENT_CDC_TRANSMIT_REQ_BIT      (0x00000002)
#define EVENT_CDC_TRANSMIT_DONE_BIT     (0x00000004)

/*
 * Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED     = 500,
    BLINK_SUSPENDED   = 1000,

    BLINK_ALWAYS_ON   = UINT32_MAX,
    BLINK_ALWAYS_OFF  = 0
};

static bool bInit = false;
static bool bMounted = false;

/* USB Device Task */
static TaskHandle_t deviceTask = NULL;
static StackType_t usb_device_stack[USB_DEVICE_STACK_SIZE];
static StaticTask_t usb_device_taskdef;

static TaskHandle_t classTask = NULL;
static StackType_t  usb_class_stack[USB_CLASS_STACK_SIZE];
static StaticTask_t usb_class_taskdef;

static TimerHandle_t blinky_tm = NULL;
static StaticTimer_t blinky_tmdef;

#define TX_STREAM_BUFFER_SIZE_BYTES     (512)
static StreamBufferHandle_t txStreamHandle = NULL;
static uint8_t txStreamStorage[TX_STREAM_BUFFER_SIZE_BYTES + 1] = {0};
static StaticStreamBuffer_t txStreamStruct;
volatile bool txInProgress = false;

static void usb_device_task(void * pxParam);
static void usb_class_task(void * pxParam);
static void led_blinky_cb(TimerHandle_t xTimer);

void usb_device_init(void)
{
    if(!bInit) {
        txStreamHandle = xStreamBufferCreateStatic(
                            TX_STREAM_BUFFER_SIZE_BYTES,
                            1,
                            txStreamStorage,
                            &txStreamStruct);
        blinky_tm = xTimerCreateStatic(
                            "usb blinky",
                            pdMS_TO_TICKS(BLINK_NOT_MOUNTED),
                            true,
                            NULL,
                            led_blinky_cb,
                            &blinky_tmdef
                            );
        deviceTask = xTaskCreateStatic(
                            usb_device_task,
                            "usb-device",
                            USB_DEVICE_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 1,
                            usb_device_stack,
                            &usb_device_taskdef
                            );
        classTask = xTaskCreateStatic(
                            usb_class_task,
                            "usb-class",
                            USB_CLASS_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 1,
                            usb_class_stack,
                            &usb_class_taskdef
                            );

        xTimerStart(blinky_tm, 0);
        bInit = true;
    }
}



//--------------------------------------------------------------------+
// USB Device Task
//--------------------------------------------------------------------+
static void usb_device_task(void * pxParam)
{
    (void)pxParam;
    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    // RTOS forever loop
    while (1) {
        // put this thread to waiting state until there is new events
        tud_task();

        if(tud_cdc_available()) {
            // Set event bit to process cdc task
            xTaskNotify(classTask, EVENT_CDC_AVAILABLE_BIT, eSetBits);
        }
        tud_cdc_write_flush();
    }
}


//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+
// Invoked when device is mounted
void tud_mount_cb(void)
{
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}


// Invoked when device is unmounted
void tud_umount_cb(void)
{
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
}


// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_SUSPENDED), 0);
}


// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    if (tud_mounted()) {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
    } else {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
    }
}


//--------------------------------------------------------------------+
// USB CDC Callbacks
//--------------------------------------------------------------------+
// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void) itf;
    (void) rts;

    // TODO set some indicator
    if ( dtr ) {
        // Terminal connected
    } else {
        // Terminal disconnected
    }
}


// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf;
}

void tud_cdc_tx_complete_cb(uint8_t itf)
{
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());

    if(bInsideISR) {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(classTask, EVENT_CDC_TRANSMIT_DONE_BIT, eSetBits,
                        &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    } else {
        xTaskNotify(classTask, EVENT_CDC_TRANSMIT_DONE_BIT, eSetBits);
    }
}


//--------------------------------------------------------------------+
// USB Class Device
//   * calls CDC class
//   * calls MSC class
//--------------------------------------------------------------------+
static void usb_class_task(void * pxParam)
{
    (void)pxParam;
    uint32_t event = 0;
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];

    while(1) {
        if(pdTRUE == xTaskNotifyWait(0, 0xFFFFFFFF, &event, portMAX_DELAY)) {
            if(!tud_cdc_connected()) {
                /* not connected */
                txInProgress = false;
            }
            if((event & EVENT_CDC_AVAILABLE_BIT) != 0) {
                while (tud_cdc_available()) {
                    uint32_t nBytes = tud_cdc_available();
                    if(nBytes > sizeof(buf)) {
                        nBytes = sizeof(buf);
                    }
                    nBytes = tud_cdc_read(buf, nBytes);
                    CLI_Receive(buf, nBytes);
                }
            }
            if((event & EVENT_CDC_TRANSMIT_REQ_BIT) != 0) {
                if((txInProgress != true) && tud_cdc_connected()) {
                    uint32_t nBytes = xStreamBufferReceive(txStreamHandle, buf, sizeof(buf), 0);
                    if(nBytes > 0) {
                        txInProgress = true;
                        tud_cdc_write(buf, nBytes);
                        tud_cdc_write_flush();
                    }
                }
            }
            if((event & EVENT_CDC_TRANSMIT_DONE_BIT) != 0) {
                const size_t size = xStreamBufferReceive(txStreamHandle, buf, sizeof(buf), 0);
                if(size > 0)  {
                    /* More to transmit */
                    tud_cdc_write(buf, size);
                    tud_cdc_write_flush();
                } else {
                    /* Nothing to transmit */
                    txInProgress = false;
                }
            }
        }
    }
}


static void led_blinky_cb(TimerHandle_t xTimer)
{
    (void) xTimer;
    static bool led_state = false;

    board_led_write(led_state);
    led_state = !led_state;
}


size_t usb_device_cdc_transmit(uint8_t * pBuf, size_t count)
{
    size_t retval = 0;
    const bool bInsideISR = (pdTRUE == xPortIsInsideInterrupt());

    if(bInsideISR) {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        retval = xStreamBufferSendFromISR(txStreamHandle,
                                 pBuf,
                                 count,
                                 &higherPriorityTaskWoken);
        xTaskNotifyFromISR(classTask, EVENT_CDC_TRANSMIT_REQ_BIT, eSetBits,
                        &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    } else {
        retval = xStreamBufferSend(txStreamHandle,
                          pBuf,
                          count,
                          0);
        xTaskNotify(classTask, EVENT_CDC_TRANSMIT_REQ_BIT, eSetBits);
    }

    return retval;
}


bool usb_device_init_done(void)
{
    return bInit;
}
