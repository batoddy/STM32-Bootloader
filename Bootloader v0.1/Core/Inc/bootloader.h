/*
 * bootloader.h
 *
 *  Created on: Sep 9, 2024
 *      Author: batuhan.odcikin
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

#include "stdint.h"
#include "main.h"
#include "usart.h"

#define CHUNK_SIZE 1024

#define UART_HANDLER huart3

#define APP1_FLASH_ADDR 0x08040000
#define APP2_FLASH_ADDR 0x08060000
#define APP3_FLASH_ADDR 0x08080000
#define APP4_FLASH_ADDR 0x080A0000

#define SOF_BYTE 0xAA
#define EOF_BYTE 0xEE

typedef enum AppAddr{
	APP1 = APP1_FLASH_ADDR,
	APP2= APP2_FLASH_ADDR,
	APP3= APP3_FLASH_ADDR,
	APP4= APP4_FLASH_ADDR
}AppAddr;

typedef enum FrameType{
	RESPONSE = 0, 	// SOF-TYPE-RESPONSE-EOF
	DATA,			// SOF-TYPE-PKGNO-LEN-DATA-CRC-EOF
	UPLOAD_INFO,	// SOF-TYPE-APP-FRAME_NBR-EOF
	END
}FrameType;

enum Response{
	WAIT_FOR_UPLOAD = 0x01,
	READY_TO_UPLOAD,
	RECEIVE_OK,
	RECEIVE_FULL,
	RECEIVE_ERR
};

// SOF-PackNo-Len-DATA-CRC32-EOF
typedef struct DataFrame{
	uint8_t sof;
	uint8_t type;
	uint16_t pkg_no;
	uint16_t len;
	uint8_t data[1024];
	uint32_t crc;
	uint8_t eof;
}DataFrame;
void runApplication(uint32_t);
uint8_t ready_to_upload();

void uart_mirror();
uint8_t recieve_bin_file(uint8_t data[][CHUNK_SIZE],uint8_t *app_no, uint16_t *chunk_cnt);
void send_response(uint8_t type);
HAL_StatusTypeDef upload_to_flash(uint8_t data[][CHUNK_SIZE], uint32_t app_addr,uint16_t chunk_cnt);

#endif /* INC_BOOTLOADER_H_ */
