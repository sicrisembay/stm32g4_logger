/*
 * qpc_hook.c
 *
 *  Created on: Mar 3, 2025
 *      Author: Sicris Rey Embay
 */

#include "FreeRTOS.h"
#include "task.h"
#include "qpc.h"
#include "qs_pkg.h"
#include "appPubList.h"
#include "lpuart.h"

Q_DEFINE_THIS_MODULE("qpc_hook.c")

/*
 * small size pool
 */
static QF_MPOOL_EL(QEvt) smallPoolSto[64];

/*
 * medium size pool
 */
typedef struct {
    QEvt super;
    uint8_t data[16];
} mediumPool;
static QF_MPOOL_EL(mediumPool) mediumPoolSto[32];

/*
 * large size pool
 */
typedef struct {
    QEvt super;
    uint8_t data[96];
} largePool;
static QF_MPOOL_EL(largePool) largePoolSto[32];

/*
 * Storage for Publish-Subscribe
 */
static QSubscrList subscrSto[MAX_PUB_SIG];

static uint8_t qsTxBuf[1024];
static uint8_t qsRxBuf[512];

void QPC_start(void)
{
    /*
     * Initialize QF framework
     */
    QF_init();

    /*
     * Initialize Event Pool
     * Note: QF can manage up to three event pools (e.g., small, medium, and large events).
     * An application may call this function up to three times to initialize up to three event
     * pools in QF.  The subsequent calls to QF_poolInit() function must be made with
     * progressively increasing values of the evtSize parameter.
     */
    QF_poolInit(smallPoolSto, sizeof(smallPoolSto), sizeof(smallPoolSto[0]));
    QF_poolInit(mediumPoolSto, sizeof(mediumPoolSto), sizeof(mediumPoolSto[0]));
    QF_poolInit(largePoolSto, sizeof(largePoolSto), sizeof(largePoolSto[0]));

    /*
     * Initialize Publish-Subscribe
     */
    QF_psInit(subscrSto, Q_DIM(subscrSto));

    QS_INIT((void *)0);

    /*
     * IMPORTANT NOTE:
     *   DO NOT CALL QF_run() as Scheduler has already started
     */
    QF_onStartup();
    // produce the QS_QF_RUN trace record
    QF_CRIT_STAT
    QF_CRIT_ENTRY();
    QS_BEGIN_PRE_(QS_QF_RUN, 0U)
    QS_END_PRE_()
    QF_CRIT_EXIT();
}


void QF_onStartup(void)
{
#ifdef Q_SPY
    QS_FILTER_ON(QS_QEP_STATE_ENTRY);
    // Enable QS Filter for all User-defined records
    QS_FILTER_ON(QS_UA_RECORDS);
#endif

}


void QF_onCleanup(void)
{
}

void Q_onAssert(char const *module, int loc)
{
    /* Disable Global interrupt */
    __asm volatile ("cpsid i");

    /* Put the System into Safe State */
    /// TODO

    /* Flush QSpy buffer with interrupt disabled! */
    while(1) {
        /// TODO
    }

    QS_ASSERTION(module, loc, (uint32_t)10000U); /* report assertion to QS */

    /* Flush QSpy buffer with interrupt disabled! */
    while(1) {
        /// TODO
    }

#ifndef NDEBUG
    /* for debugging, hang on in an endless loop... */
    for (;;) {
    }
#endif

    NVIC_SystemReset();
}


/************** QSPY Related section ***************************/
#define QSPY_WORKER_STACK_SIZE      (256)
#define QSPY_WORKER_TASK_PRIORITY   (1)
#define QSPY_RX_NOTIFY_TIMEOUT_MS   (2)
#define QSPY_RX_NOTIFY_TIMEOUT      (QSPY_RX_NOTIFY_TIMEOUT_MS / portTICK_PERIOD_MS)


static StaticTask_t xQspyWorkerTCB;
static StackType_t xQspyWorkerStackSto[QSPY_WORKER_STACK_SIZE];
static TaskHandle_t xQspyWorkerTaskHandle = NULL;


static void _QspyWorkerTask(void *pvParam)
{
    uint8_t const *pBlock;
    uint16_t txLen;
    uint8_t rxBuf[64];
    bool rxNew = false;

    (void)pvParam; /* Unused parameter */


    while(1) {
        rxNew = false;
        while(1) {
            int32_t readCount = LPUART_Receive(rxBuf, sizeof(rxBuf));
            if(readCount <= 0) {
                break;
            }
            rxNew = true;
            for(uint32_t i = 0; i < readCount; i++) {
                QS_RX_PUT(rxBuf[i]);
            }
        }

        if(rxNew) {
            QS_rxParse();
        }

        txLen = 64;
        taskENTER_CRITICAL();
        pBlock = QS_getBlock(&txLen);
        taskEXIT_CRITICAL();
        if(txLen > 0) {
            Q_ASSERT(txLen == LPUART_Send(pBlock, txLen));
        }
    }
}


uint8_t QS_onStartup(void const *arg)
{
    (void)arg;  // unused parameter
    QS_initBuf(qsTxBuf, sizeof(qsTxBuf));
    QS_rxInitBuf(qsRxBuf, sizeof(qsRxBuf));

    /* Create worker task */
    if(NULL == xQspyWorkerTaskHandle) {
        xQspyWorkerTaskHandle = xTaskCreateStatic(
                _QspyWorkerTask,
                "QSpyWorker",
                QSPY_WORKER_STACK_SIZE,
                (void *)0,
                QSPY_WORKER_TASK_PRIORITY,
                xQspyWorkerStackSto,
                &xQspyWorkerTCB
                );
    }

    return (uint8_t)1; /* return success */
}


void QS_onCleanup(void)
{
    /// TODO
}


QSTimeCtr QS_onGetTime(void)
{
    /* Use freeRTOS tick count */
    return(xTaskGetTickCount());
}


void QS_onFlush(void)
{
    /// TODO: Implement QS buffer flushing to HW peripheral
}


void QS_onReset(void)
{
    NVIC_SystemReset();
}


void QS_onCommand(uint8_t cmdId,
                  uint32_t param1, uint32_t param2, uint32_t param3)
{
    (void)cmdId;
    (void)param1;
    (void)param2;
    (void)param3;
}

