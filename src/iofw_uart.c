#include "iofw_uart.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "iofw_registry.h"
#include "iofw_slots.h"

#include <stm32f4xx_hal.h>

#include "circ.h"


struct iofw_uart_device {
    UART_HandleTypeDef huart;
    int refs;
    int irqn;
    struct circ_buf *buf;
    HAL_LockTypeDef Lock;
};

struct iofw_uart_file_handle {
    struct iofw_uart_device *dev;
    int flags;
};

struct iofw_uart_device iofw_uart1 = {NULL};
struct iofw_uart_device iofw_uart2 = {NULL};
struct iofw_uart_device iofw_uart6 = {NULL};


void *iofw_uart_open(struct _reent *ptr, const char *file, int flags,  int mode, void *h);
int iofw_uart_read(struct _reent *ptr, void *handle, void *buf, size_t cnt);
int iofw_uart_write(struct _reent *ptr, void *handle, const void *buf, size_t cnt);
int iofw_uart_close(struct _reent *ptr, void *handle);
int iofw_uart_isatty(struct _reent *ptr, void *handle);

static void _iofw_uart_device_init(struct iofw_uart_device *handle, USART_TypeDef *usart)
{
    handle->huart.Instance = usart;
    handle->huart.Init.BaudRate = 115200;
    handle->huart.Init.WordLength = UART_WORDLENGTH_8B;
    handle->huart.Init.StopBits = UART_STOPBITS_1;
    handle->huart.Init.Parity = UART_PARITY_NONE;
    handle->huart.Init.Mode = UART_MODE_TX_RX;
    handle->huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    handle->huart.Init.OverSampling = UART_OVERSAMPLING_16;
}


void iofw_uart_register()
{
    struct iofw_uart_device *dev;
    struct iofw_regentry entry = {NULL};
    entry.open = &iofw_uart_open;
    entry.close = &iofw_uart_close;
    entry.read = &iofw_uart_read;
    entry.write = &iofw_uart_write;
    entry.isatty = &iofw_uart_isatty;

    entry.name = "/dev/ttyS1";
    dev = &iofw_uart1;
    dev->irqn = USART1_IRQn;
    _iofw_uart_device_init(dev, USART1);
    entry.handle = dev;
    iofw_add_registry(&entry);

    entry.name = "/dev/ttyS2";
    dev = &iofw_uart2;
    dev->irqn = USART2_IRQn;
    _iofw_uart_device_init(dev, USART2);
    entry.handle = dev;
    iofw_add_registry(&entry);

    entry.name = "/dev/ttyS6";
    dev = &iofw_uart6;
    dev->irqn = USART6_IRQn;
    _iofw_uart_device_init(dev, USART6);
    entry.handle = dev;
    iofw_add_registry(&entry);
}

static HAL_StatusTypeDef _iofw_uart_restart_reciever(struct iofw_uart_device *dev)
{
    return HAL_UART_Receive_IT(&dev->huart, &dev->buf->buf[dev->buf->head], 1);
}

void *iofw_uart_open(struct _reent *ptr, const char *file, int flags, int mode, void *h)
{
    int access_mode;
    struct iofw_uart_device *dev = h;
    struct iofw_uart_file_handle *fh = calloc(1, sizeof(struct iofw_uart_file_handle));
    if(!fh) {
        __errno_r(ptr) = ENOMEM;
        goto error;
    }
    fh->dev = dev;
    fh->flags = flags;
    fh->dev->refs++;

    if(HAL_UART_GetState(&fh->dev->huart) == HAL_UART_STATE_RESET) {
        HAL_UART_Init(&fh->dev->huart);
    }

    access_mode = (flags & O_ACCMODE)+1;
    if(access_mode & FREAD) {
        if(fh->dev->buf) {
            __errno_r(ptr) = EACCES;
            goto error;
        }

        fh->dev->buf = calloc(1, sizeof(*fh->dev->buf));
        if(!fh->dev->buf) {
            __errno_r(ptr) = ENOMEM;
            goto error;
        }
        if(!circ_alloc_buffer(fh->dev->buf, 256)) {
            __errno_r(ptr) = ENOMEM;
            goto error;
        }

        // enable IRQ
        HAL_NVIC_SetPriority(fh->dev->irqn, 0, 0);
        HAL_NVIC_EnableIRQ(fh->dev->irqn);

        if(_iofw_uart_restart_reciever(fh->dev) != HAL_OK) {
            __errno_r(ptr) = EBUSY;
            goto error;
        }
    }

    return fh;
error:
    iofw_uart_close(NULL, fh);
    return INVALID_HANDLE;
}

HAL_StatusTypeDef _iofw_uart_close_reader(struct iofw_uart_device *dev)
{
    __HAL_LOCK(dev);

    if(dev->buf) {
        free(dev->buf->buf);
        dev->buf->buf = NULL;
    }
    free(dev->buf);
    dev->buf = NULL;

    /* Disable RXNE, PE and ERR (Frame error, noise error, overrun error) interrupts */
    CLEAR_BIT(dev->huart.Instance->CR1, (USART_CR1_RXNEIE | USART_CR1_PEIE));
    CLEAR_BIT(dev->huart.Instance->CR3, USART_CR3_EIE);

    /* At end of Rx process, restore huart->RxState to Ready */
    dev->huart.RxState = HAL_UART_STATE_READY;
    __HAL_UNLOCK(dev);
    return HAL_OK;
}

int iofw_uart_close(struct _reent *ptr, void *handle)
{
    if(!handle)
        return;
    struct iofw_uart_file_handle *fh = handle;

    int access_mode = (fh->flags & O_ACCMODE)+1;
    if(access_mode & FREAD) {
        HAL_NVIC_DisableIRQ(fh->dev->irqn);

        while(_iofw_uart_close_reader(fh->dev) != HAL_OK);
    }

    fh->dev->refs--;
    if(fh->dev->refs == 0) {
        HAL_UART_DeInit(&fh->dev->huart);
    }

    free(fh);
    return 0;
}

int iofw_uart_read(struct _reent *ptr, void *handle, void *buf, size_t cnt)
{
    if(!handle)
        return;
    struct iofw_uart_file_handle *fh = handle;
    int access_mode = (fh->flags & O_ACCMODE)+1;
    if((access_mode & FREAD) == 0) {
        __errno_r(ptr) = EACCES;
        return -1;
    }

    int cnt_to_read = CIRC_CNT(*fh->dev->buf);
    if(cnt_to_read == 0) {
        __errno_r(ptr) = EAGAIN;
        return -1;
    }

    if(cnt_to_read > cnt) {
        cnt_to_read = cnt;
    }

    // Value to return
    cnt = cnt_to_read;

    char* cbuf = buf;
    while(cnt_to_read) {
        int buf_size = CIRC_CNT_TO_END(*fh->dev->buf);
        if(buf_size > cnt_to_read) {
            buf_size = cnt_to_read;
        }
        memcpy(cbuf, fh->dev->buf->buf+fh->dev->buf->tail, buf_size);
        cnt_to_read -= buf_size;
        CIRC_INCREMENT(*fh->dev->buf, tail, buf_size);
        cbuf += buf_size;
    }

    return cnt;
}

int iofw_uart_write(struct _reent *ptr, void *handle, const void *buf, size_t cnt)
{
    if(!handle)
        return;
    struct iofw_uart_file_handle *fh = handle;
    int access_mode = (fh->flags & O_ACCMODE)+1;
    if((access_mode & FWRITE) == 0) {
        __errno_r(ptr) = EACCES;
        return -1;
    }
    // The HAL_UART_Transmit accepts non-const buf!
    HAL_StatusTypeDef status = HAL_UART_Transmit(&fh->dev->huart, (uint8_t*)buf, cnt, 1000);
    switch(status) {
    case HAL_OK:
        return cnt;
    case HAL_ERROR:
        __errno_r(ptr) = EINVAL;
        return -1;
    case HAL_BUSY:
        __errno_r(ptr) = EBUSY;
        return -1;
    case HAL_TIMEOUT:
        __errno_r(ptr) = EIO;
        return -1;
    }
}

int iofw_uart_isatty(struct _reent *ptr, void *handle)
{
    return 1;
}

__attribute__((weak, alias ("iofw_uart_USART1_IRQHandler"))) void USART1_IRQHandler(void);
void iofw_uart_USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&iofw_uart1.huart);
}

__attribute__((weak, alias ("iofw_uart_USART2_IRQHandler"))) void USART2_IRQHandler(void);
void iofw_uart_USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&iofw_uart2.huart);
}

__attribute__((weak, alias ("iofw_uart_USART6_IRQHandler"))) void USART6_IRQHandler(void);
void iofw_uart_USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&iofw_uart6.huart);
}

HAL_StatusTypeDef _iofw_uart_HAL_UART_RxCpltCallback(struct iofw_uart_device *dev)
{
    // So far if device is locked, then it is destroying
    __HAL_LOCK(dev);

    if(!dev->buf)
        return; // Closed UART device

    // Increment head is there is a space, otherwise loose last data
    if(CIRC_SPACE(*dev->buf)) {
        CIRC_INCREMENT(*dev->buf, head, 1);
    }

    // wait for next symbol
    _iofw_uart_restart_reciever(dev);

    __HAL_UNLOCK(dev);
}

__attribute__((weak, alias ("iofw_uart_HAL_UART_RxCpltCallback"))) void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void iofw_uart_HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    struct iofw_uart_device *dev;
    if(&iofw_uart1.huart == huart) {
        dev = &iofw_uart1;
    }
    else if(&iofw_uart2.huart == huart) {
        dev = &iofw_uart2;
    }
    else if(&iofw_uart6.huart == huart) {
        dev = &iofw_uart6;
    }
    else {
        return; // Unsupported UART device
    }

    _iofw_uart_HAL_UART_RxCpltCallback(dev);

}
