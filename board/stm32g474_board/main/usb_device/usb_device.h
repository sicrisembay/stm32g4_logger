/*!
 ******************************************************************************
 * @file           : usb_device.h
 * @author         : Sicris Rey Embay
 ******************************************************************************
 */

#ifndef USB_DEVICE_H_
#define USB_DEVICE_H_

void usb_device_init(void);
bool usb_device_init_done(void);
size_t usb_device_cdc_transmit(uint8_t * buf, size_t count);

#endif /* USB_DEVICE_H_ */
